/**
 * @file nimcp_phonological.h
 * @brief Phonological processing module for Broca's region
 *
 * WHAT: Phoneme sequencing, syllable planning, and prosody control
 * WHY:  Essential for speech production planning and articulation
 * HOW:  Phoneme buffering, syllable boundary detection, stress application
 *
 * BIOLOGICAL BASIS:
 * - Broca's Area (BA44/45): Speech production, articulation planning
 * - Left Inferior Frontal Gyrus: Phonological working memory
 * - Premotor Cortex: Motor planning for speech articulation
 * - Supplementary Motor Area: Sequencing and timing
 *
 * LINGUISTIC MODELS:
 * - Levelt's Speaking Model (1989): Phonological encoding stage
 * - Phonological Loop (Baddeley): Working memory for phonology
 * - Metrical Phonology: Stress patterns and syllable structure
 * - Coarticulation Theory: Overlapping articulatory gestures
 *
 * NEUROSCIENCE REFERENCES:
 * - Hickok & Poeppel (2007): "The cortical organization of speech processing"
 * - Indefrey & Levelt (2004): "The spatial and temporal signatures of word production"
 * - Guenther (2016): "Neural Control of Speech"
 *
 * RESPONSIBILITIES (SRP):
 * 1. Phoneme sequencing and pattern generation
 * 2. Syllable structure planning
 * 3. Prosody control (stress, intonation patterns)
 * 4. Coarticulation planning
 *
 * @version Phase B1: Broca's Region Phonological Processing
 * @date 2025-11-22
 */

#ifndef NIMCP_PHONOLOGICAL_H
#define NIMCP_PHONOLOGICAL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// CONSTANTS
//=============================================================================

/* Buffer sizes */
#define PHONOLOGICAL_DEFAULT_MAX_PHONEMES 256
#define PHONOLOGICAL_DEFAULT_MAX_SYLLABLES 128

/* Phoneme categories (IPA-based) */
#define PHONEME_CATEGORY_VOWEL 0x00
#define PHONEME_CATEGORY_CONSONANT 0x01
#define PHONEME_CATEGORY_SEMIVOWEL 0x02
#define PHONEME_CATEGORY_SILENCE 0x03

/* Syllable structure */
#define SYLLABLE_MAX_ONSET 3      /* Maximum consonants in onset (e.g., "str") */
#define SYLLABLE_MAX_NUCLEUS 2    /* Maximum vowels in nucleus (diphthongs) */
#define SYLLABLE_MAX_CODA 4       /* Maximum consonants in coda (e.g., "ngths") */

/* Stress levels */
#define STRESS_LEVEL_NONE 0.0f     /* Unstressed */
#define STRESS_LEVEL_SECONDARY 0.5f /* Secondary stress */
#define STRESS_LEVEL_PRIMARY 1.0f   /* Primary stress */

/* Prosody parameters */
#define PROSODY_F0_MIN 80.0f       /* Minimum fundamental frequency (Hz) */
#define PROSODY_F0_MAX 300.0f      /* Maximum fundamental frequency (Hz) */
#define PROSODY_F0_DEFAULT 120.0f  /* Default fundamental frequency (Hz) */

//=============================================================================
// ENUMERATIONS
//=============================================================================

/**
 * @brief Syllable types
 */
typedef enum {
    SYLLABLE_TYPE_OPEN,        /* CV, V (ends in vowel) */
    SYLLABLE_TYPE_CLOSED,      /* CVC, VC (ends in consonant) */
    SYLLABLE_TYPE_ONSET_ONLY,  /* Single consonant */
    SYLLABLE_TYPE_NUCLEUS_ONLY /* Single vowel */
} syllable_type_t;

/**
 * @brief Intonation patterns
 */
typedef enum {
    INTONATION_PATTERN_FLAT,       /* Monotone */
    INTONATION_PATTERN_RISING,     /* Question intonation */
    INTONATION_PATTERN_FALLING,    /* Statement intonation */
    INTONATION_PATTERN_RISE_FALL,  /* Emphasis */
    INTONATION_PATTERN_FALL_RISE   /* Uncertainty */
} intonation_pattern_t;

/**
 * @brief Processing status
 */
typedef enum {
    PHONOLOGICAL_STATUS_IDLE,           /* No active processing */
    PHONOLOGICAL_STATUS_BUFFERING,      /* Accumulating phonemes */
    PHONOLOGICAL_STATUS_SYLLABIFYING,   /* Creating syllable structure */
    PHONOLOGICAL_STATUS_APPLYING_STRESS,/* Applying stress patterns */
    PHONOLOGICAL_STATUS_READY           /* Ready for articulation */
} phonological_status_t;

//=============================================================================
// DATA STRUCTURES
//=============================================================================

/**
 * @brief Individual phoneme representation
 */
typedef struct {
    uint8_t symbol;           /* IPA or custom phoneme code */
    uint8_t category;         /* Vowel, consonant, etc. */
    float duration_ms;        /* Duration in milliseconds */
    float voicing;            /* Voicing strength [0-1] */
    bool is_stressed;         /* Part of stressed syllable? */
} phoneme_t;

