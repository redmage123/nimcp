/**
 * @file nimcp_phonological_analyzer.h
 * @brief Phonological analysis layer for Wernicke's area
 *
 * WHAT: Phoneme categorization, syllable parsing, and prosodic analysis
 * WHY:  First processing layer in language comprehension pipeline
 * HOW:  Formant extraction → Feature vectors → Phoneme classification
 *
 * BIOLOGICAL BASIS:
 * - Superior Temporal Gyrus (STG) contains phoneme-selective neurons
 * - Phoneme categories are perceptual abstractions (Liberman et al. 1957)
 * - Categorical perception: continuous acoustic signal → discrete phonemes
 * - Coarticulation: phoneme boundaries influenced by neighboring sounds
 *
 * PROCESSING STAGES:
 * 1. Feature extraction: Formants (F1-F4), spectral centroid, ZCR
 * 2. Phoneme classification: Feature vectors → phoneme categories
 * 3. Syllable parsing: Phoneme sequences → syllable boundaries
 * 4. Prosodic analysis: Pitch, stress, and rhythm extraction
 *
 * @version Phase W1: Wernicke's Area Core Integration
 * @date 2026-01-04
 */

#ifndef NIMCP_PHONOLOGICAL_ANALYZER_H
#define NIMCP_PHONOLOGICAL_ANALYZER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* Phoneme types */
#include "perception/nimcp_speech_cortex.h"

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Default configuration values
 */
#define PHON_DEFAULT_FRAME_SIZE_MS       25
#define PHON_DEFAULT_HOP_SIZE_MS         10
#define PHON_DEFAULT_NUM_FORMANTS        4
#define PHON_DEFAULT_MAX_PHONEMES        256
#define PHON_DEFAULT_MAX_SYLLABLES       64
#define PHON_DEFAULT_FFT_SIZE            512

/**
 * @brief Phonological analyzer configuration
 */
typedef struct {
    /* Audio parameters */
    uint32_t sample_rate;                /**< Audio sample rate (Hz) */
    uint32_t frame_size_ms;              /**< Analysis frame size (ms) */
    uint32_t hop_size_ms;                /**< Frame hop size (ms) */
    uint32_t fft_size;                   /**< FFT window size */

    /* Formant tracking */
    uint32_t num_formants;               /**< Number of formants (F1-F4) */
    float formant_floor_hz;              /**< Minimum formant frequency */
    float formant_ceiling_hz;            /**< Maximum formant frequency */

    /* Phoneme detection */
    uint32_t max_phonemes;               /**< Maximum phonemes per utterance */
    float confidence_threshold;          /**< Minimum confidence for detection */

    /* Syllable parsing */
    uint32_t max_syllables;              /**< Maximum syllables per utterance */
    bool enable_syllable_parsing;        /**< Enable syllable boundary detection */

    /* Prosody */
    bool enable_prosody;                 /**< Enable prosodic analysis */
    bool enable_stress_detection;        /**< Enable stress pattern detection */
    bool enable_pitch_tracking;          /**< Enable F0 tracking */
} phonological_config_t;

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief Formant analysis result
 */
typedef struct {
    float f1;                            /**< First formant (Hz) - tongue height */
    float f2;                            /**< Second formant (Hz) - tongue frontness */
    float f3;                            /**< Third formant (Hz) - rhoticity */
    float f4;                            /**< Fourth formant (Hz) */
    float f1_bandwidth;                  /**< F1 bandwidth (Hz) */
    float f2_bandwidth;                  /**< F2 bandwidth (Hz) */
    float f3_bandwidth;                  /**< F3 bandwidth (Hz) */
    float f4_bandwidth;                  /**< F4 bandwidth (Hz) */
} formant_result_t;

/**
 * @brief Acoustic feature vector for phoneme classification
 */
