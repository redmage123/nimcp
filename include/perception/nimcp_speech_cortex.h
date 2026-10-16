/**
 * @file nimcp_speech_cortex.h
 * @brief Biologically-inspired speech and language processing system
 *
 * WHAT: Speech cortex modeling Wernicke's area (comprehension) and Broca's area (production)
 * WHY:  Enable linguistic processing and phonological working memory in NIMCP
 * HOW:  Phoneme recognition + formant analysis + prosody extraction + lexical access
 *
 * BIOLOGICAL CONTEXT:
 * Models the human speech processing hierarchy:
 * - Primary Auditory Cortex (A1): Spectral analysis (handled by audio cortex)
 * - Superior Temporal Gyrus (STG): Phoneme categorization, syllable parsing
 * - Wernicke's Area (BA 22): Word recognition, semantic comprehension
 * - Broca's Area (BA 44/45): Speech production, articulatory planning
 * - Angular Gyrus: Grapheme-phoneme conversion (reading)
 *
 * DESIGN PRINCIPLES:
 * - Compositional: Phonemes → Syllables → Words → Sentences
 * - Bidirectional: Bottom-up (acoustic) + Top-down (semantic) processing
 * - Temporal: Working memory for phonological loop (Baddeley model)
 * - Categorical: Phoneme categories with tolerance to speaker variation
 *
 * REFERENCES:
 * - Hickok & Poeppel (2007) "The cortical organization of speech processing"
 * - Wernicke (1874) "The aphasic symptom-complex"
 * - Broca (1861) "Remarks on the seat of the faculty of articulated language"
 * - Peterson & Barney (1952) "Control methods for vowel formant measurement"
 * - Liberman et al. (1967) "Perception of the speech code" (motor theory)
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 2.7 (Phase 8.8)
 */

#ifndef NIMCP_SPEECH_CORTEX_H
#define NIMCP_SPEECH_CORTEX_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/error/nimcp_error_codes.h"

/* Bio-async communication system */
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration for brain integration
typedef struct brain_struct* brain_t;

//=============================================================================
// Configuration Constants
//=============================================================================

/** Number of phonemes in IPA subset (English: ~44, Full IPA: ~160) */
#define SPEECH_NUM_PHONEMES 44

/** Number of formant frequencies tracked (F1, F2, F3, F4) */
#define SPEECH_NUM_FORMANTS 4

/** Maximum phonological working memory span (7±2 items, Miller 1956) */
#define SPEECH_MAX_PHONOLOGICAL_BUFFER 9

/** Maximum lexicon size (vocabulary) */
#define SPEECH_MAX_LEXICON_SIZE 10000

/** Phoneme frame size (typically 10-25ms for speech) */
#define SPEECH_PHONEME_FRAME_MS 20

/** Maximum duration for phoneme integration (ms) */
#define SPEECH_MAX_PHONEME_DURATION_MS 300

//=============================================================================
// Phoneme Representation
//=============================================================================

/**
 * @brief Phoneme categories (IPA subset for English)
 *
 * BIOLOGICAL: Phonemes are abstract perceptual categories, not acoustic templates.
 * The brain tolerates speaker variation via categorical perception (Liberman et al. 1957).
 */