/**
 * @brief Syllable structure
 */
typedef struct {
    syllable_type_t type;                       /* Open, closed, etc. */

    /* Syllable components */
    phoneme_t onset[SYLLABLE_MAX_ONSET];        /* Initial consonant(s) */
    uint8_t onset_count;

    phoneme_t nucleus[SYLLABLE_MAX_NUCLEUS];    /* Vowel(s) */
    uint8_t nucleus_count;

    phoneme_t coda[SYLLABLE_MAX_CODA];          /* Final consonant(s) */
    uint8_t coda_count;

    /* Prosodic features */
    float stress_level;       /* 0.0 (none) to 1.0 (primary) */
    float pitch_f0;           /* Fundamental frequency (Hz) */
    float duration_ms;        /* Total syllable duration */

    /* Position in word/utterance */
    bool is_initial;          /* First syllable? */
    bool is_final;            /* Last syllable? */

} syllable_t;

/**
 * @brief Prosody curve for intonation
 */
typedef struct {
    float f0_values[PHONOLOGICAL_DEFAULT_MAX_PHONEMES]; /* F0 per phoneme */
    uint32_t num_points;
    intonation_pattern_t pattern;
    float baseline_f0;        /* Speaker's baseline pitch */
    float f0_range;           /* Pitch range (semitones) */
} prosody_curve_t;

/**
 * @brief Configuration structure
 */
typedef struct {
    uint32_t max_phonemes;    /* Phoneme buffer capacity */
    uint32_t max_syllables;   /* Syllable buffer capacity */
    float stress_weight;      /* Weight for stress application [0-1] */
    bool enable_prosody;      /* Enable prosody generation? */
    bool enable_coarticulation; /* Enable coarticulation planning? */
    float default_f0;         /* Default fundamental frequency (Hz) */
} phonological_config_t;

/**
 * @brief Opaque phonological processor handle
 */
typedef struct phonological_processor phonological_processor_t;

//=============================================================================
// LIFECYCLE FUNCTIONS
//=============================================================================

/**
 * @brief Get default configuration
 *
 * WHAT: Returns default phonological processor configuration
 * WHY:  Provides sensible defaults for initialization
 * HOW:  Sets default buffer sizes, prosody settings
 *
 * @return Default configuration structure
 */
phonological_config_t phonological_default_config(void);

/**
 * @brief Create phonological processor
 *
 * WHAT: Allocates and initializes phonological processor
 * WHY:  Central system for speech planning and phoneme sequencing
 * HOW:  Allocates buffers, initializes state, sets configuration
 *
 * @param config Configuration (or NULL for defaults)
 * @return Processor handle or NULL on failure
 */
phonological_processor_t* phonological_create(const phonological_config_t* config);

/**
 * @brief Destroy phonological processor
 *
 * WHAT: Frees processor resources
 * WHY:  Prevent memory leaks
 * HOW:  Frees all allocated buffers and state
 *
 * @param processor Processor to destroy (NULL safe)
 */
void phonological_destroy(phonological_processor_t* processor);

/**
 * @brief Reset processor to initial state
 *
 * WHAT: Clears all buffers and resets state
 * WHY:  Prepare for new utterance
 * HOW:  Clears phoneme/syllable buffers, resets status
 *
 * @param processor Processor to reset
 * @return true on success, false on invalid input
 */
bool phonological_reset(phonological_processor_t* processor);

//=============================================================================
// PHONEME OPERATIONS
//=============================================================================

/**
 * @brief Add phoneme to buffer
 *
 * WHAT: Appends phoneme to processing buffer
 * WHY:  Build up phoneme sequence for syllabification
 * HOW:  Adds to buffer, checks capacity
 *
 * @param processor Phonological processor
 * @param phoneme Phoneme code (IPA or custom)
 * @return true on success, false if buffer full or invalid input
 */
bool phonological_add_phoneme(phonological_processor_t* processor, uint8_t phoneme);

/**
 * @brief Add phoneme with full details
 *
 * WHAT: Adds phoneme with category, duration, voicing
 * WHY:  Provide fine-grained control over phoneme properties
 * HOW:  Populates full phoneme_t structure
 *
 * @param processor Phonological processor
 * @param phoneme Phoneme code
 * @param category Phoneme category (vowel, consonant, etc.)
 * @param duration_ms Duration in milliseconds
 * @param voicing Voicing strength [0-1]
 * @return true on success, false on error
 */
bool phonological_add_phoneme_detailed(phonological_processor_t* processor,
                                       uint8_t phoneme,
                                       uint8_t category,
                                       float duration_ms,
                                       float voicing);

/**
 * @brief Get number of buffered phonemes
 *
 * @param processor Phonological processor
 * @return Phoneme count (0 if processor is NULL)
 */
uint32_t phonological_get_phoneme_count(const phonological_processor_t* processor);

