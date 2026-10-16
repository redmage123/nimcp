/**
 * @file nimcp_hyperthymesia.h
 * @brief Superhuman autobiographical memory enhancement module
 *
 * WHAT: Provides hyperthymesia-like perfect autobiographical memory recall
 * WHY:  Enable superhuman episodic memory capabilities with date-indexed retrieval
 * HOW:  Hierarchical temporal encoding, emotional tagging, vivid re-experiencing
 *
 * ARCHITECTURE:
 * - Date-indexed memory storage with hierarchical temporal resolution
 * - Emotional valence and arousal tagging for memory consolidation
 * - Contextual feature encoding for rich episodic content
 * - Vivid re-experiencing through pattern reactivation
 * - Autobiographical timeline navigation
 *
 * BIOLOGICAL BASIS:
 * - Models hyperthymestic syndrome (Highly Superior Autobiographical Memory)
 * - Enhanced hippocampal-prefrontal connectivity
 * - Heightened amygdala involvement for emotional memories
 * - Superior temporal pattern binding
 * - Eidetic-like sensory memory traces
 *
 * @version Phase T12: Superhuman Enhancement Modules
 * @date 2026-01-13
 */

#ifndef NIMCP_HYPERTHYMESIA_H
#define NIMCP_HYPERTHYMESIA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*=============================================================================
 * ERROR CODES
 *===========================================================================*/

/**
 * @brief Hyperthymesia module error codes
 *
 * NOTE: These are module-local error codes specific to the hyperthymesia
 * subsystem. They should NOT be compared with nimcp_error_t values.
 * Use hyperthymesia_error_string() to convert to human-readable form.
 */
typedef enum {
    HYPERTHYMESIA_ERROR_NONE = 0,
    HYPERTHYMESIA_ERROR_INVALID_INPUT,
    HYPERTHYMESIA_ERROR_ENCODING_FAILED,
    HYPERTHYMESIA_ERROR_RETRIEVAL_FAILED,
    HYPERTHYMESIA_ERROR_DATE_NOT_FOUND,
    HYPERTHYMESIA_ERROR_MEMORY_FULL,
    HYPERTHYMESIA_ERROR_TIMELINE_CORRUPTION,
    HYPERTHYMESIA_ERROR_REEXPERIENCE_FAILED,
    HYPERTHYMESIA_ERROR_CONTEXT_MISMATCH,
    HYPERTHYMESIA_ERROR_TEMPORAL_OVERFLOW,
    HYPERTHYMESIA_ERROR_NOT_INITIALIZED,
    HYPERTHYMESIA_ERROR_INTERNAL
} hyperthymesia_error_t;

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Processing status
 */
typedef enum {
    HYPERTHYMESIA_STATUS_IDLE = 0,
    HYPERTHYMESIA_STATUS_ENCODING,
    HYPERTHYMESIA_STATUS_RETRIEVING,
    HYPERTHYMESIA_STATUS_NAVIGATING,
    HYPERTHYMESIA_STATUS_REEXPERIENCING,
    HYPERTHYMESIA_STATUS_CONSOLIDATING,
    HYPERTHYMESIA_STATUS_READY,
    HYPERTHYMESIA_STATUS_ERROR
} hyperthymesia_status_t;

/**
 * @brief Temporal resolution levels for hierarchical indexing
 */
typedef enum {
    TEMPORAL_RESOLUTION_SECOND = 0,
    TEMPORAL_RESOLUTION_MINUTE,
    TEMPORAL_RESOLUTION_HOUR,
    TEMPORAL_RESOLUTION_DAY,
    TEMPORAL_RESOLUTION_WEEK,
    TEMPORAL_RESOLUTION_MONTH,
    TEMPORAL_RESOLUTION_YEAR,
    TEMPORAL_RESOLUTION_DECADE,
    TEMPORAL_RESOLUTION_COUNT
} temporal_resolution_t;

/**
 * @brief Memory vividness levels
 */
typedef enum {
    VIVIDNESS_FAINT = 0,
    VIVIDNESS_MODERATE,
    VIVIDNESS_CLEAR,
    VIVIDNESS_VIVID,
    VIVIDNESS_EIDETIC
} memory_vividness_t;

/**
 * @brief Autobiographical memory types
 */