typedef enum {
    // === VOWELS ===
    PHONEME_IY,  // "ee" in "beet" (high front)
    PHONEME_IH,  // "i" in "bit"
    PHONEME_EY,  // "a" in "bait" (mid front)
    PHONEME_EH,  // "e" in "bet"
    PHONEME_AE,  // "a" in "bat" (low front)
    PHONEME_AA,  // "a" in "father" (low back)
    PHONEME_AO,  // "o" in "caught"
    PHONEME_OW,  // "o" in "boat" (mid back)
    PHONEME_UH,  // "oo" in "book"
    PHONEME_UW,  // "oo" in "boot" (high back)
    PHONEME_AH,  // "u" in "but" (central)
    PHONEME_ER,  // "er" in "bird" (r-colored)

    // === CONSONANTS: STOPS ===
    PHONEME_P,   // "p" in "pat" (voiceless bilabial)
    PHONEME_B,   // "b" in "bat" (voiced bilabial)
    PHONEME_T,   // "t" in "tap" (voiceless alveolar)
    PHONEME_D,   // "d" in "dap" (voiced alveolar)
    PHONEME_K,   // "k" in "cap" (voiceless velar)
    PHONEME_G,   // "g" in "gap" (voiced velar)

    // === CONSONANTS: FRICATIVES ===
    PHONEME_F,   // "f" in "fat" (voiceless labiodental)
    PHONEME_V,   // "v" in "vat" (voiced labiodental)
    PHONEME_TH,  // "th" in "thin" (voiceless dental)
    PHONEME_DH,  // "th" in "this" (voiced dental)
    PHONEME_S,   // "s" in "sat" (voiceless alveolar)
    PHONEME_Z,   // "z" in "zap" (voiced alveolar)
    PHONEME_SH,  // "sh" in "ship" (voiceless postalveolar)
    PHONEME_ZH,  // "s" in "measure" (voiced postalveolar)
    PHONEME_H,   // "h" in "hat" (glottal)

    // === CONSONANTS: NASALS ===
    PHONEME_M,   // "m" in "mat" (bilabial)
    PHONEME_N,   // "n" in "nat" (alveolar)
    PHONEME_NG,  // "ng" in "sing" (velar)

    // === CONSONANTS: APPROXIMANTS ===
    PHONEME_L,   // "l" in "lap" (lateral)
    PHONEME_R,   // "r" in "rap" (rhotic)
    PHONEME_W,   // "w" in "wap" (labio-velar)
    PHONEME_Y,   // "y" in "yap" (palatal)

    // === AFFRICATES ===
    PHONEME_CH,  // "ch" in "chap" (voiceless postalveolar)
    PHONEME_JH,  // "j" in "jap" (voiced postalveolar)

    // === SPECIAL ===
    PHONEME_SILENCE, // No speech (pause, breath)
    PHONEME_UNKNOWN, // Unrecognized sound

    PHONEME_COUNT
} phoneme_t;

/**
 * @brief Phoneme features (articulatory + acoustic)
 *
 * WHAT: Distinctive features for phoneme categorization
 * WHY:  Represent phonemes as feature bundles (Jakobson, Chomsky & Halle)
 * HOW:  Binary features for articulation place/manner + continuous acoustics
 */
typedef struct {
    // === CATEGORICAL FEATURES (Articulatory) ===
    bool is_vowel;              ///< Vowel vs consonant
    bool is_voiced;             ///< Vocal fold vibration
    bool is_nasal;              ///< Nasal airflow (m, n, ng)
    bool is_stop;               ///< Complete airflow blockage (p, t, k)
    bool is_fricative;          ///< Turbulent airflow (f, s, sh)
    bool is_liquid;             ///< Lateral/rhotic (l, r)

    // === ACOUSTIC FEATURES (Continuous) ===
    float formant_f1;           ///< First formant (Hz, vowel height)
    float formant_f2;           ///< Second formant (Hz, vowel frontness)
    float formant_f3;           ///< Third formant (Hz, rhoticity)
    float formant_f4;           ///< Fourth formant (Hz, rare)
    float duration_ms;          ///< Phoneme duration (ms)
    float pitch_hz;             ///< Fundamental frequency (Hz)
    float intensity_db;         ///< Loudness (dB SPL)
    float spectral_centroid;    ///< Center of gravity (Hz)
    float zero_crossing_rate;   ///< Voicing cue (crossings/sec)

    // === PROSODIC FEATURES ===
    float stress_level;         ///< Syllable stress [0,1]
    float tone_contour;         ///< Pitch contour (for tonal languages)

} phoneme_features_t;

/**
 * @brief Recognized phoneme with timing and confidence
 */
typedef struct {
    phoneme_t phoneme;          ///< Phoneme category
    phoneme_features_t features;///< Acoustic/articulatory features
    float confidence;           ///< Recognition confidence [0,1]
    uint64_t onset_time_ms;     ///< Start time (ms)
    uint64_t offset_time_ms;    ///< End time (ms)

    // Positional encoding (Phase 10.13)
    float* position_embedding;  ///< Position embedding vector (pe_embedding_dim floats)
    uint32_t sequence_position; ///< Position in phoneme sequence
} phoneme_event_t;

