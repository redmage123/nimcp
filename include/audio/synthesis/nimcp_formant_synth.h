/**
 * @file nimcp_formant_synth.h
 * @brief Biological formant synthesizer — Athena's voice
 *
 * Converts phoneme sequences from the SNN speech bridge into audio waveforms
 * using a Klatt-style source-filter model. No external TTS, no deep learning.
 * Pure signal processing driven by the brain's neural output.
 *
 * TRAINABLE: Formant parameters are stored as a learned weight matrix.
 * During training, audio reconstruction loss adjusts these weights, so
 * Athena's voice improves and adapts to the speech she hears.
 *
 * EMOTIONAL: Prosody (pitch, rate, intensity, voice quality) is modulated
 * by the brain's emotional_state_t via Broca's emotional_prosody system.
 *
 * MULTILINGUAL: Supports extended phoneme sets beyond English IPA via
 * the universal phoneme inventory (phoneme_ext_t).
 *
 * Architecture:
 *   phoneme_t[] → synth_target_t[] → excitation + resonators → PCM audio
 *                                                              → viseme_event_t[]
 */

#ifndef NIMCP_FORMANT_SYNTH_H
#define NIMCP_FORMANT_SYNTH_H

#include <stdint.h>
#include <stdbool.h>

/* Forward declarations to avoid circular includes */
typedef struct snn_speech_bridge_s snn_speech_bridge_t;
struct emotional_prosody;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define SYNTH_MAX_FORMANTS        5     /* F1-F5 resonators */
#define SYNTH_MAX_PHONEMES_PER_WORD 32  /* Max phonemes in a single word */
#define SYNTH_DEFAULT_SAMPLE_RATE 16000 /* 16kHz output */
#define SYNTH_MAX_SAMPLES_PER_WORD 32000 /* 2 seconds max per word */
#define SYNTH_COARTICULATION_MS   25.0f /* Formant interpolation window */
#define SYNTH_GLOTTAL_OQ_DEFAULT  0.6f  /* Rosenberg open quotient */

/* Learned parameter dimensions */
#define SYNTH_VOWEL_PARAMS        5     /* F1,F2,F3,bandwidth,duration per vowel */
#define SYNTH_CONSONANT_PARAMS    6     /* noise_freq,noise_bw,burst_amp,burst_dur,duration,voiced */
#define SYNTH_PROSODY_PARAMS      4     /* pitch_scale,rate_scale,intensity_scale,breathiness */

/* Extended phoneme capacity for multilingual */
#define PHONEME_EXT_MAX          128    /* Universal phoneme inventory size */

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Language/accent identifier
 */
typedef enum {
    SYNTH_LANG_ENGLISH_US = 0,
    SYNTH_LANG_ENGLISH_UK,
    SYNTH_LANG_ENGLISH_AU,
    SYNTH_LANG_ENGLISH_SCOTTISH,
    SYNTH_LANG_ENGLISH_IRISH,
    SYNTH_LANG_FRENCH,
    SYNTH_LANG_SPANISH,
    SYNTH_LANG_GERMAN,
    SYNTH_LANG_ITALIAN,
    SYNTH_LANG_PORTUGUESE,
    SYNTH_LANG_MANDARIN,
    SYNTH_LANG_JAPANESE,
    SYNTH_LANG_KOREAN,
    SYNTH_LANG_ARABIC,
    SYNTH_LANG_HINDI,
    SYNTH_LANG_RUSSIAN,
    SYNTH_LANG_SWAHILI,
    SYNTH_LANG_COUNT
} synth_language_t;

/**
 * @brief Viseme categories (matches lip_reading.h VISEME_* for avatar)
 */
typedef enum {
    SYNTH_VISEME_BILABIAL = 0,      /* /p/, /b/, /m/ — lips closed */
    SYNTH_VISEME_LABIODENTAL,       /* /f/, /v/ — teeth on lip */
    SYNTH_VISEME_DENTAL,            /* /th/, /dh/ — tongue between teeth */
    SYNTH_VISEME_ALVEOLAR,          /* /t/, /d/, /n/, /l/, /s/, /z/ */
    SYNTH_VISEME_VELAR,             /* /k/, /g/, /ng/ */
    SYNTH_VISEME_ROUNDED_CLOSE,     /* /u/, /o/ — lips rounded, small */
    SYNTH_VISEME_ROUNDED_OPEN,      /* /aw/ — lips rounded, wide */
    SYNTH_VISEME_UNROUNDED_CLOSE,   /* /i/ — lips spread */
    SYNTH_VISEME_UNROUNDED_MID,     /* /e/, /uh/ — neutral */
    SYNTH_VISEME_UNROUNDED_OPEN,    /* /a/ — mouth wide */
    SYNTH_VISEME_SILENCE,           /* Mouth closed */
    SYNTH_VISEME_COUNT
} synth_viseme_t;

