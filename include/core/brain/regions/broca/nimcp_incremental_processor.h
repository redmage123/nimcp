/**
 * @file nimcp_incremental_processor.h
 * @brief Incremental speech processing for real-time operation
 *
 * WHAT: Real-time streaming phoneme and word processing
 * WHY:  Enable responsive, natural-sounding speech production
 * HOW:  Process speech in small increments with pipeline buffering
 *
 * ARCHITECTURE:
 * - Input Buffer: Accumulates incoming phonemes/words
 * - Incremental Parser: Processes input as it arrives
 * - Commit Manager: Manages what has been finalized
 * - Output Buffer: Holds ready-to-output segments
 *
 * BIOLOGICAL BASIS:
 * - Models incremental nature of human speech production
 * - Left inferior frontal gyrus involvement
 * - Working memory integration for buffer management
 *
 * KEY CONCEPTS:
 * - Incremental Unit: Smallest processable chunk
 * - Commitment: Point at which output is finalized
 * - Lookahead: Future context consideration
 * - Revision: Ability to revise uncommitted content
 *
 * @version Phase B4: Speech Enhancement
 * @date 2026-01-15
 */

#ifndef NIMCP_INCREMENTAL_PROCESSOR_H
#define NIMCP_INCREMENTAL_PROCESSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

typedef struct incremental_processor incremental_processor_t;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define INCREMENTAL_DEFAULT_BUFFER_SIZE     256
#define INCREMENTAL_DEFAULT_LOOKAHEAD         3
#define INCREMENTAL_DEFAULT_COMMIT_DELAY_MS  50
#define INCREMENTAL_MAX_REVISION_DEPTH        5

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Processing unit types
 */
typedef enum {
    UNIT_TYPE_PHONEME = 0,
    UNIT_TYPE_SYLLABLE,
    UNIT_TYPE_WORD,
    UNIT_TYPE_PHRASE,
    UNIT_TYPE_COUNT
} incremental_unit_type_t;

/**
 * @brief Unit status
 */
typedef enum {
    UNIT_STATUS_PENDING = 0,     /**< In buffer, not processed */
    UNIT_STATUS_PROCESSING,      /**< Currently being processed */
    UNIT_STATUS_TENTATIVE,       /**< Processed but uncommitted */
    UNIT_STATUS_COMMITTED,       /**< Finalized, cannot revise */
    UNIT_STATUS_REVISED          /**< Was revised/replaced */
} unit_status_t;

/**
 * @brief Processing status
 */
typedef enum {
    INCREMENTAL_STATUS_IDLE = 0,
    INCREMENTAL_STATUS_BUFFERING,
    INCREMENTAL_STATUS_PROCESSING,
    INCREMENTAL_STATUS_OUTPUTTING,
    INCREMENTAL_STATUS_ERROR
} incremental_status_t;

typedef enum {
    INCREMENTAL_ERROR_NONE = 0,
    INCREMENTAL_ERROR_INVALID_INPUT,
    INCREMENTAL_ERROR_BUFFER_FULL,
    INCREMENTAL_ERROR_REVISION_FAILED,
    INCREMENTAL_ERROR_INTERNAL
} incremental_error_t;

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief Configuration
 */
typedef struct {
    uint32_t input_buffer_size;      /**< Input buffer capacity */
    uint32_t output_buffer_size;     /**< Output buffer capacity */
    uint32_t lookahead_units;        /**< Units to look ahead */
    uint32_t commit_delay_ms;        /**< Delay before committing */
    bool enable_revision;            /**< Allow revisions */
    bool enable_bio_async;           /**< Bio-async messaging */
    incremental_unit_type_t default_unit; /**< Default unit type */
} incremental_config_t;

/**
 * @brief Incremental unit
 */
typedef struct {
    uint32_t unit_id;                /**< Unique identifier */
    incremental_unit_type_t type;    /**< Unit type */
    unit_status_t status;            /**< Current status */
    char content[64];                /**< Unit content */
    uint64_t arrival_time_ms;        /**< When it arrived */
    uint64_t commit_time_ms;         /**< When committed (0 if not) */
    float confidence;                /**< Processing confidence */
} incremental_unit_t;

/**
 * @brief Output result
 */