typedef enum {
    AUTOBIO_TYPE_EVENT = 0,
    AUTOBIO_TYPE_FACT,
    AUTOBIO_TYPE_EMOTIONAL,
    AUTOBIO_TYPE_SENSORY,
    AUTOBIO_TYPE_NARRATIVE,
    AUTOBIO_TYPE_FLASHBULB
} autobiographical_type_t;

/**
 * @brief Re-experience modality
 */
typedef enum {
    REEXPERIENCE_VISUAL = 0,
    REEXPERIENCE_AUDITORY,
    REEXPERIENCE_OLFACTORY,
    REEXPERIENCE_TACTILE,
    REEXPERIENCE_GUSTATORY,
    REEXPERIENCE_EMOTIONAL,
    REEXPERIENCE_FULL_IMMERSION,
    REEXPERIENCE_MODALITY_COUNT
} reexperience_modality_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Default configuration values
 */
#define HYPERTHYMESIA_DEFAULT_MEMORY_CAPACITY       100000
#define HYPERTHYMESIA_DEFAULT_CONTEXT_DIM           256
#define HYPERTHYMESIA_DEFAULT_SENSORY_DIM           128
#define HYPERTHYMESIA_DEFAULT_EMOTIONAL_DIM         32
#define HYPERTHYMESIA_DEFAULT_ENCODING_STRENGTH     0.9f
#define HYPERTHYMESIA_DEFAULT_DECAY_RATE            0.0001f
#define HYPERTHYMESIA_DEFAULT_REEXPERIENCE_DEPTH    0.8f
#define HYPERTHYMESIA_DEFAULT_TIMELINE_YEARS        100

/**
 * @brief Hyperthymesia module configuration
 */
typedef struct {
    /* Capacity limits */
    uint32_t memory_capacity;            /**< Maximum episodic memories */
    uint32_t timeline_years;             /**< Years of timeline to maintain */

    /* Feature dimensions */
    uint32_t context_dim;                /**< Contextual feature dimension */
    uint32_t sensory_dim;                /**< Sensory feature dimension per modality */
    uint32_t emotional_dim;              /**< Emotional state dimension */

    /* Encoding parameters */
    float encoding_strength;             /**< Base encoding strength [0, 1] */
    float decay_rate;                    /**< Memory decay rate (minimal for hyperthymesia) */
    float consolidation_threshold;       /**< Threshold for permanent storage */

    /* Re-experience parameters */
    float reexperience_depth;            /**< Depth of re-experiencing [0, 1] */
    float vividness_threshold;           /**< Minimum vividness for retrieval */
    bool enable_full_immersion;          /**< Allow full sensory re-experiencing */

    /* Timeline features */
    bool enable_date_indexing;           /**< Date-based memory indexing */
    bool enable_emotional_tagging;       /**< Emotional memory enhancement */
    bool enable_sensory_traces;          /**< Sensory modality traces */
    bool enable_narrative_binding;       /**< Narrative thread connections */

    /* Processing options */
    bool enable_parallel_retrieval;      /**< Parallel date range searches */
    bool enable_context_matching;        /**< Context-based memory matching */
    uint32_t max_concurrent_recalls;     /**< Max simultaneous recalls */
} hyperthymesia_config_t;

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief Precise date-time structure
 */
typedef struct {
    uint16_t year;                       /**< Year (e.g., 2026) */
    uint8_t month;                       /**< Month (1-12) */
    uint8_t day;                         /**< Day (1-31) */
    uint8_t hour;                        /**< Hour (0-23) */
    uint8_t minute;                      /**< Minute (0-59) */
    uint8_t second;                      /**< Second (0-59) */
    uint16_t millisecond;                /**< Millisecond (0-999) */
    int8_t timezone_offset;              /**< Timezone offset in hours */
} hyperthymesia_datetime_t;

/**
 * @brief Emotional state tag
 */
typedef struct {
    float valence;                       /**< Emotional valence [-1, 1] */
    float arousal;                       /**< Emotional arousal [0, 1] */
    float dominance;                     /**< Feeling of control [0, 1] */
    float surprise;                      /**< Unexpectedness [0, 1] */
    float significance;                  /**< Personal significance [0, 1] */
} emotional_state_t;

/**
 * @brief Sensory trace for a specific modality
 */