//=============================================================================
// Speech Cortex Configuration
//=============================================================================

/**
 * @brief Speech cortex configuration
 */
typedef struct {
    uint32_t sample_rate;           ///< Audio sample rate (Hz)
    uint32_t frame_size_ms;         ///< Analysis frame size (ms)
    uint32_t hop_size_ms;           ///< Frame hop size (ms)
    uint32_t num_phonemes;          ///< Phoneme vocabulary size
    uint32_t num_formants;          ///< Number of formants to track
    uint32_t phonological_buffer_size; ///< Working memory capacity
    uint32_t lexicon_size;          ///< Vocabulary size
    uint32_t feature_dim;           ///< Output feature dimensionality
    bool enable_wernicke;           ///< Enable comprehension (word recognition)
    bool enable_broca;              ///< Enable production (articulatory features)
    bool enable_prosody;            ///< Enable prosodic analysis (pitch, stress)
    bool enable_memory;             ///< Enable phonological working memory

    // NIMCP 2.7 Phase 8.5: Fractal Topology Integration
    bool enable_fractal_topology;   /**< Enable scale-free topology within STG */
    float hub_ratio;                /**< Fraction of hub neurons, default: 0.15 */
    float power_law_gamma;          /**< Power-law exponent, default: -2.1 */
    uint32_t internal_neurons;      /**< Internal neurons for recurrent processing */

    // Bio-async communication
    bool enable_bio_async;          /**< Enable bio-async messaging */

    // Second messenger cascades (Phase 10.12)
    bool enable_second_messengers;  /**< Enable second messenger cascades */

    // Positional encoding (Phase 10.13)
    bool enable_positional_encoding; /**< Enable positional encoding for phoneme sequences */
    uint32_t pe_embedding_dim;       /**< Positional encoding embedding dimension */
    uint32_t pe_phoneme_type;        /**< PE type for phoneme sequences (0=sinusoidal, 1=learned) */
    uint32_t pe_buffer_type;         /**< PE type for phonological buffer (0=sinusoidal, 1=learned) */
} speech_cortex_config_t;

/**
 * @brief Speech cortex instance (opaque)
 */
typedef struct speech_cortex speech_cortex_t;

/**
 * @brief Speech cortex statistics
 */
typedef struct {
    uint64_t frames_processed;      ///< Total frames processed
    uint32_t phonemes_detected;     ///< Total phonemes detected
    uint32_t words_recognized;      ///< Total words recognized (if Wernicke enabled)
    float avg_processing_time_ms;   ///< Avg processing time (ms)
    float phoneme_accuracy;         ///< Phoneme recognition accuracy [0,1]
} speech_cortex_stats_t;

//=============================================================================
// Core API
//=============================================================================

/**
 * @brief Create speech cortex instance
 *
 * WHAT: Initialize speech processing pipeline
 * WHY:  Enable linguistic processing in NIMCP brain
 * HOW:  Allocate memory for phoneme recognition, formant tracking, and lexicon
 *
 * @param config Configuration parameters
 * @return Speech cortex instance or NULL on failure
 */
speech_cortex_t* speech_cortex_create(const speech_cortex_config_t* config);

/**
 * @brief Destroy speech cortex instance
 * @param cortex Speech cortex to destroy
 */
void speech_cortex_destroy(speech_cortex_t* cortex);

/**
 * @brief Process audio frame and extract speech features
 *
 * WHAT: Extract phonemes, formants, and prosody from audio
 * WHY:  Convert raw audio to linguistic representations
 * HOW:  Formant extraction → Phoneme classification → Feature vector
 *
 * COMPLEXITY: O(N log N) for FFT + O(P) for phoneme classification
 *
 * @param cortex Speech cortex instance
 * @param audio_data Raw audio samples (float32, mono)
 * @param num_samples Number of samples
 * @param features Output feature vector (must be pre-allocated)
 * @return true on success, false on failure
 */
bool speech_cortex_process(
    speech_cortex_t* cortex,
    const float* audio_data,
    uint32_t num_samples,
    float* features
);

/**
 * @brief Get speech cortex statistics
 * @param cortex Speech cortex instance
 * @param stats Output statistics structure
 * @return true on success, false on failure
 */