typedef struct {
    incremental_unit_t* units;       /**< Output units */
    uint32_t unit_count;             /**< Number of units */
    bool is_final;                   /**< Is this final output */
    uint64_t timestamp_ms;           /**< Output timestamp */
} incremental_output_t;

/**
 * @brief Revision record
 */
typedef struct {
    uint32_t original_id;            /**< Original unit ID */
    uint32_t revised_id;             /**< New unit ID */
    char original_content[64];       /**< What it was */
    char revised_content[64];        /**< What it became */
    uint64_t revision_time_ms;       /**< When revised */
} revision_record_t;

/**
 * @brief Statistics
 */
typedef struct {
    uint64_t units_received;
    uint64_t units_committed;
    uint64_t units_revised;
    uint64_t revisions_failed;
    double avg_commit_latency_ms;
    double avg_processing_time_ms;
    float revision_rate;
} incremental_stats_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

incremental_config_t incremental_default_config(void);
incremental_processor_t* incremental_create(const incremental_config_t* config);
void incremental_destroy(incremental_processor_t* processor);
bool incremental_reset(incremental_processor_t* processor);

/*=============================================================================
 * INPUT FUNCTIONS
 *===========================================================================*/

/**
 * @brief Add unit to input buffer
 */
uint32_t incremental_add_unit(
    incremental_processor_t* processor,
    const char* content,
    incremental_unit_type_t type,
    uint64_t timestamp_ms
);

/**
 * @brief Add phoneme to buffer
 */
uint32_t incremental_add_phoneme(
    incremental_processor_t* processor,
    uint8_t phoneme,
    uint64_t timestamp_ms
);

/**
 * @brief Add word to buffer
 */
uint32_t incremental_add_word(
    incremental_processor_t* processor,
    const char* word,
    uint64_t timestamp_ms
);

/**
 * @brief Signal end of input
 */
bool incremental_end_input(incremental_processor_t* processor);

/*=============================================================================
 * PROCESSING FUNCTIONS
 *===========================================================================*/

/**
 * @brief Process buffered input
 */
bool incremental_process(
    incremental_processor_t* processor,
    uint64_t current_time_ms
);

/**
 * @brief Force commit pending units
 */
bool incremental_force_commit(incremental_processor_t* processor);

/**
 * @brief Get ready output
 */
bool incremental_get_output(
    incremental_processor_t* processor,
    incremental_output_t* output
);

/**
 * @brief Free output resources
 */
void incremental_free_output(incremental_output_t* output);

/*=============================================================================
 * REVISION FUNCTIONS
 *===========================================================================*/

/**
 * @brief Revise uncommitted unit
 */
bool incremental_revise_unit(
    incremental_processor_t* processor,
    uint32_t unit_id,
    const char* new_content
);

/**
 * @brief Get revision history
 */
uint32_t incremental_get_revisions(
    const incremental_processor_t* processor,
    revision_record_t* revisions,
    uint32_t max_revisions
);

/**
 * @brief Check if unit can be revised
 */
bool incremental_can_revise(
    const incremental_processor_t* processor,
    uint32_t unit_id
);

/*=============================================================================
 * BUFFER MANAGEMENT
 *===========================================================================*/

/**
 * @brief Get pending unit count
 */
uint32_t incremental_get_pending_count(const incremental_processor_t* processor);

/**
 * @brief Get committed unit count
 */
uint32_t incremental_get_committed_count(const incremental_processor_t* processor);

/**
 * @brief Clear uncommitted units
 */
bool incremental_clear_pending(incremental_processor_t* processor);

/*=============================================================================
 * STATUS AND STATISTICS
 *===========================================================================*/

incremental_status_t incremental_get_status(const incremental_processor_t* processor);
incremental_error_t incremental_get_last_error(const incremental_processor_t* processor);
bool incremental_get_stats(const incremental_processor_t* processor, incremental_stats_t* stats);
void incremental_reset_stats(incremental_processor_t* processor);
bool incremental_get_config(const incremental_processor_t* processor, incremental_config_t* config);

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

bool incremental_register_bio_handler(
    incremental_processor_t* processor,
    bio_router_t* router
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INCREMENTAL_PROCESSOR_H */