typedef struct {
    reexperience_modality_t modality;    /**< Sensory modality */
    float* features;                     /**< Sensory feature vector */
    uint32_t feature_count;              /**< Number of features */
    float intensity;                     /**< Sensory intensity [0, 1] */
    float clarity;                       /**< Trace clarity [0, 1] */
} sensory_trace_t;

/**
 * @brief Contextual features of an episodic memory
 */
typedef struct {
    float* spatial_context;              /**< Where it happened */
    uint32_t spatial_dim;
    float* social_context;               /**< Who was present */
    uint32_t social_dim;
    float* activity_context;             /**< What was being done */
    uint32_t activity_dim;
    float* semantic_context;             /**< Meaning and interpretation */
    uint32_t semantic_dim;
} memory_context_t;

/**
 * @brief Autobiographical episodic memory entry
 */
typedef struct {
    uint64_t memory_id;                  /**< Unique memory identifier */
    hyperthymesia_datetime_t timestamp;  /**< Precise time of encoding */
    autobiographical_type_t type;        /**< Memory type */
    memory_vividness_t vividness;        /**< Current vividness level */

    /* Content */
    float* core_features;                /**< Core memory features */
    uint32_t core_feature_count;
    memory_context_t context;            /**< Contextual information */

    /* Emotional tag */
    emotional_state_t emotion;           /**< Emotional state at encoding */

    /* Sensory traces */
    sensory_trace_t* sensory_traces;     /**< Multi-modal sensory traces */
    uint32_t trace_count;

    /* Metadata */
    float encoding_strength;             /**< Initial encoding strength */
    float current_strength;              /**< Current memory strength */
    uint32_t retrieval_count;            /**< Times memory was retrieved */
    uint64_t last_retrieval_ms;          /**< Last retrieval timestamp */

    /* Narrative connections */
    uint64_t* linked_memories;           /**< Related memory IDs */
    uint32_t link_count;
    char narrative_tag[64];              /**< Narrative thread identifier */
} autobiographical_memory_t;

/**
 * @brief Re-experience result structure
 */
typedef struct {
    uint64_t memory_id;                  /**< Memory being re-experienced */
    memory_vividness_t achieved_vividness; /**< Achieved vividness level */
    float immersion_depth;               /**< Depth of immersion [0, 1] */

    /* Reactivated content */
    float* reactivated_features;         /**< Reactivated core features */
    uint32_t feature_count;
    emotional_state_t reactivated_emotion; /**< Reactivated emotional state */
    sensory_trace_t* reactivated_senses; /**< Reactivated sensory traces */
    uint32_t reactivated_trace_count;

    /* Quality metrics */
    float temporal_accuracy;             /**< Temporal localization accuracy */
    float contextual_richness;           /**< Context reconstruction quality */
    float emotional_intensity;           /**< Emotional re-experience intensity */
} reexperience_result_t;

/**
 * @brief Date range query structure
 */
typedef struct {
    hyperthymesia_datetime_t start;      /**< Range start */
    hyperthymesia_datetime_t end;        /**< Range end */
    temporal_resolution_t resolution;    /**< Query resolution */
    autobiographical_type_t* type_filter; /**< Optional type filter */
    uint32_t type_filter_count;
    float min_vividness;                 /**< Minimum vividness threshold */
    float min_emotional_significance;    /**< Minimum emotional significance */
} date_query_t;

/**
 * @brief Query result structure
 */
typedef struct {
    autobiographical_memory_t* memories; /**< Retrieved memories */
    uint32_t count;                      /**< Number of memories */
    float* relevance_scores;             /**< Relevance to query */
    hyperthymesia_datetime_t query_start; /**< Query range start */
    hyperthymesia_datetime_t query_end;  /**< Query range end */
    uint64_t query_time_us;              /**< Query execution time */
} retrieval_result_t;

/**
 * @brief Timeline navigation cursor
 */
typedef struct {
    hyperthymesia_datetime_t current;    /**< Current position in timeline */
    temporal_resolution_t zoom_level;    /**< Current temporal resolution */
    uint64_t* visible_memories;          /**< Memory IDs in current view */
    uint32_t visible_count;
    int32_t scroll_offset;               /**< Navigation offset */
} timeline_cursor_t;

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Module statistics
 */