bool speech_cortex_get_stats(
    const speech_cortex_t* cortex,
    speech_cortex_stats_t* stats
);

//=============================================================================
// Phoneme Recognition (Superior Temporal Gyrus)
//=============================================================================

/**
 * @brief Detect phonemes in audio frame
 *
 * WHAT: Extract phoneme sequence from audio
 * WHY:  Enable categorical perception of speech sounds
 * HOW:  Formant analysis → Feature extraction → Phoneme classification
 *
 * BIOLOGICAL: STG contains phoneme-selective neurons (Chang et al. 2010)
 *
 * @param cortex Speech cortex instance
 * @param audio_data Audio samples
 * @param num_samples Number of samples
 * @param phonemes Output phoneme sequence
 * @param max_phonemes Maximum phonemes to detect
 * @param num_detected Number of phonemes actually detected
 * @return true on success, false on failure
 */
bool speech_cortex_detect_phonemes(
    speech_cortex_t* cortex,
    const float* audio_data,
    uint32_t num_samples,
    phoneme_event_t* phonemes,
    uint32_t max_phonemes,
    uint32_t* num_detected
);

/**
 * @brief Extract formant frequencies (F1, F2, F3, F4)
 *
 * WHAT: Track vocal tract resonances
 * WHY:  Formants uniquely identify vowels (Peterson & Barney 1952)
 * HOW:  LPC analysis + peak picking in spectrum
 *
 * @param cortex Speech cortex instance
 * @param audio_data Audio samples
 * @param num_samples Number of samples
 * @param formants Output formant frequencies (Hz)
 * @param num_formants Number of formants (typically 4)
 * @return true on success, false on failure
 */
bool speech_cortex_extract_formants(
    speech_cortex_t* cortex,
    const float* audio_data,
    uint32_t num_samples,
    float* formants,
    uint32_t num_formants
);

/**
 * @brief Classify vowel from formant frequencies
 *
 * WHAT: Map (F1, F2) → Vowel phoneme
 * WHY:  F1 correlates with tongue height, F2 with frontness/backness
 * HOW:  Euclidean distance in formant space
 *
 * @param f1 First formant (Hz)
 * @param f2 Second formant (Hz)
 * @return Vowel phoneme (PHONEME_IY, PHONEME_AA, etc.)
 */
phoneme_t speech_cortex_classify_vowel(float f1, float f2);

/**
 * @brief Classify consonant from spectral features
 *
 * WHAT: Map spectral envelope → Consonant phoneme
 * WHY:  Consonants characterized by spectral shape, not formants
 * HOW:  Spectral centroid + zero-crossing rate + duration
 *
 * @param cortex Speech cortex instance
 * @param audio_data Audio samples
 * @param num_samples Number of samples
 * @return Consonant phoneme (PHONEME_P, PHONEME_S, etc.)
 */
phoneme_t speech_cortex_classify_consonant(
    speech_cortex_t* cortex,
    const float* audio_data,
    uint32_t num_samples
);

//=============================================================================
// Prosody Analysis
//=============================================================================

/**
 * @brief Extract prosodic features (pitch, stress, rhythm)
 *
 * WHAT: Extract suprasegmental features for intonation and emphasis
 * WHY:  Prosody conveys emotion, focus, and syntactic structure
 * HOW:  Pitch tracking (autocorrelation) + intensity contour
 *
 * BIOLOGICAL: Right hemisphere specialized for prosody (Ross 1981)
 *
 * @param cortex Speech cortex instance
 * @param audio_data Audio samples
 * @param num_samples Number of samples
 * @param pitch_hz Output fundamental frequency (Hz)
 * @param stress_level Output stress level [0,1]
 * @return true on success, false on failure
 */
bool speech_cortex_extract_prosody(
    speech_cortex_t* cortex,
    const float* audio_data,
    uint32_t num_samples,
    float* pitch_hz,
    float* stress_level
);

//=============================================================================
// Lexical Access (Wernicke's Area)
//=============================================================================

