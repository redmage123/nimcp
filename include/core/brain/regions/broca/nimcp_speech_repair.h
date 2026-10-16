/**
 * @file nimcp_speech_repair.h
 * @brief Speech repair and disfluency handling
 *
 * WHAT: Detection and processing of speech repairs, restarts, and corrections
 * WHY:  Handle natural disfluencies in speech production and comprehension
 * HOW:  Identify repair patterns and maintain coherent output
 *
 * ARCHITECTURE:
 * - Disfluency Detector: Identifies hesitations, false starts
 * - Repair Classifier: Categorizes repair types
 * - Edit Manager: Handles insertions, deletions, substitutions
 * - Fluency Restorer: Produces clean output
 *
 * BIOLOGICAL BASIS:
 * - Basal ganglia role in speech monitoring
 * - Anterior cingulate cortex error detection
 * - Left prefrontal cortex in repair planning
 *
 * REPAIR TYPES:
 * - Restart: "I want the... I want the red one"
 * - Replacement: "Turn left... right at the corner"
 * - Insertion: "The big... the big red ball"
 * - Deletion: "I need to... get some milk"
 * - Correction: "It's on Monday... Tuesday"
 *
 * @version Phase B4: Speech Enhancement
 * @date 2026-01-15
 */

#ifndef NIMCP_SPEECH_REPAIR_H
#define NIMCP_SPEECH_REPAIR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

typedef struct speech_repair speech_repair_t;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define REPAIR_DEFAULT_MAX_REPAIRS           32
#define REPAIR_DEFAULT_HISTORY_SIZE          64
#define REPAIR_DEFAULT_DETECTION_WINDOW_MS  500
#define REPAIR_MAX_REPARANDUM_LENGTH        128
#define REPAIR_MAX_ALTERATION_LENGTH        128

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Repair types
 */
typedef enum {
    REPAIR_TYPE_NONE = 0,
    REPAIR_TYPE_RESTART,         /**< Fresh start of same content */
    REPAIR_TYPE_REPLACEMENT,     /**< Substitute different content */
    REPAIR_TYPE_INSERTION,       /**< Add missing content */
    REPAIR_TYPE_DELETION,        /**< Remove erroneous content */
    REPAIR_TYPE_CORRECTION,      /**< Explicit error correction */
    REPAIR_TYPE_REPETITION,      /**< Simple repetition (not error) */
    REPAIR_TYPE_COUNT
} repair_type_t;

/**
 * @brief Disfluency types
 */
typedef enum {
    DISFLUENCY_NONE = 0,
    DISFLUENCY_FILLED_PAUSE,     /**< "um", "uh", "er" */
    DISFLUENCY_SILENT_PAUSE,     /**< Extended silence */
    DISFLUENCY_LENGTHENING,      /**< Sound lengthening */
    DISFLUENCY_WORD_FRAGMENT,    /**< Incomplete word */
    DISFLUENCY_REPETITION,       /**< Word/phrase repeat */
    DISFLUENCY_FALSE_START,      /**< Abandoned utterance */
    DISFLUENCY_COUNT
} disfluency_type_t;

/**
 * @brief Edit signal types (markers of repair)
 */
typedef enum {
    EDIT_SIGNAL_NONE = 0,
    EDIT_SIGNAL_PAUSE,           /**< Pause indicates edit */
    EDIT_SIGNAL_FILLER,          /**< Filler word */
    EDIT_SIGNAL_CUE_PHRASE,      /**< "I mean", "sorry" */
    EDIT_SIGNAL_INTONATION,      /**< Prosodic break */
    EDIT_SIGNAL_COUNT
} edit_signal_t;

/**
 * @brief Processing status
 */
typedef enum {
    REPAIR_STATUS_IDLE = 0,
    REPAIR_STATUS_DETECTING,
    REPAIR_STATUS_PROCESSING,
    REPAIR_STATUS_READY,
    REPAIR_STATUS_ERROR
} repair_status_t;

typedef enum {
    REPAIR_ERROR_NONE = 0,
    REPAIR_ERROR_INVALID_INPUT,
    REPAIR_ERROR_BUFFER_FULL,
    REPAIR_ERROR_DETECTION_FAILED,
    REPAIR_ERROR_INTERNAL
} repair_error_t;

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief Configuration
 */
typedef struct {
    uint32_t max_repairs;            /**< Maximum repairs to track */
    uint32_t history_size;           /**< History buffer size */
    uint32_t detection_window_ms;    /**< Window for repair detection */
    float pause_threshold_ms;        /**< Pause duration threshold */
    bool enable_auto_correction;     /**< Auto-correct output */
    bool enable_bio_async;           /**< Bio-async messaging */
    bool preserve_disfluencies;      /**< Keep disfluencies in output */
} speech_repair_config_t;

/**
 * @brief Detected disfluency
 */