typedef struct {
    /* Memory counts */
    uint64_t total_memories;             /**< Total memories encoded */
    uint64_t active_memories;            /**< Currently stored memories */
    uint64_t flashbulb_memories;         /**< Flashbulb memory count */

    /* Operation counts */
    uint64_t encodings_performed;        /**< Total encoding operations */
    uint64_t retrievals_performed;       /**< Total retrieval operations */
    uint64_t reexperiences_performed;    /**< Total re-experience operations */
    uint64_t timeline_navigations;       /**< Timeline navigation count */

    /* Quality metrics */
    float avg_encoding_strength;         /**< Average encoding strength */
    float avg_vividness;                 /**< Average memory vividness */
    float avg_retrieval_accuracy;        /**< Average retrieval accuracy */
    float avg_reexperience_depth;        /**< Average re-experience depth */

    /* Temporal coverage */
    hyperthymesia_datetime_t earliest;   /**< Earliest memory date */
    hyperthymesia_datetime_t latest;     /**< Latest memory date */
    uint32_t years_covered;              /**< Years with memories */

    /* Performance metrics */
    float avg_encoding_time_us;          /**< Average encoding time */
    float avg_retrieval_time_us;         /**< Average retrieval time */
    float avg_reexperience_time_us;      /**< Average re-experience time */

    /* Resource usage */
    size_t memory_used_bytes;            /**< Total memory usage */
    size_t sensory_traces_bytes;         /**< Sensory trace storage */
} hyperthymesia_stats_t;

/*=============================================================================
 * CALLBACK TYPES
 *===========================================================================*/

/**
 * @brief Callback for memory encoding completion
 */
typedef void (*hyperthymesia_encode_callback_t)(
    uint64_t memory_id,
    float encoding_strength,
    void* user_data
);

/**
 * @brief Callback for re-experience events
 */
typedef void (*hyperthymesia_reexperience_callback_t)(
    const reexperience_result_t* result,
    void* user_data
);

/**
 * @brief Callback for timeline navigation events
 */
typedef void (*hyperthymesia_navigation_callback_t)(
    const timeline_cursor_t* cursor,
    void* user_data
);

/*=============================================================================
 * OPAQUE TYPE
 *===========================================================================*/

/**
 * @brief Opaque hyperthymesia module handle
 */
typedef struct hyperthymesia_module hyperthymesia_module_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Provide starting point for customization
 * HOW:  Initialize all fields with hyperthymesia-optimized values
 *
 * @return Default configuration structure
 */
hyperthymesia_config_t hyperthymesia_default_config(void);

/**
 * @brief Create hyperthymesia module
 *
 * WHAT: Allocate and initialize the module
 * WHY:  Enable superhuman autobiographical memory
 * HOW:  Create temporal index, allocate memory stores, initialize timeline
 *
 * @param config Configuration (NULL for defaults)
 * @return New module instance, or NULL on failure
 */
hyperthymesia_module_t* hyperthymesia_create(const hyperthymesia_config_t* config);

/**
 * @brief Destroy hyperthymesia module
 *
 * WHAT: Free all resources
 * WHY:  Prevent memory leaks
 * HOW:  Destroy index structures, free memory stores
 *
 * @param module Module to destroy
 */
void hyperthymesia_destroy(hyperthymesia_module_t* module);

/**
 * @brief Reset module state
 *
 * WHAT: Clear all memories and reset state
 * WHY:  Allow fresh start without reallocation
 * HOW:  Clear temporal index and memory stores
 *
 * @param module Module instance
 * @return true on success
 */
bool hyperthymesia_reset(hyperthymesia_module_t* module);

/*=============================================================================
 * EPISODIC MEMORY ENCODING
 *===========================================================================*/

/**
 * @brief Encode new autobiographical memory
 *
 * WHAT: Store new episodic memory with full contextual encoding
 * WHY:  Create hyperthymesia-grade memory trace
 * HOW:  Date-index, emotional tag, bind sensory traces
 *
 * @param module Module instance
 * @param timestamp When the memory occurred
 * @param type Memory type classification
 * @param core_features Core experience features
 * @param feature_count Number of core features
 * @param context Contextual information
 * @param emotion Emotional state at encoding
 * @return Memory ID on success, 0 on failure
 */
uint64_t hyperthymesia_encode_memory(
    hyperthymesia_module_t* module,
    const hyperthymesia_datetime_t* timestamp,
    autobiographical_type_t type,
    const float* core_features,
    uint32_t feature_count,
    const memory_context_t* context,
    const emotional_state_t* emotion
);