/**
 * @brief Voice quality (maps to emotional_prosody voice_quality_t)
 */
typedef enum {
    SYNTH_VOICE_NORMAL = 0,
    SYNTH_VOICE_BREATHY,    /* High aspiration, high Oq — sad/intimate */
    SYNTH_VOICE_CREAKY,     /* Low F0, irregular pulses — tired/bored */
    SYNTH_VOICE_TENSE,      /* Low Oq, low aspiration — angry/stressed */
    SYNTH_VOICE_LAX,        /* High Oq, relaxed — calm/content */
    SYNTH_VOICE_COUNT
} synth_voice_quality_t;

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief Second-order resonant filter (formant or anti-formant)
 */
typedef struct {
    float freq_hz;          /* Center frequency */
    float bandwidth_hz;     /* 3-dB bandwidth */
    /* Biquad coefficients (recomputed when freq/bw change) */
    float a1, a2, b0;
    /* Filter state */
    float y_1, y_2;        /* Previous outputs */
} formant_resonator_t;

/**
 * @brief Anti-formant (nasal zero) — two-zero filter
 */
typedef struct {
    float freq_hz;
    float bandwidth_hz;
    float a1, a2, b0, b1, b2;
    float x_1, x_2, y_1, y_2;
} antiformant_t;

/**
 * @brief Synthesis target for a single phoneme
 *
 * Produced by phoneme_to_target(), consumed by the synthesis loop.
 * For trainable voice, these values come from the learned weight matrix
 * rather than a hardcoded table.
 */
typedef struct {
    uint32_t phoneme_id;        /* Source phoneme index */
    float duration_ms;          /* Duration in milliseconds */
    float f0_hz;                /* Fundamental frequency (pitch) */
    float intensity;            /* Amplitude [0, 1] */
    float stress;               /* Stress level [0, 1] */

    /* Formant targets: F1-F5 frequency + bandwidth */
    float formant_freq[SYNTH_MAX_FORMANTS];
    float formant_bw[SYNTH_MAX_FORMANTS];

    /* Excitation parameters */
    bool voiced;                /* Glottal pulse (true) vs noise (false) */
    float noise_freq_hz;        /* Center freq for fricative noise */
    float noise_bw_hz;          /* Bandwidth for noise shaping */
    float burst_amplitude;      /* Stop burst amplitude [0, 1] */
    float burst_duration_ms;    /* Stop burst duration */
    float aspiration;           /* Aspiration/breathiness [0, 1] */

    /* Nasal */
    bool nasal;                 /* Enable nasal anti-formant */
    float nasal_zero_hz;        /* Anti-formant frequency */
    float nasal_pole_hz;        /* Nasal resonance frequency */

    /* Viseme for avatar */
    synth_viseme_t viseme;
    float mouth_aperture;       /* [0, 1] */
    float lip_rounding;         /* [0, 1] */
    float jaw_open;             /* [0, 1] */
} synth_target_t;

/**
 * @brief Timed viseme event for avatar lip-sync
 */
typedef struct {
    synth_viseme_t viseme;      /* Which mouth shape */
    float onset_ms;             /* Start time relative to utterance start */
    float duration_ms;          /* How long to hold */
    float mouth_aperture;       /* Vertical opening [0, 1] */
    float lip_rounding;         /* Horizontal rounding [0, 1] */
    float jaw_open;             /* Jaw displacement [0, 1] */
    float blend_in_ms;          /* Transition in from previous */
    float blend_out_ms;         /* Transition out to next */
} viseme_event_t;

/**
 * @brief Synthesizer configuration
 */