typedef struct {
    /* Formant features */
    formant_result_t formants;           /**< Formant frequencies and bandwidths */

    /* Spectral features */
    float spectral_centroid;             /**< Center of spectral mass (Hz) */
    float spectral_spread;               /**< Spectral standard deviation */
    float spectral_rolloff;              /**< 85% energy rolloff frequency */
    float spectral_flux;                 /**< Spectral change rate */

    /* Temporal features */
    float zero_crossing_rate;            /**< Zero crossings per second */
    float rms_energy;                    /**< Root mean square energy */
    float duration_ms;                   /**< Segment duration */

    /* Voicing features */
    float fundamental_freq;              /**< F0 (Hz) or 0 if unvoiced */
    float harmonicity;                   /**< Harmonic-to-noise ratio */
    bool is_voiced;                      /**< Voice activity detection */

    /* MFCCs (optional) */
    float mfcc[13];                      /**< Mel-frequency cepstral coefficients */
    bool has_mfcc;                       /**< MFCC computed flag */
} acoustic_features_t;

/**
 * @brief Syllable structure
 */
typedef struct {
    uint32_t start_phoneme;              /**< First phoneme index */
    uint32_t end_phoneme;                /**< Last phoneme index (exclusive) */
    uint32_t nucleus_phoneme;            /**< Vowel nucleus index */
    float stress_level;                  /**< Syllable stress [0,1] */
    float duration_ms;                   /**< Syllable duration */
    uint64_t onset_time_ms;              /**< Syllable onset time */
    uint64_t offset_time_ms;             /**< Syllable offset time */
    bool is_stressed;                    /**< Primary stress flag */
} syllable_t;

/**
 * @brief Prosodic contour
 */
typedef struct {
    float* pitch_contour;                /**< F0 values over time */
    float* intensity_contour;            /**< Intensity values over time */
    uint32_t num_frames;                 /**< Number of frames in contours */
    float mean_pitch;                    /**< Mean F0 (Hz) */
    float pitch_range;                   /**< F0 range (Hz) */
    float speech_rate;                   /**< Syllables per second */
} prosodic_contour_t;

/**
 * @brief Complete phonological analysis result
 */
typedef struct {
    /* Phoneme sequence */
    phoneme_event_t* phonemes;           /**< Detected phonemes */
    uint32_t num_phonemes;               /**< Number of phonemes */

    /* Syllable structure */
    syllable_t* syllables;               /**< Parsed syllables */
    uint32_t num_syllables;              /**< Number of syllables */

    /* Prosody */
    prosodic_contour_t prosody;          /**< Prosodic contour */

    /* Quality metrics */
    float avg_confidence;                /**< Average phoneme confidence */
    float signal_quality;                /**< Input signal quality [0,1] */
    uint64_t processing_time_ms;         /**< Processing time */
} phonological_result_t;

/*=============================================================================
 * PHONOLOGICAL ANALYZER OPAQUE TYPE
 *===========================================================================*/

typedef struct phonological_analyzer phonological_analyzer_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default configuration
 *
 * @return Default configuration
 */
phonological_config_t wernicke_phonological_default_config(void);

/**
 * @brief Create phonological analyzer
 *
 * @param config Configuration (NULL for defaults)
 * @return New analyzer or NULL on failure
 */
phonological_analyzer_t* wernicke_phonological_create(const phonological_config_t* config);

/**
 * @brief Destroy phonological analyzer
 *
 * @param analyzer Analyzer to destroy
 */
void wernicke_phonological_destroy(phonological_analyzer_t* analyzer);

/**
 * @brief Reset analyzer state
 *
 * @param analyzer Analyzer instance
 * @return true on success
 */
bool wernicke_phonological_reset(phonological_analyzer_t* analyzer);

/*=============================================================================
 * FEATURE EXTRACTION
 *===========================================================================*/

/**
 * @brief Extract formants from audio frame
 *
 * WHAT: Track vocal tract resonances (F1-F4)
 * WHY:  Formants uniquely identify vowels
 * HOW:  LPC analysis with root finding
 *
 * @param analyzer Analyzer instance
 * @param audio Audio samples
 * @param num_samples Number of samples
 * @param result Output formant result
 * @return true on success
 */
bool phonological_extract_formants(
    phonological_analyzer_t* analyzer,
    const float* audio,
    uint32_t num_samples,
    formant_result_t* result
);

/**
 * @brief Extract acoustic features from audio frame
 *
 * WHAT: Compute full acoustic feature vector
 * WHY:  Features needed for phoneme classification
 * HOW:  FFT-based spectral analysis + formants + MFCCs
 *
 * @param analyzer Analyzer instance
 * @param audio Audio samples
 * @param num_samples Number of samples
 * @param features Output feature vector
 * @return true on success
 */