/**
 * @brief Add sensory trace to existing memory
 *
 * WHAT: Attach sensory modality trace to memory
 * WHY:  Enable multi-modal re-experiencing
 * HOW:  Link sensory features to memory entry
 *
 * @param module Module instance
 * @param memory_id Target memory
 * @param trace Sensory trace to add
 * @return true on success
 */
bool hyperthymesia_add_sensory_trace(
    hyperthymesia_module_t* module,
    uint64_t memory_id,
    const sensory_trace_t* trace
);

/**
 * @brief Encode flashbulb memory
 *
 * WHAT: Create high-priority flashbulb memory
 * WHY:  Model emotionally significant events
 * HOW:  Maximum encoding strength, enhanced sensory binding
 *
 * @param module Module instance
 * @param timestamp When event occurred
 * @param core_features Event features
 * @param feature_count Number of features
 * @param emotion Emotional state (high arousal/significance)
 * @param narrative_tag Event narrative identifier
 * @return Memory ID on success, 0 on failure
 */
uint64_t hyperthymesia_encode_flashbulb(
    hyperthymesia_module_t* module,
    const hyperthymesia_datetime_t* timestamp,
    const float* core_features,
    uint32_t feature_count,
    const emotional_state_t* emotion,
    const char* narrative_tag
);

/**
 * @brief Link related memories in narrative thread
 *
 * WHAT: Connect memories in autobiographical narrative
 * WHY:  Enable narrative-based retrieval
 * HOW:  Create bidirectional links between memories
 *
 * @param module Module instance
 * @param memory_id_1 First memory
 * @param memory_id_2 Second memory
 * @param narrative_tag Optional shared narrative tag
 * @return true on success
 */
bool hyperthymesia_link_memories(
    hyperthymesia_module_t* module,
    uint64_t memory_id_1,
    uint64_t memory_id_2,
    const char* narrative_tag
);

/*=============================================================================
 * DATE-INDEXED RETRIEVAL
 *===========================================================================*/

/**
 * @brief Retrieve memory by exact date
 *
 * WHAT: Find memories at specific date/time
 * WHY:  Core hyperthymesia capability - date-indexed recall
 * HOW:  Hierarchical temporal index lookup
 *
 * @param module Module instance
 * @param datetime Exact date/time to query
 * @param resolution Temporal resolution for matching
 * @param result Output result structure
 * @return true on success
 */
bool hyperthymesia_retrieve_by_date(
    hyperthymesia_module_t* module,
    const hyperthymesia_datetime_t* datetime,
    temporal_resolution_t resolution,
    retrieval_result_t* result
);

/**
 * @brief Query memories in date range
 *
 * WHAT: Retrieve memories within time window
 * WHY:  Enable temporal context exploration
 * HOW:  Range query on temporal index
 *
 * @param module Module instance
 * @param query Date range query specification
 * @param max_results Maximum memories to return
 * @param result Output result structure
 * @return true on success
 */
bool hyperthymesia_query_date_range(
    hyperthymesia_module_t* module,
    const date_query_t* query,
    uint32_t max_results,
    retrieval_result_t* result
);

/**
 * @brief Get memories for specific day of year
 *
 * WHAT: Retrieve memories on same date across years
 * WHY:  Anniversary and recurring event access
 * HOW:  Query all matching month/day combinations
 *
 * @param module Module instance
 * @param month Month (1-12)
 * @param day Day (1-31)
 * @param result Output result structure
 * @return true on success
 */
bool hyperthymesia_retrieve_by_anniversary(
    hyperthymesia_module_t* module,
    uint8_t month,
    uint8_t day,
    retrieval_result_t* result
);

/**
 * @brief Calculate day of week for any date
 *
 * WHAT: Determine weekday for historical date
 * WHY:  Classic hyperthymesia demonstration ability
 * HOW:  Calendar calculation algorithm
 *
 * @param module Module instance
 * @param datetime Date to check
 * @return Day of week (0=Sunday, 6=Saturday), -1 on error
 */
int8_t hyperthymesia_get_day_of_week(
    hyperthymesia_module_t* module,
    const hyperthymesia_datetime_t* datetime
);

/*=============================================================================
 * VIVID RE-EXPERIENCING
 *===========================================================================*/