typedef struct {
    uint32_t sample_rate;               /* Output sample rate (16000) */
    float default_f0_hz;                /* Base pitch (120=male, 220=female) */
    float glottal_open_quotient;        /* Rosenberg Oq (0.6) */
    float aspiration_baseline;          /* Breathiness floor (0.03) */
    float coarticulation_ms;            /* Formant interpolation window */
    bool enable_prosody_modulation;     /* Use emotional prosody */
    bool enable_trainable_params;       /* Use learned weights vs hardcoded */
    synth_language_t language;          /* Current language/accent */
} formant_synth_config_t;

/**
 * @brief Learned voice parameters (trainable via backprop)
 *
 * Instead of hardcoded formant tables, these are weight matrices that
 * get adjusted during training. Each language/accent has its own set.
 * The brain learns to map phoneme IDs to formant targets.
 */
typedef struct {
    /* Vowel formant targets: [12 vowels × 5 params] */
    /* Params: F1_hz, F2_hz, F3_hz, bandwidth_hz, duration_ms */
    float vowel_params[12][SYNTH_VOWEL_PARAMS];

    /* Consonant targets: [26 consonants × 6 params] */
    /* Params: noise_freq, noise_bw, burst_amp, burst_dur, duration, voiced_mix */
    float consonant_params[26][SYNTH_CONSONANT_PARAMS];

    /* Prosody modifiers per emotion: [EMOTION_COUNT × 4] */
    /* Params: pitch_scale, rate_scale, intensity_scale, breathiness */
    float emotion_prosody[8][SYNTH_PROSODY_PARAMS];

    /* Global voice characteristics */
    float base_f0_hz;           /* Learned base pitch */
    float f0_variance;          /* Natural pitch jitter */
    float breathiness;          /* Global aspiration level */
    float nasality;             /* Global nasal resonance */

    /* Training state */
    uint64_t training_steps;    /* Number of parameter updates */
    float learning_rate;        /* Current LR for voice params */
    bool frozen;                /* Freeze params (inference only) */
} voice_params_t;

/**
 * @brief Combined synthesis result (audio + visemes)
 */
typedef struct {
    float* audio_samples;       /* PCM audio at sample_rate */
    uint32_t num_samples;       /* Number of audio samples */
    uint32_t sample_rate;       /* Sample rate (16000) */

    viseme_event_t* visemes;    /* Timed viseme stream */
    uint32_t num_visemes;       /* Number of viseme events */

    float duration_ms;          /* Total utterance duration */
} speech_output_t;

/**
 * @brief Formant synthesizer instance
 */
typedef struct formant_synth {
    formant_synth_config_t config;

    /* Resonator cascade: F1-F5 */
    formant_resonator_t resonators[SYNTH_MAX_FORMANTS];
    antiformant_t nasal_zero;

    /* Glottal source state */
    float glottal_phase;        /* 0-1 within current pitch period */
    uint32_t noise_seed;        /* xorshift32 PRNG state */

    /* Coarticulation interpolation */
    synth_target_t current_target;
    synth_target_t next_target;
    float interp_progress;      /* 0-1 between targets */
    float interp_rate;          /* Progress per sample */

    /* Output buffer */
    float* output_buffer;
    uint32_t output_capacity;
    uint32_t output_length;

    /* Viseme stream buffer */
    viseme_event_t* viseme_buffer;
    uint32_t viseme_capacity;
    uint32_t viseme_count;

    /* Learned voice parameters (per-language) */
    voice_params_t voice[SYNTH_LANG_COUNT];
    synth_language_t active_language;

    /* Emotional prosody connection */
    struct emotional_prosody* prosody;  /* Optional: NULL = neutral */
    float current_arousal;
    float current_valence;
    synth_voice_quality_t current_voice_quality;

    /* Statistics */
    uint64_t samples_generated;
    uint64_t phonemes_synthesized;
    uint64_t words_synthesized;
} formant_synth_t;

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

/** Get default configuration */
formant_synth_config_t formant_synth_default_config(void);

/** Create a synthesizer instance */
formant_synth_t* formant_synth_create(const formant_synth_config_t* config);

/** Destroy and free */
void formant_synth_destroy(formant_synth_t* synth);

/** Initialize voice parameters to default (English US) */
void formant_synth_init_voice_defaults(formant_synth_t* synth);

/*=============================================================================
 * EMOTIONAL PROSODY INTEGRATION
 *===========================================================================*/

/** Connect to brain's emotional prosody system */
void formant_synth_set_prosody(formant_synth_t* synth,
                                struct emotional_prosody* prosody);