bool phonological_extract_features(
    phonological_analyzer_t* analyzer,
    const float* audio,
    uint32_t num_samples,
    acoustic_features_t* features
);

/*=============================================================================
 * PHONEME CLASSIFICATION
 *===========================================================================*/

/**
 * @brief Classify phoneme from acoustic features
 *
 * WHAT: Map features to phoneme category
 * WHY:  Convert continuous acoustics to discrete phonemes
 * HOW:  Distance-based classification in feature space
 *
 * @param analyzer Analyzer instance
 * @param features Input feature vector
 * @param phoneme Output phoneme category
 * @param confidence Output confidence [0,1]
 * @return true on success
 */
bool phonological_classify_phoneme(
    phonological_analyzer_t* analyzer,
    const acoustic_features_t* features,
    phoneme_t* phoneme,
    float* confidence
);

/**
 * @brief Classify vowel from formants
 *
 * WHAT: Map (F1, F2) to vowel phoneme
 * WHY:  Vowels identified by formant frequencies
 * HOW:  Euclidean distance in F1-F2 space
 *
 * @param analyzer Analyzer instance
 * @param f1 First formant (Hz)
 * @param f2 Second formant (Hz)
 * @param phoneme Output vowel phoneme
 * @param confidence Output confidence [0,1]
 * @return true on success
 */
bool phonological_classify_vowel(
    phonological_analyzer_t* analyzer,
    float f1,
    float f2,
    phoneme_t* phoneme,
    float* confidence
);

/**
 * @brief Classify consonant from spectral features
 *
 * WHAT: Map spectral envelope to consonant phoneme
 * WHY:  Consonants characterized by spectral shape
 * HOW:  Spectral centroid + ZCR + duration features
 *
 * @param analyzer Analyzer instance
 * @param features Acoustic features
 * @param phoneme Output consonant phoneme
 * @param confidence Output confidence [0,1]
 * @return true on success
 */
bool phonological_classify_consonant(
    phonological_analyzer_t* analyzer,
    const acoustic_features_t* features,
    phoneme_t* phoneme,
    float* confidence
);

/*=============================================================================
 * COMPLETE ANALYSIS
 *===========================================================================*/

/**
 * @brief Analyze audio for complete phonological representation
 *
 * WHAT: Full phonological analysis pipeline
 * WHY:  Get phonemes, syllables, and prosody from audio
 * HOW:  Feature extraction → Classification → Segmentation
 *
 * @param analyzer Analyzer instance
 * @param audio Audio samples
 * @param num_samples Number of samples
 * @param result Output phonological result
 * @return true on success
 */
bool phonological_analyze(
    phonological_analyzer_t* analyzer,
    const float* audio,
    uint32_t num_samples,
    phonological_result_t* result
);

/**
 * @brief Detect phonemes in audio stream
 *
 * WHAT: Extract phoneme sequence
 * WHY:  Core phoneme detection function
 * HOW:  Sliding window analysis with segmentation
 *
 * @param analyzer Analyzer instance
 * @param audio Audio samples
 * @param num_samples Number of samples
 * @param phonemes Output phoneme events
 * @param max_phonemes Maximum phonemes
 * @param num_detected Output: phonemes detected
 * @return true on success
 */
bool phonological_detect_phonemes(
    phonological_analyzer_t* analyzer,
    const float* audio,
    uint32_t num_samples,
    phoneme_event_t* phonemes,
    uint32_t max_phonemes,
    uint32_t* num_detected
);

/*=============================================================================
 * SYLLABLE PARSING
 *===========================================================================*/

/**
 * @brief Parse syllables from phoneme sequence
 *
 * WHAT: Group phonemes into syllables
 * WHY:  Syllables are rhythmic units of speech
 * HOW:  Sonority-based syllabification
 *
 * @param analyzer Analyzer instance
 * @param phonemes Input phoneme sequence
 * @param num_phonemes Number of phonemes
 * @param syllables Output syllable array
 * @param max_syllables Maximum syllables
 * @param num_syllables Output: syllables parsed
 * @return true on success
 */