/**
 * @brief Recognize word from phoneme sequence
 *
 * WHAT: Map phoneme sequence → Word (lexical access)
 * WHY:  Enable word-level comprehension
 * HOW:  Phoneme sequence matching against lexicon
 *
 * BIOLOGICAL: Wernicke's area (BA 22) for word comprehension
 *
 * @param cortex Speech cortex instance
 * @param phonemes Phoneme sequence
 * @param num_phonemes Number of phonemes
 * @param word_buffer Output word string (must be pre-allocated)
 * @param buffer_size Size of word buffer
 * @param confidence Output recognition confidence [0,1]
 * @return true if word recognized, false otherwise
 */
bool speech_cortex_recognize_word(
    speech_cortex_t* cortex,
    const phoneme_t* phonemes,
    uint32_t num_phonemes,
    char* word_buffer,
    uint32_t buffer_size,
    float* confidence
);

/**
 * @brief Add word to lexicon
 *
 * WHAT: Store word → phoneme sequence mapping
 * WHY:  Build vocabulary for word recognition
 * HOW:  Store in phonological lexicon (hash table)
 *
 * @param cortex Speech cortex instance
 * @param word Word string (orthographic)
 * @param phonemes Phoneme sequence
 * @param num_phonemes Number of phonemes
 * @return true on success, false on failure
 */
bool speech_cortex_add_word_to_lexicon(
    speech_cortex_t* cortex,
    const char* word,
    const phoneme_t* phonemes,
    uint32_t num_phonemes
);

//=============================================================================
// Phonological Working Memory
//=============================================================================

/**
 * @brief Store phoneme sequence in phonological buffer
 *
 * WHAT: Maintain phoneme sequence in working memory (Baddeley's phonological loop)
 * WHY:  Enable sentence-level processing and verbal rehearsal
 * HOW:  Circular buffer with ~7±2 item capacity (Miller 1956)
 *
 * BIOLOGICAL: Left inferior parietal cortex (BA 40) for phonological store
 *
 * @param cortex Speech cortex instance
 * @param phonemes Phoneme sequence
 * @param num_phonemes Number of phonemes
 * @return true on success, false if buffer full
 */
bool speech_cortex_store_phonological_buffer(
    speech_cortex_t* cortex,
    const phoneme_t* phonemes,
    uint32_t num_phonemes
);

/**
 * @brief Retrieve phoneme sequence from phonological buffer
 *
 * @param cortex Speech cortex instance
 * @param phonemes Output phoneme sequence (must be pre-allocated)
 * @param max_phonemes Maximum phonemes to retrieve
 * @param num_retrieved Number of phonemes actually retrieved
 * @return true on success, false on failure
 */
bool speech_cortex_retrieve_phonological_buffer(
    speech_cortex_t* cortex,
    phoneme_t* phonemes,
    uint32_t max_phonemes,
    uint32_t* num_retrieved
);

/**
 * @brief Clear phonological buffer
 * @param cortex Speech cortex instance
 */
void speech_cortex_clear_phonological_buffer(speech_cortex_t* cortex);

/**
 * @brief Associate brain with speech cortex for neuromodulation
 *
 * WHAT: Set brain reference for DA + ACh modulation
 * WHY:  Enable neurochemical modulation of speech processing
 * HOW:  Store brain pointer for neurotransmitter reading
 *
 * @param cortex Speech cortex instance
 * @param brain Brain instance (or NULL to clear)
 *
 * BIOLOGY:
 * - Dopamine modulates speech production rate and fluency
 * - Acetylcholine modulates speech comprehension and phoneme discrimination
 *
 * CLINICAL EXAMPLES:
 * - Depression (low DA): Slow, quiet, monotone speech
 * - Mania (high DA): Rapid, pressured speech
 * - ADHD (low ACh): Poor speech comprehension, misses words
 * - Parkinson's (low DA): Hypophonic speech, reduced prosody
 */
void speech_cortex_set_brain(speech_cortex_t* cortex, brain_t brain);

//=============================================================================
// Plasticity API (Phase 10.11)
//=============================================================================