/** Update emotional state (called before synthesis) */
void formant_synth_set_emotion(formant_synth_t* synth,
                                float arousal, float valence,
                                int primary_emotion);

/*=============================================================================
 * LANGUAGE / ACCENT
 *===========================================================================*/

/** Switch active language/accent */
void formant_synth_set_language(formant_synth_t* synth, synth_language_t lang);

/** Get current language */
synth_language_t formant_synth_get_language(const formant_synth_t* synth);

/*=============================================================================
 * CORE SYNTHESIS
 *===========================================================================*/

/**
 * @brief Convert phoneme ID to synthesis target using learned params
 *
 * Uses voice_params_t (trainable) rather than hardcoded tables.
 * Emotional state modulates pitch, rate, intensity, voice quality.
 */
int formant_synth_phoneme_to_target(formant_synth_t* synth,
                                     uint32_t phoneme_id,
                                     float stress,
                                     synth_target_t* target_out);

/**
 * @brief Synthesize a single phoneme to audio samples
 *
 * Generates PCM samples for one phoneme target. Handles coarticulation
 * by interpolating from current resonator state to the target.
 */
int formant_synth_render_phoneme(formant_synth_t* synth,
                                  const synth_target_t* target,
                                  float* samples_out,
                                  uint32_t* num_samples_out,
                                  uint32_t max_samples);

/**
 * @brief Synthesize a phoneme sequence into audio + visemes
 *
 * High-level API: takes a phoneme ID array and produces synchronized
 * audio waveform and viseme event stream.
 */
int formant_synth_speak_phonemes(formant_synth_t* synth,
                                  const uint32_t* phoneme_ids,
                                  uint32_t num_phonemes,
                                  speech_output_t* output);

/**
 * @brief Synthesize from SNN speech bridge word production
 *
 * Integration point: calls bridge's produce_word() to get phonemes,
 * then synthesizes audio + visemes.
 */
int formant_synth_speak_word(formant_synth_t* synth,
                              snn_speech_bridge_t* bridge,
                              uint32_t word_pop_index,
                              speech_output_t* output);

/** Free speech_output_t contents (not the struct itself) */
void speech_output_free(speech_output_t* output);

/*=============================================================================
 * TRAINING (voice parameter learning)
 *===========================================================================*/

/**
 * @brief Update voice parameters from audio reconstruction loss
 *
 * Called during training when Athena hears speech. Compares her
 * synthesized output against the real audio and adjusts formant
 * parameters via gradient descent.
 *
 * @param synth       Synthesizer instance
 * @param phoneme_ids Phoneme sequence that was synthesized
 * @param num_phonemes Number of phonemes
 * @param target_audio Real audio samples (what it should sound like)
 * @param num_target_samples Number of target samples
 * @return 0 on success, -1 on error
 */
int formant_synth_train_step(formant_synth_t* synth,
                              const uint32_t* phoneme_ids,
                              uint32_t num_phonemes,
                              const float* target_audio,
                              uint32_t num_target_samples);

/** Freeze/unfreeze voice parameters */
void formant_synth_freeze(formant_synth_t* synth, bool frozen);

/** Get training step count */
uint64_t formant_synth_get_training_steps(const formant_synth_t* synth);

/*=============================================================================
 * SERIALIZATION
 *===========================================================================*/

/** Save voice parameters to file */
int formant_synth_save_voice(const formant_synth_t* synth, const char* path);

/** Load voice parameters from file */
int formant_synth_load_voice(formant_synth_t* synth, const char* path);

/*=============================================================================
 * SIGNAL PROCESSING PRIMITIVES (used internally, exposed for testing)
 *===========================================================================*/

/** Compute biquad resonator coefficients from freq/bandwidth/sample_rate */
void resonator_compute_coefficients(formant_resonator_t* res,
                                     uint32_t sample_rate);

/** Process one sample through a resonator */
float resonator_tick(formant_resonator_t* res, float input);

/** Generate one glottal pulse sample (Rosenberg model) */
float glottal_pulse(float phase, float open_quotient);

/** Generate band-limited noise sample */
float noise_sample(uint32_t* seed);

/** xorshift32 PRNG */
uint32_t xorshift32(uint32_t* state);

/** Map phoneme ID → viseme for avatar */
synth_viseme_t phoneme_to_viseme(uint32_t phoneme_id);

#endif /* NIMCP_FORMANT_SYNTH_H */