bool phonological_parse_syllables(
    phonological_analyzer_t* analyzer,
    const phoneme_event_t* phonemes,
    uint32_t num_phonemes,
    syllable_t* syllables,
    uint32_t max_syllables,
    uint32_t* num_syllables
);

/**
 * @brief Detect syllable nuclei (vowels)
 *
 * WHAT: Find vowel centers of syllables
 * WHY:  Vowels form syllable nuclei
 * HOW:  Energy peaks in vowel frequency bands
 *
 * @param analyzer Analyzer instance
 * @param phonemes Input phoneme sequence
 * @param num_phonemes Number of phonemes
 * @param nuclei_indices Output nucleus indices
 * @param max_nuclei Maximum nuclei
 * @param num_nuclei Output: nuclei found
 * @return true on success
 */
bool phonological_detect_nuclei(
    phonological_analyzer_t* analyzer,
    const phoneme_event_t* phonemes,
    uint32_t num_phonemes,
    uint32_t* nuclei_indices,
    uint32_t max_nuclei,
    uint32_t* num_nuclei
);

/*=============================================================================
 * PROSODIC ANALYSIS
 *===========================================================================*/

/**
 * @brief Extract prosodic contour from audio
 *
 * WHAT: Track pitch and intensity over time
 * WHY:  Prosody conveys emotion and emphasis
 * HOW:  Autocorrelation pitch tracking
 *
 * @param analyzer Analyzer instance
 * @param audio Audio samples
 * @param num_samples Number of samples
 * @param prosody Output prosodic contour
 * @return true on success
 */
bool phonological_extract_prosody(
    phonological_analyzer_t* analyzer,
    const float* audio,
    uint32_t num_samples,
    prosodic_contour_t* prosody
);

/**
 * @brief Detect stress pattern
 *
 * WHAT: Identify stressed syllables
 * WHY:  Stress affects word meaning and syntax
 * HOW:  Pitch + duration + intensity analysis
 *
 * @param analyzer Analyzer instance
 * @param syllables Input syllable array
 * @param num_syllables Number of syllables
 * @param stress_pattern Output stress flags (bool array)
 * @return true on success
 */
bool phonological_detect_stress(
    phonological_analyzer_t* analyzer,
    syllable_t* syllables,
    uint32_t num_syllables,
    bool* stress_pattern
);

/*=============================================================================
 * COARTICULATION
 *===========================================================================*/

/**
 * @brief Apply coarticulation model
 *
 * WHAT: Adjust phoneme boundaries for coarticulation
 * WHY:  Phonemes overlap in natural speech
 * HOW:  Locus equations and transition modeling
 *
 * @param analyzer Analyzer instance
 * @param phonemes Input/output phoneme sequence
 * @param num_phonemes Number of phonemes
 * @return true on success
 */
bool phonological_apply_coarticulation(
    phonological_analyzer_t* analyzer,
    phoneme_event_t* phonemes,
    uint32_t num_phonemes
);

/*=============================================================================
 * RESULT MANAGEMENT
 *===========================================================================*/

/**
 * @brief Allocate phonological result
 *
 * @param max_phonemes Maximum phonemes
 * @param max_syllables Maximum syllables
 * @param max_frames Maximum prosody frames
 * @return Allocated result or NULL
 */
phonological_result_t* phonological_result_alloc(
    uint32_t max_phonemes,
    uint32_t max_syllables,
    uint32_t max_frames
);

/**
 * @brief Free phonological result
 *
 * @param result Result to free
 */
void phonological_result_free(phonological_result_t* result);

/*=============================================================================
 * VOWEL SPACE CONSTANTS
 *===========================================================================*/

/**
 * @brief Vowel formant prototypes (average adult male)
 *
 * Reference: Peterson & Barney (1952)
 */
typedef struct {
    phoneme_t phoneme;
    float f1;  /**< F1 in Hz */
    float f2;  /**< F2 in Hz */
} vowel_prototype_t;

/**
 * @brief Get vowel prototype array
 *
 * @param count Output: number of prototypes
 * @return Prototype array (static, do not free)
 */
const vowel_prototype_t* phonological_get_vowel_prototypes(uint32_t* count);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PHONOLOGICAL_ANALYZER_H */