/**
 * @brief Train speech cortex on labeled phoneme example
 *
 * WHAT: Update STDP weights, mirror neurons, and eligibility traces
 * WHY:  Enable phoneme category learning via experience
 * HOW:  Process audio → extract formants → STDP learning → mirror activation
 *
 * ALGORITHM:
 * 1. Extract formants from audio frame
 * 2. Update phoneme detector weights via STDP
 * 3. Activate mirror neurons based on articulatory gestures
 * 4. Update eligibility traces for temporal credit assignment
 * 5. Consolidate learning during dopamine bursts (correct detections)
 *
 * LEARNING MODES:
 * - Supervised: Provide target_phoneme for labeled data
 * - Reinforcement: Provide reward signal for correct/incorrect detections
 * - Combined: Use both for maximum learning efficiency
 *
 * BIOLOGICAL BASIS:
 * - STDP learns phoneme categories in STG (Mesgarani et al. 2014)
 * - Mirror neurons simulate articulation during perception (Liberman 1985)
 * - Dopamine bursts gate consolidation (Reynolds & Wickens 2002)
 * - Eligibility traces assign credit to delayed outcomes (Izhikevich 2007)
 *
 * @param cortex Speech cortex instance
 * @param audio_data Audio frame containing phoneme
 * @param num_samples Number of audio samples
 * @param target_phoneme Ground truth phoneme (PHONEME_UNKNOWN for unsupervised)
 * @param reward Reward signal [0, 1] (1.0 = correct detection, 0.5 = neutral)
 * @return true if training successful, false on error
 *
 * COMPLEXITY: O(P × F) where P = num_phonemes, F = num_formants
 */
bool speech_cortex_train_phoneme(
    speech_cortex_t* cortex,
    const float* audio_data,
    uint32_t num_samples,
    phoneme_t target_phoneme,
    float reward
);

/**
 * @brief Get plasticity statistics
 *
 * WHAT: Query learning progress and plasticity metrics
 * WHY:  Monitor phoneme learning, mirror neuron activity, and neuromodulation
 * HOW:  Return statistics on STDP updates, mirror activations, and burst events
 *
 * @param cortex Speech cortex instance
 * @param stdp_updates Output: total STDP weight updates
 * @param mirror_activations Output: total mirror neuron activations
 * @param burst_events Output: dopamine burst count
 * @param avg_learning_rate Output: average learning rate
 * @return true on success, false on error
 */
bool speech_cortex_get_plasticity_stats(
    const speech_cortex_t* cortex,
    uint64_t* stdp_updates,
    uint64_t* mirror_activations,
    uint64_t* burst_events,
    float* avg_learning_rate
);

/**
 * @brief Get phoneme detector activation for specific phoneme
 *
 * WHAT: Query current activation level of phoneme-selective neuron
 * WHY:  Inspect learned phoneme representations
 * HOW:  Return activation from most recent processing
 *
 * @param cortex Speech cortex instance
 * @param phoneme Phoneme to query
 * @return Activation level [0, 1], or -1.0f on error
 */
float speech_cortex_get_phoneme_activation(
    const speech_cortex_t* cortex,
    phoneme_t phoneme
);

/**
 * @brief Get mirror neuron activation for phoneme
 *
 * WHAT: Query mirror neuron response to phoneme
 * WHY:  Inspect motor theory of speech representations
 * HOW:  Query mirror neuron system activation
 *
 * @param cortex Speech cortex instance
 * @param phoneme Phoneme to query
 * @return Mirror activation level [0, 1], or -1.0f on error
 */
float speech_cortex_get_mirror_activation(
    const speech_cortex_t* cortex,
    phoneme_t phoneme
);

//=============================================================================
// Bidirectional Feedback Functions (Phase 10.11.3)
//=============================================================================

/**
 * @brief Get phoneme confidence from recent processing
 *
 * WHAT: Query how confident speech recognition is
 * WHY:  Audio cortex can boost speech-band processing when confidence is low
 * HOW:  Return average phoneme detection confidence
 *
 * BIOLOGY: Top-down feedback from STG sharpens A1 frequency tuning
 *
 * @param cortex Speech cortex instance
 * @return Average phoneme confidence [0, 1]
 */
float speech_cortex_get_phoneme_confidence(speech_cortex_t* cortex);