/**
 * @brief Re-experience memory with full vividness
 *
 * WHAT: Reactivate memory for vivid re-experiencing
 * WHY:  Core hyperthymesia phenomenology
 * HOW:  Pattern reactivation across all encoded modalities
 *
 * @param module Module instance
 * @param memory_id Memory to re-experience
 * @param target_depth Target immersion depth [0, 1]
 * @param result Output re-experience result
 * @return true on success
 */
bool hyperthymesia_reexperience(
    hyperthymesia_module_t* module,
    uint64_t memory_id,
    float target_depth,
    reexperience_result_t* result
);

/**
 * @brief Re-experience specific sensory modality
 *
 * WHAT: Reactivate single sensory trace
 * WHY:  Focused modality-specific recall
 * HOW:  Target modality pattern reactivation
 *
 * @param module Module instance
 * @param memory_id Target memory
 * @param modality Modality to reactivate
 * @param intensity Target intensity [0, 1]
 * @param trace Output sensory trace
 * @return true on success
 */
bool hyperthymesia_reexperience_modality(
    hyperthymesia_module_t* module,
    uint64_t memory_id,
    reexperience_modality_t modality,
    float intensity,
    sensory_trace_t* trace
);

/**
 * @brief Get emotional re-experience
 *
 * WHAT: Reactivate emotional state from memory
 * WHY:  Emotional time travel to past state
 * HOW:  Emotional pattern reactivation
 *
 * @param module Module instance
 * @param memory_id Target memory
 * @param intensity Re-experience intensity [0, 1]
 * @param emotion Output emotional state
 * @return true on success
 */
bool hyperthymesia_reexperience_emotion(
    hyperthymesia_module_t* module,
    uint64_t memory_id,
    float intensity,
    emotional_state_t* emotion
);

/*=============================================================================
 * TIMELINE NAVIGATION
 *===========================================================================*/

/**
 * @brief Create timeline navigation cursor
 *
 * WHAT: Initialize cursor for timeline exploration
 * WHY:  Enable temporal browsing of memories
 * HOW:  Set starting position and zoom level
 *
 * @param module Module instance
 * @param start_date Starting position
 * @param zoom_level Initial temporal resolution
 * @param cursor Output cursor structure
 * @return true on success
 */
bool hyperthymesia_create_cursor(
    hyperthymesia_module_t* module,
    const hyperthymesia_datetime_t* start_date,
    temporal_resolution_t zoom_level,
    timeline_cursor_t* cursor
);

/**
 * @brief Navigate timeline forward/backward
 *
 * WHAT: Move cursor along timeline
 * WHY:  Temporal memory exploration
 * HOW:  Update cursor position by offset units
 *
 * @param module Module instance
 * @param cursor Cursor to update
 * @param offset Units to move (negative = backward)
 * @return true on success
 */
bool hyperthymesia_navigate_timeline(
    hyperthymesia_module_t* module,
    timeline_cursor_t* cursor,
    int32_t offset
);

/**
 * @brief Change timeline zoom level
 *
 * WHAT: Adjust temporal resolution of view
 * WHY:  Hierarchical time exploration
 * HOW:  Change resolution, update visible memories
 *
 * @param module Module instance
 * @param cursor Cursor to update
 * @param new_zoom New zoom level
 * @return true on success
 */
bool hyperthymesia_set_zoom(
    hyperthymesia_module_t* module,
    timeline_cursor_t* cursor,
    temporal_resolution_t new_zoom
);

/**
 * @brief Jump to specific date in timeline
 *
 * WHAT: Move cursor to exact date
 * WHY:  Direct temporal navigation
 * HOW:  Update cursor position directly
 *
 * @param module Module instance
 * @param cursor Cursor to update
 * @param target_date Target position
 * @return true on success
 */
bool hyperthymesia_jump_to_date(
    hyperthymesia_module_t* module,
    timeline_cursor_t* cursor,
    const hyperthymesia_datetime_t* target_date
);

/**
 * @brief Free cursor resources
 *
 * @param cursor Cursor to free
 */
void hyperthymesia_free_cursor(timeline_cursor_t* cursor);

/*=============================================================================
 * MEMORY MANAGEMENT
 *===========================================================================*/

/**
 * @brief Get memory by ID
 *
 * WHAT: Direct memory access
 * WHY:  Precise memory inspection
 * HOW:  ID-based lookup
 *
 * @param module Module instance
 * @param memory_id Memory ID
 * @param memory Output memory structure
 * @return true if found
 */