typedef struct {
    disfluency_type_t type;          /**< Disfluency type */
    uint32_t position;               /**< Position in utterance */
    uint32_t length;                 /**< Length of disfluent segment */
    char content[64];                /**< The disfluent content */
    float confidence;                /**< Detection confidence */
} disfluency_t;

/**
 * @brief Repair structure
 */
typedef struct {
    uint32_t repair_id;              /**< Unique identifier */
    repair_type_t type;              /**< Repair type */

    /* Reparandum: what was repaired */
    char reparandum[REPAIR_MAX_REPARANDUM_LENGTH];
    uint32_t reparandum_start;       /**< Start position */
    uint32_t reparandum_end;         /**< End position */

    /* Edit interval: signal of repair */
    edit_signal_t edit_signal;
    char edit_content[32];           /**< Edit signal content */

    /* Alteration: the correction */
    char alteration[REPAIR_MAX_ALTERATION_LENGTH];
    uint32_t alteration_start;
    uint32_t alteration_end;

    float confidence;                /**< Detection confidence */
    uint64_t timestamp_ms;           /**< When detected */
} repair_instance_t;

/**
 * @brief Repair analysis result
 */
typedef struct {
    /* Disfluencies found */
    disfluency_t disfluencies[8];
    uint32_t disfluency_count;

    /* Repairs found */
    repair_instance_t repairs[4];
    uint32_t repair_count;

    /* Cleaned output */
    char cleaned_output[512];
    bool has_cleaned_output;

    /* Fluency metrics */
    float fluency_score;             /**< 0-1, 1 = perfectly fluent */
    float repair_rate;               /**< Repairs per 100 words */

    double processing_time_ms;
} repair_analysis_t;

/**
 * @brief Statistics
 */
typedef struct {
    uint64_t utterances_processed;
    uint64_t repairs_detected;
    uint64_t disfluencies_detected;
    uint64_t auto_corrections_made;
    double avg_fluency_score;
    double avg_processing_time_ms;
    uint64_t repair_type_counts[REPAIR_TYPE_COUNT];
} repair_stats_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

speech_repair_config_t speech_repair_default_config(void);
speech_repair_t* speech_repair_create(const speech_repair_config_t* config);
void speech_repair_destroy(speech_repair_t* processor);
bool speech_repair_reset(speech_repair_t* processor);

/*=============================================================================
 * DISFLUENCY DETECTION
 *===========================================================================*/

/**
 * @brief Detect disfluencies in utterance
 */
uint32_t speech_repair_detect_disfluencies(
    speech_repair_t* processor,
    const char* utterance,
    disfluency_t* disfluencies,
    uint32_t max_disfluencies
);

/**
 * @brief Check if word is a filled pause
 */
bool speech_repair_is_filler(const char* word);

/**
 * @brief Get disfluency type name
 */
const char* speech_repair_disfluency_name(disfluency_type_t type);

/*=============================================================================
 * REPAIR DETECTION
 *===========================================================================*/

/**
 * @brief Detect repairs in utterance
 */
uint32_t speech_repair_detect_repairs(
    speech_repair_t* processor,
    const char* utterance,
    repair_instance_t* repairs,
    uint32_t max_repairs
);

/**
 * @brief Get repair type name
 */
const char* speech_repair_type_name(repair_type_t type);

/*=============================================================================
 * FULL ANALYSIS
 *===========================================================================*/

/**
 * @brief Perform full repair analysis
 */
bool speech_repair_analyze(
    speech_repair_t* processor,
    const char* utterance,
    repair_analysis_t* analysis
);

/**
 * @brief Get cleaned (fluent) version of utterance
 */
bool speech_repair_clean(
    speech_repair_t* processor,
    const char* utterance,
    char* cleaned,
    size_t cleaned_size
);

/*=============================================================================
 * REPAIR GENERATION (for speech production)
 *===========================================================================*/

/**
 * @brief Generate repair for error
 */
bool speech_repair_generate_correction(
    speech_repair_t* processor,
    const char* original,
    const char* correction,
    repair_type_t type,
    char* output,
    size_t output_size
);

/**
 * @brief Insert disfluency marker
 */
bool speech_repair_insert_hesitation(
    speech_repair_t* processor,
    const char* input,
    uint32_t position,
    disfluency_type_t type,
    char* output,
    size_t output_size
);

/*=============================================================================
 * STATUS AND STATISTICS
 *===========================================================================*/

repair_status_t speech_repair_get_status(const speech_repair_t* processor);
repair_error_t speech_repair_get_last_error(const speech_repair_t* processor);
bool speech_repair_get_stats(const speech_repair_t* processor, repair_stats_t* stats);
void speech_repair_reset_stats(speech_repair_t* processor);
bool speech_repair_get_config(const speech_repair_t* processor, speech_repair_config_t* config);

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

bool speech_repair_register_bio_handler(
    speech_repair_t* processor,
    bio_router_t* router
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SPEECH_REPAIR_H */