/**
 * @brief Request frequency resolution boost from audio cortex
 *
 * WHAT: Signal need for enhanced frequency resolution
 * WHY:  Difficult phoneme discrimination needs finer frequency detail
 * HOW:  Return target frequency bands for enhancement
 *
 * @param cortex Speech cortex instance
 * @param target_freq_hz Output: center frequency needing enhancement
 * @param bandwidth_hz Output: bandwidth around target
 * @return true if boost needed, false if recognition is confident
 */
bool speech_cortex_request_frequency_boost(speech_cortex_t* cortex,
                                            float* target_freq_hz,
                                            float* bandwidth_hz);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get phoneme name string
 * @param phoneme Phoneme enum
 * @return Human-readable phoneme name (e.g., "IY", "P", "S")
 */
const char* speech_cortex_phoneme_name(phoneme_t phoneme);

/**
 * @brief Get phoneme IPA symbol
 * @param phoneme Phoneme enum
 * @return IPA symbol (e.g., "i:", "p", "s")
 */
const char* speech_cortex_phoneme_ipa(phoneme_t phoneme);

/**
 * @brief Check if phoneme is vowel
 * @param phoneme Phoneme enum
 * @return true if vowel, false if consonant
 */
bool speech_cortex_is_vowel(phoneme_t phoneme);

/**
 * @brief Get default speech cortex configuration
 * @return Default configuration with sensible values
 */
speech_cortex_config_t speech_cortex_default_config(void);

//=============================================================================
// Bio-Async Communication API
//=============================================================================

/**
 * @brief Get bio-async module context
 *
 * @param cortex Speech cortex instance
 * @return Bio-async module context, or NULL if not enabled
 */
bio_module_context_t speech_cortex_get_bio_context(speech_cortex_t* cortex);

/**
 * @brief Process pending bio-async messages
 *
 * @param cortex Speech cortex instance
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t speech_cortex_process_bio_messages(speech_cortex_t* cortex, uint32_t max_messages);

/**
 * @brief Broadcast phoneme recognition via bio-async
 *
 * @param cortex Speech cortex instance
 * @param phoneme Recognized phoneme
 * @param confidence Recognition confidence (0-1)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t speech_cortex_broadcast_phoneme(
    speech_cortex_t* cortex,
    phoneme_t phoneme,
    float confidence
);

/**
 * @brief Broadcast word recognition via bio-async
 *
 * @param cortex Speech cortex instance
 * @param word Recognized word string
 * @param confidence Recognition confidence (0-1)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t speech_cortex_broadcast_word(
    speech_cortex_t* cortex,
    const char* word,
    float confidence
);

//=============================================================================
// Second Messenger Cascade API (Phase 10.12)
//=============================================================================

/**
 * @brief Trigger receptor activation for second messenger cascades
 *
 * WHAT: Activate G-protein coupled receptors to trigger cascades
 * WHY:  Neuromodulators enhance phoneme discrimination and speech processing
 * HOW:  Norepinephrine -> beta-adrenergic -> cAMP -> PKA for phoneme sharpening
 *       Acetylcholine -> muscarinic -> IP3/DAG for comprehension
 *       Dopamine -> D1 -> cAMP for speech rate modulation
 *
 * BIOLOGICAL MAPPING:
 * - Norepinephrine: Enhances phoneme discrimination in STG via beta-adrenergic receptors
 * - Acetylcholine: Modulates speech comprehension in Wernicke's area
 * - Dopamine: Affects speech processing rate and fluency
 *
 * @param cortex Speech cortex instance
 * @param neuron_id Neuron ID within speech cortex
 * @param receptor_type Receptor type (from neuromodulator system)
 * @param occupancy Receptor occupancy [0, 1]
 * @param timestamp_ms Current timestamp
 * @return true on success, false on error
 */
bool speech_cortex_trigger_receptor(
    speech_cortex_t* cortex,
    uint32_t neuron_id,
    int receptor_type,
    float occupancy,
    uint64_t timestamp_ms
);

/**
 * @brief Get second messenger cascade state for neuron
 *
 * WHAT: Query current cascade state (PKA, PKC, CaMKII activities)
 * WHY:  Other modules need to know cascade state for integration
 * HOW:  Forward query to second messenger system
 *
 * @param cortex Speech cortex instance
 * @param neuron_id Neuron ID to query
 * @param state Output state buffer (must be allocated by caller)
 * @return true on success, false on error
 */