bool hyperthymesia_get_memory(
    const hyperthymesia_module_t* module,
    uint64_t memory_id,
    autobiographical_memory_t* memory
);

/**
 * @brief Update memory vividness
 *
 * WHAT: Modify memory vividness level
 * WHY:  Model memory enhancement/degradation
 * HOW:  Update vividness and strength fields
 *
 * @param module Module instance
 * @param memory_id Target memory
 * @param vividness New vividness level
 * @return true on success
 */
bool hyperthymesia_update_vividness(
    hyperthymesia_module_t* module,
    uint64_t memory_id,
    memory_vividness_t vividness
);

/**
 * @brief Consolidate memories above threshold
 *
 * WHAT: Mark strong memories as consolidated
 * WHY:  Model memory consolidation
 * HOW:  Scan and update status of strong memories
 *
 * @param module Module instance
 * @param strength_threshold Minimum strength for consolidation
 * @return Number of memories consolidated
 */
uint32_t hyperthymesia_consolidate(
    hyperthymesia_module_t* module,
    float strength_threshold
);

/*=============================================================================
 * CALLBACKS
 *===========================================================================*/

/**
 * @brief Set encoding callback
 *
 * @param module Module instance
 * @param callback Callback function
 * @param user_data User context
 * @return true on success
 */
bool hyperthymesia_set_encode_callback(
    hyperthymesia_module_t* module,
    hyperthymesia_encode_callback_t callback,
    void* user_data
);

/**
 * @brief Set re-experience callback
 *
 * @param module Module instance
 * @param callback Callback function
 * @param user_data User context
 * @return true on success
 */
bool hyperthymesia_set_reexperience_callback(
    hyperthymesia_module_t* module,
    hyperthymesia_reexperience_callback_t callback,
    void* user_data
);

/**
 * @brief Set navigation callback
 *
 * @param module Module instance
 * @param callback Callback function
 * @param user_data User context
 * @return true on success
 */
bool hyperthymesia_set_navigation_callback(
    hyperthymesia_module_t* module,
    hyperthymesia_navigation_callback_t callback,
    void* user_data
);

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

/**
 * @brief Get current status
 *
 * @param module Module instance
 * @return Current status
 */
hyperthymesia_status_t hyperthymesia_get_status(const hyperthymesia_module_t* module);

/**
 * @brief Get last error code
 *
 * @param module Module instance
 * @return Last error code
 */
hyperthymesia_error_t hyperthymesia_get_last_error(const hyperthymesia_module_t* module);

/**
 * @brief Get error description string
 *
 * @param error Error code
 * @return Human-readable description
 */
const char* hyperthymesia_error_string(hyperthymesia_error_t error);

/**
 * @brief Get status description string
 *
 * @param status Status code
 * @return Human-readable description
 */
const char* hyperthymesia_status_string(hyperthymesia_status_t status);

/**
 * @brief Get module statistics
 *
 * @param module Module instance
 * @param stats Output statistics structure
 * @return true on success
 */
bool hyperthymesia_get_stats(const hyperthymesia_module_t* module, hyperthymesia_stats_t* stats);

/**
 * @brief Get module configuration
 *
 * @param module Module instance
 * @param config Output configuration structure
 * @return true on success
 */
bool hyperthymesia_get_config(const hyperthymesia_module_t* module, hyperthymesia_config_t* config);

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

/**
 * @brief Compare two datetime values
 *
 * @param dt1 First datetime
 * @param dt2 Second datetime
 * @return -1 if dt1 < dt2, 0 if equal, 1 if dt1 > dt2
 */
int hyperthymesia_compare_datetime(
    const hyperthymesia_datetime_t* dt1,
    const hyperthymesia_datetime_t* dt2
);

/**
 * @brief Get current datetime
 *
 * @param datetime Output datetime structure
 * @return true on success
 */
bool hyperthymesia_get_current_datetime(hyperthymesia_datetime_t* datetime);

/**
 * @brief Free retrieval result resources
 *
 * @param result Result to free
 */
void hyperthymesia_free_result(retrieval_result_t* result);

/**
 * @brief Free re-experience result resources
 *
 * @param result Result to free
 */
void hyperthymesia_free_reexperience_result(reexperience_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPERTHYMESIA_H */