/**
 * @brief Get phoneme by index
 *
 * WHAT: Retrieve a phoneme from the buffer
 * WHY:  Enable iteration over phoneme sequence
 * HOW:  Copy phoneme at index to output
 *
 * @param processor Phonological processor
 * @param index Phoneme index (0 to count-1)
 * @param output Output phoneme structure (filled on success)
 * @return true on success, false if index out of bounds
 */
bool phonological_get_phoneme(const phonological_processor_t* processor,
                              uint32_t index,
                              phoneme_t* output);

/**
 * @brief Clear phoneme buffer
 *
 * @param processor Phonological processor
 * @return true on success, false on invalid input
 */
bool phonological_clear_phonemes(phonological_processor_t* processor);

//=============================================================================
// SYLLABLE OPERATIONS
//=============================================================================

/**
 * @brief Generate syllables from phoneme buffer
 *
 * WHAT: Segments phonemes into syllable structures
 * WHY:  Syllables are the basic unit of speech production
 * HOW:  Apply syllabification rules (onset maximization, sonority)
 *
 * @param processor Phonological processor
 * @return true on success, false on error
 */
bool phonological_generate_syllables(phonological_processor_t* processor);

/**
 * @brief Apply stress to syllable
 *
 * WHAT: Sets stress level for specific syllable
 * WHY:  Stress patterns are critical for intelligibility
 * HOW:  Updates syllable stress_level field
 *
 * @param processor Phonological processor
 * @param syllable_idx Syllable index (0-based)
 * @param stress_level Stress level [0-1]
 * @return true on success, false on invalid index
 */
bool phonological_apply_stress(phonological_processor_t* processor,
                               uint32_t syllable_idx,
                               float stress_level);

/**
 * @brief Get number of generated syllables
 *
 * @param processor Phonological processor
 * @return Syllable count (0 if processor is NULL)
 */
uint32_t phonological_get_syllable_count(const phonological_processor_t* processor);

/**
 * @brief Get syllable by index
 *
 * WHAT: Retrieves syllable structure by index
 * WHY:  Allow inspection of syllable details
 * HOW:  Copies syllable to output parameter
 *
 * @param processor Phonological processor
 * @param syllable_idx Syllable index (0-based)
 * @param output Output syllable structure
 * @return true on success, false on invalid index or NULL output
 */
bool phonological_get_syllable(const phonological_processor_t* processor,
                               uint32_t syllable_idx,
                               syllable_t* output);

//=============================================================================
// PROSODY OPERATIONS
//=============================================================================

/**
 * @brief Generate prosody curve
 *
 * WHAT: Creates pitch contour for utterance
 * WHY:  Prosody conveys meaning and emotion
 * HOW:  Applies intonation pattern to generate F0 values
 *
 * @param processor Phonological processor
 * @param pattern Intonation pattern
 * @return true on success, false on error
 */
bool phonological_generate_prosody(phonological_processor_t* processor,
                                   intonation_pattern_t pattern);

/**
 * @brief Set baseline fundamental frequency
 *
 * WHAT: Sets speaker's baseline pitch
 * WHY:  Different speakers have different pitch ranges
 * HOW:  Updates prosody curve baseline
 *
 * @param processor Phonological processor
 * @param f0_hz Fundamental frequency in Hz [PROSODY_F0_MIN - PROSODY_F0_MAX]
 * @return true on success, false on invalid input
 */
bool phonological_set_baseline_f0(phonological_processor_t* processor, float f0_hz);

/**
 * @brief Get prosody curve
 *
 * WHAT: Retrieves generated prosody curve
 * WHY:  Allow integration with speech synthesis
 * HOW:  Copies prosody curve to output parameter
 *
 * @param processor Phonological processor
 * @param output Output prosody curve structure
 * @return true on success, false on NULL input or no prosody generated
 */
bool phonological_get_prosody(const phonological_processor_t* processor,
                              prosody_curve_t* output);

//=============================================================================
// COARTICULATION PLANNING
//=============================================================================

/**
 * @brief Plan coarticulation between phonemes
 *
 * WHAT: Adjusts phoneme features for smooth transitions
 * WHY:  Natural speech involves overlapping articulation
 * HOW:  Modifies phoneme durations and features based on context
 *
 * @param processor Phonological processor
 * @return true on success, false on error
 */
bool phonological_plan_coarticulation(phonological_processor_t* processor);

//=============================================================================
// STATUS AND QUERY
//=============================================================================

/**
 * @brief Get processing status
 *
 * @param processor Phonological processor
 * @return Current processing status
 */
phonological_status_t phonological_get_status(const phonological_processor_t* processor);

/**
 * @brief Check if ready for articulation
 *
 * WHAT: Determines if processing is complete
 * WHY:  Signal when ready for motor execution
 * HOW:  Checks if syllables generated and prosody applied
 *
 * @param processor Phonological processor
 * @return true if ready for articulation, false otherwise
 */
bool phonological_is_ready(const phonological_processor_t* processor);

/**
 * @brief Get configuration
 *
 * @param processor Phonological processor
 * @param output Output configuration structure
 * @return true on success, false on NULL input
 */
bool phonological_get_config(const phonological_processor_t* processor,
                             phonological_config_t* output);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PHONOLOGICAL_H */