bool speech_cortex_get_second_messenger_state(
    const speech_cortex_t* cortex,
    uint32_t neuron_id,
    void* state  /* second_messenger_state_t* */
);

//=============================================================================
// Positional Encoding API (Phase 10.13)
//=============================================================================

/**
 * @brief Configure positional encoding for speech cortex
 *
 * WHAT: Set up positional encoding parameters for phoneme sequences
 * WHY:  Position information is critical for speech processing serial order
 * HOW:  Create and configure two encoders: sinusoidal for sequences, learned for buffer
 *
 * BIOLOGICAL BASIS:
 * - Phonological loop maintains serial order of phonemes (Baddeley & Hitch 1974)
 * - Speech processing is inherently sequential (STG→Wernicke)
 * - Position encoding supports temporal ordering in language comprehension
 * - Primacy/recency effects in working memory suggest position-dependent encoding
 *
 * ENCODING STRATEGIES:
 * 1. Sinusoidal for phoneme sequences: Variable length, needs extrapolation
 * 2. Learned for phonological buffer: Fixed capacity (9 items), optimized per-position
 *
 * @param cortex Speech cortex instance
 * @param enable Enable or disable positional encoding
 * @param embedding_dim Dimension of position embeddings (0 = use default)
 * @param phoneme_seq_type Type for phoneme sequences (0=sinusoidal, 1=learned)
 * @param buffer_type Type for phonological buffer (0=sinusoidal, 1=learned)
 * @return true on success, false on error
 */
bool speech_cortex_set_pe_config(
    speech_cortex_t* cortex,
    bool enable,
    uint32_t embedding_dim,
    uint32_t phoneme_seq_type,
    uint32_t buffer_type
);

/**
 * @brief Apply positional encoding to phoneme sequence
 *
 * WHAT: Add position embeddings to phoneme feature representations
 * WHY:  Enable position-aware phoneme processing for speech comprehension
 * HOW:  Use sinusoidal encoder to generate position embeddings for each phoneme
 *
 * BIOLOGICAL BASIS:
 * - Superior Temporal Gyrus (STG) neurons are sensitive to phoneme position
 * - Serial order information critical for distinguishing words (e.g., "stop" vs "pots")
 * - Phoneme positions affect coarticulation patterns
 *
 * ALGORITHM:
 * 1. For each phoneme in sequence [0, num_phonemes):
 *    - Generate position embedding using sinusoidal encoder
 *    - Store embedding in phoneme_event_t.position_embedding
 *    - Set phoneme_event_t.sequence_position = index
 * 2. If additive mode: Add position embedding to phoneme features
 *
 * @param cortex Speech cortex instance
 * @param phonemes Array of phoneme events
 * @param num_phonemes Number of phonemes in sequence
 * @param additive If true, add to features; if false, just store embeddings
 * @return true on success, false on error
 */
bool speech_cortex_encode_phoneme_positions(
    speech_cortex_t* cortex,
    phoneme_event_t* phonemes,
    uint32_t num_phonemes,
    bool additive
);

/**
 * @brief Get position embedding for phonological buffer slot
 *
 * WHAT: Retrieve learned position embedding for specific buffer position
 * WHY:  Phonological working memory slots have distinct position encodings
 * HOW:  Use learned encoder to get embedding for buffer slot [0, 8]
 *
 * BIOLOGICAL BASIS:
 * - Phonological buffer has fixed capacity (7±2 items, Miller 1956)
 * - Serial position effects: primacy (first items) and recency (last items)
 * - Left inferior parietal cortex (BA 40) encodes phonological store positions
 * - Position in buffer affects recall probability and latency
 *
 * USE CASES:
 * - Retrieving phonemes from working memory in order
 * - Implementing serial recall tasks
 * - Modeling position-dependent decay rates
 *
 * @param cortex Speech cortex instance
 * @param buffer_position Position in phonological buffer [0, 8]
 * @param output Output position embedding (pe_embedding_dim floats, pre-allocated)
 * @return true on success, false on error
 */
bool speech_cortex_get_phonological_position_embedding(
    speech_cortex_t* cortex,
    uint32_t buffer_position,
    float* output
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SPEECH_CORTEX_H
