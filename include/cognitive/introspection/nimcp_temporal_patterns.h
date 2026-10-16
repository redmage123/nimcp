/**
 * @file nimcp_temporal_patterns.h
 * @brief Temporal Pattern Analysis for Brain Introspection
 *
 * WHAT: Detects, matches, and predicts temporal patterns in brain state evolution
 * WHY:  Metacognition requires understanding recurring patterns of neural activity
 *       to enable prediction, planning, and self-awareness
 * HOW:  Uses Dynamic Time Warping (DTW) for pattern similarity, sliding window
 *       analysis for detection, and historical pattern matching for prediction
 *
 * BIOLOGICAL BASIS:
 * - Hippocampal replay during sleep consolidation (Wilson & McNaughton, 1994)
 * - Pattern completion in CA3 region (Nakazawa et al., 2002)
 * - Predictive coding in cortical hierarchies (Rao & Ballard, 1999)
 * - Memory consolidation through reactivation (Buzsáki, 1989)
 *
 * DESIGN PATTERNS:
 * - Strategy: DTW algorithm for flexible sequence matching
 * - Observer: Callback registration for pattern detection events
 * - Memento: State history maintenance for pattern analysis
 * - Factory: Pattern library creation and management
 *
 * THREAD SAFETY: All functions are thread-safe via mutex protection
 *
 * PERFORMANCE:
 * - Pattern detection: O(n*m*w) where n=patterns, m=window size, w=state dim
 * - Pattern matching: O(m*w) where m=pattern length, w=state dimension
 * - Prediction: O(n*m) where n=library size, m=pattern length
 * - Trend analysis: O(h) where h=history length
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_TEMPORAL_PATTERNS_H
#define NIMCP_TEMPORAL_PATTERNS_H

#include <stdbool.h>
#include <stdint.h>
#include "cognitive/introspection/nimcp_introspection.h"
#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * CONSTANTS
 * ======================================================================== */

/** Default window size for pattern detection (number of states) */
#define TEMPORAL_DEFAULT_WINDOW_SIZE 10

/** Default minimum pattern length */
#define TEMPORAL_DEFAULT_MIN_PATTERN_LENGTH 3

/** Default maximum pattern length */
#define TEMPORAL_DEFAULT_MAX_PATTERN_LENGTH 20

/** Default similarity threshold for pattern matching (0-1) */
#define TEMPORAL_DEFAULT_SIMILARITY_THRESHOLD 0.8F

/** Default maximum patterns to store in library */
#define TEMPORAL_DEFAULT_MAX_PATTERNS 100

/** Default minimum pattern occurrences before registering */
#define TEMPORAL_DEFAULT_MIN_OCCURRENCES 3

/** Default trend analysis window (number of states) */
#define TEMPORAL_DEFAULT_TREND_WINDOW 50

/** Maximum state vector dimension */
#define TEMPORAL_MAX_STATE_DIMENSION 1024

/** Maximum pattern name length */
#define TEMPORAL_MAX_PATTERN_NAME 64

/* ========================================================================
 * TYPE DEFINITIONS
 * ======================================================================== */

/**
 * WHAT: Temporal pattern configuration
 * WHY: Customize pattern detection behavior for different use cases
 * HOW: Parameters control detection sensitivity, library size, and analysis
 */
typedef struct {
    uint32_t window_size;              /**< Sliding window size for detection */
    uint32_t min_pattern_length;       /**< Minimum pattern length to detect */
    uint32_t max_pattern_length;       /**< Maximum pattern length to detect */
    float similarity_threshold;        /**< Similarity threshold (0-1) */
    uint32_t max_patterns;             /**< Maximum patterns in library */
    uint32_t min_occurrences;          /**< Min occurrences to register pattern */
    uint32_t trend_window;             /**< Window for trend analysis */
    bool enable_auto_detection;        /**< Auto-detect patterns in background */
    bool enable_prediction;            /**< Enable pattern-based prediction */
    bool enable_callbacks;             /**< Enable pattern detection callbacks */
} temporal_pattern_config_t;

/**
 * WHAT: Pattern trend direction
 * WHY: Classify long-term evolution of metrics
 * HOW: Statistical analysis of metric values over time
 */
typedef enum {
    TREND_INCREASING,    /**< Metric is increasing (positive slope) */
    TREND_DECREASING,    /**< Metric is decreasing (negative slope) */
    TREND_STABLE,        /**< Metric is stable (near-zero slope) */
    TREND_OSCILLATING,   /**< Metric oscillates (high variance) */
    TREND_UNKNOWN        /**< Insufficient data for trend */
} pattern_trend_direction_t;

/**
 * WHAT: Temporal pattern representation
 * WHY: Store detected recurring sequences of brain states
 * HOW: Sequence of state vectors with metadata
 *
 * MEMORY: Caller must free state_sequence array
 */
typedef struct {
    char name[TEMPORAL_MAX_PATTERN_NAME];  /**< Pattern identifier */
    float** state_sequence;                /**< Sequence of state vectors */
    uint32_t sequence_length;              /**< Number of states in pattern */
    uint32_t state_dimension;              /**< Dimension of each state vector */
    float strength;                        /**< Pattern strength (0-1) */
    uint32_t occurrence_count;             /**< Times pattern has occurred */
    uint64_t first_detected;               /**< Timestamp first detected */
    uint64_t last_detected;                /**< Timestamp last detected */
    float average_duration_ms;             /**< Average duration of pattern */
} temporal_pattern_t;

/**
 * WHAT: Sequence of state transitions
 * WHY: Track ordered brain state changes for pattern analysis
 * HOW: Array of state vectors with timestamps
 *
 * MEMORY: Caller must free states array
 */
typedef struct {
    brain_state_t* states;     /**< Array of brain states */
    uint32_t num_states;       /**< Number of states */
    uint64_t start_time;       /**< Timestamp of first state */
    uint64_t end_time;         /**< Timestamp of last state */
} pattern_sequence_t;

/**
 * WHAT: Pattern match result with confidence
 * WHY: Quantify how well current state matches a pattern
 * HOW: DTW distance normalized to confidence score
 */
typedef struct {
    temporal_pattern_t* matched_pattern;  /**< Matched pattern (NULL if none) */
    float confidence;                     /**< Match confidence (0-1) */
    float dtw_distance;                   /**< Raw DTW distance */
    uint32_t match_offset;                /**< Offset in pattern sequence */
    bool is_complete_match;               /**< Full pattern matched? */
} pattern_match_result_t;

/**
 * WHAT: Long-term trend analysis for a metric
 * WHY: Track how brain behavior evolves over time
 * HOW: Linear regression and variance analysis
 */
typedef struct {
    char metric_name[TEMPORAL_MAX_PATTERN_NAME]; /**< Metric identifier */
    pattern_trend_direction_t direction;         /**< Trend direction */
    float slope;                                 /**< Linear regression slope */
    float r_squared;                             /**< Goodness of fit (0-1) */
    float mean_value;                            /**< Mean metric value */
    float variance;                              /**< Metric variance */
    float min_value;                             /**< Minimum observed value */
    float max_value;                             /**< Maximum observed value */
    uint32_t num_samples;                        /**< Number of data points */
} temporal_trend_t;

/**
 * WHAT: Pattern detection callback function type
 * WHY: Notify applications when patterns are detected
 * HOW: Called when new pattern detected or existing pattern matched
 *
 * @param pattern The detected/matched pattern
 * @param confidence Confidence score (0-1)
 * @param user_data User-provided context
 */
typedef void (*pattern_detected_callback_t)(const temporal_pattern_t* pattern,
                                            float confidence,
                                            void* user_data);

/* ========================================================================
 * CONFIGURATION API
 * ======================================================================== */

/**
 * WHAT: Get default temporal pattern configuration
 * WHY: Sensible defaults for most use cases
 * HOW: Returns pre-configured struct with balanced settings
 *
 * DEFAULT SETTINGS:
 * - Window size: 10 states
 * - Pattern length: 3-20 states
 * - Similarity threshold: 0.8
 * - Max patterns: 100
 * - Min occurrences: 3
 * - Trend window: 50 states
 * - Auto-detection: enabled
 * - Prediction: enabled
 * - Callbacks: disabled
 *
 * @return Default configuration struct
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
temporal_pattern_config_t temporal_pattern_default_config(void);

/* ========================================================================
 * PATTERN DETECTION API
 * ======================================================================== */

/**
 * WHAT: Detect recurring patterns in activity history
 * WHY: Identify common sequences of brain states
 * HOW: Sliding window + DTW clustering + occurrence counting
 *
 * ALGORITHM:
 * 1. Extract state sequences from history using sliding window
 * 2. Compute DTW distance between all sequence pairs
 * 3. Cluster similar sequences (distance < threshold)
 * 4. Extract representative pattern from each cluster
 * 5. Filter by minimum occurrence count
 *
 * @param context Introspection context
 * @param config Detection configuration (NULL for defaults)
 * @param num_patterns Output: number of patterns detected
 * @return Array of detected patterns (must be freed with pattern_array_free)
 *
 * ERRORS: Returns NULL if context is NULL or detection fails
 *
 * COMPLEXITY: O(n^2 * m * d) where n=windows, m=length, d=dimension
 * THREAD-SAFE: Yes
 * TIME: ~10-100ms for typical histories
 */
temporal_pattern_t* introspection_detect_patterns(introspection_context_t context,
                                                   const temporal_pattern_config_t* config,
                                                   uint32_t* num_patterns);

/**
 * WHAT: Check if current state matches a known pattern
 * WHY: Identify when brain is exhibiting learned behavior
 * HOW: DTW distance between current window and pattern
 *
 * @param context Introspection context
 * @param pattern Pattern to match against
 * @param config Configuration (NULL for defaults)
 * @return Match result with confidence score
 *
 * ERRORS: Returns zero confidence if context or pattern is NULL
 *
 * COMPLEXITY: O(m * d) where m=pattern length, d=dimension
 * THREAD-SAFE: Yes
 */
pattern_match_result_t introspection_match_pattern(introspection_context_t context,
                                                    const temporal_pattern_t* pattern,
                                                    const temporal_pattern_config_t* config);

/**
 * WHAT: Predict next brain state based on pattern matching
 * WHY: Enable anticipation and planning
 * HOW: Find best matching pattern, return next expected state
 *
 * ALGORITHM:
 * 1. Get recent state history from context
 * 2. Match against all patterns in library
 * 3. Find pattern with highest confidence
 * 4. Return next state in that pattern's sequence
 *
 * @param context Introspection context
 * @param config Configuration (NULL for defaults)
 * @return Predicted next state (must be freed with brain_state_free)
 *
 * ERRORS: Returns empty state if no matching pattern found
 *
 * COMPLEXITY: O(n * m * d) where n=library size, m=pattern length
 * THREAD-SAFE: Yes
 */
brain_state_t introspection_predict_next_state(introspection_context_t context,
                                                const temporal_pattern_config_t* config);

/**
 * WHAT: Analyze long-term trend for a metric
 * WHY: Track brain evolution over extended periods
 * HOW: Linear regression on metric values from history
 *
 * METRICS SUPPORTED:
 * - "avg_activation": Average neuron activation
 * - "max_activation": Maximum neuron activation
 * - "num_active": Active neuron count
 * - "energy": Energy consumption estimate
 * - "entropy": State entropy
 *
 * @param context Introspection context
 * @param metric_name Name of metric to analyze
 * @param config Configuration (NULL for defaults)
 * @return Trend analysis result
 *
 * ERRORS: Returns TREND_UNKNOWN if metric not found or insufficient data
 *
 * COMPLEXITY: O(h) where h=history length
 * THREAD-SAFE: Yes
 */
temporal_trend_t introspection_get_trend(introspection_context_t context,
                                          const char* metric_name,
                                          const temporal_pattern_config_t* config);

/* ========================================================================
 * PATTERN LIBRARY API
 * ======================================================================== */

/**
 * WHAT: Register a known pattern in the pattern library
 * WHY: Store patterns for future matching and prediction
 * HOW: Add pattern to introspection context's library
 *
 * @param context Introspection context
 * @param pattern Pattern to register
 * @return true on success, false on error
 *
 * ERRORS: Returns false if context or pattern is NULL, or library is full
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool introspection_register_pattern(introspection_context_t context,
                                     const temporal_pattern_t* pattern);

/**
 * WHAT: Clear all patterns from the pattern library
 * WHY: Reset library for fresh pattern learning or testing
 * HOW: Free all library entries and reset count
 *
 * @param context Introspection context
 *
 * COMPLEXITY: O(n) where n=library size
 * THREAD-SAFE: Yes
 */
void introspection_clear_pattern_library(introspection_context_t context);

/**
 * WHAT: Get all patterns in the pattern library
 * WHY: Inspect learned patterns
 * HOW: Return copy of library contents
 *
 * @param context Introspection context
 * @param num_patterns Output: number of patterns
 * @return Array of patterns (must be freed with pattern_array_free)
 *
 * ERRORS: Returns NULL if context is NULL
 *
 * COMPLEXITY: O(n) where n=library size
 * THREAD-SAFE: Yes
 */
temporal_pattern_t* introspection_get_pattern_library(introspection_context_t context,
                                                       uint32_t* num_patterns);

/**
 * WHAT: Compare two patterns for similarity
 * WHY: Measure pattern distance for clustering
 * HOW: Average DTW distance across state sequences
 *
 * @param pattern1 First pattern
 * @param pattern2 Second pattern
 * @return Similarity score (0=different, 1=identical)
 *
 * ERRORS: Returns 0.0 if patterns are NULL or incompatible dimensions
 *
 * COMPLEXITY: O(m * d) where m=pattern length, d=dimension
 * THREAD-SAFE: Yes
 */
float introspection_pattern_similarity(const temporal_pattern_t* pattern1,
                                        const temporal_pattern_t* pattern2);

/* ========================================================================
 * BRAIN INTEGRATION API
 * ======================================================================== */

/**
 * WHAT: Enable automatic pattern detection on brain
 * WHY: Continuously monitor and learn patterns
 * HOW: Register callback to detect patterns after each brain step
 *
 * @param brain Brain instance
 * @param config Detection configuration (NULL for defaults)
 * @return true on success, false on error
 *
 * ERRORS: Returns false if brain is NULL or introspection not available
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool brain_enable_pattern_detection(brain_t brain,
                                     const temporal_pattern_config_t* config);

/**
 * WHAT: Get currently active patterns in brain
 * WHY: See which patterns are currently executing
 * HOW: Match recent history against library, return active patterns
 *
 * @param brain Brain instance
 * @param num_patterns Output: number of active patterns
 * @return Array of active patterns (must be freed with pattern_array_free)
 *
 * ERRORS: Returns NULL if brain is NULL
 *
 * COMPLEXITY: O(n * m * d) where n=library size
 * THREAD-SAFE: Yes
 */
temporal_pattern_t* brain_get_active_patterns(brain_t brain, uint32_t* num_patterns);

/**
 * WHAT: Register callback for pattern detection events
 * WHY: React to pattern detection in real-time
 * HOW: Store callback, invoke on pattern detection
 *
 * @param brain Brain instance
 * @param callback Callback function
 * @param user_data User context passed to callback
 * @return true on success, false on error
 *
 * ERRORS: Returns false if brain is NULL
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool brain_on_pattern_detected(brain_t brain,
                                pattern_detected_callback_t callback,
                                void* user_data);

/* ========================================================================
 * MEMORY MANAGEMENT
 * ======================================================================== */

/**
 * WHAT: Free temporal pattern structure
 * WHY: Release allocated state sequences
 * HOW: Free sequence array, zero struct
 *
 * @param pattern Pattern to free
 *
 * SAFETY: Safe to call with NULL
 *
 * COMPLEXITY: O(m) where m=pattern length
 * THREAD-SAFE: Yes
 */
void temporal_pattern_free(temporal_pattern_t* pattern);

/**
 * WHAT: Free array of patterns
 * WHY: Release pattern library allocations
 * HOW: Free each pattern, free array
 *
 * @param patterns Pattern array
 * @param num_patterns Number of patterns
 *
 * SAFETY: Safe to call with NULL
 *
 * COMPLEXITY: O(n*m) where n=num patterns, m=pattern length
 * THREAD-SAFE: Yes
 */
void pattern_array_free(temporal_pattern_t* patterns, uint32_t num_patterns);

/**
 * WHAT: Free pattern sequence structure
 * WHY: Release state array
 * HOW: Free states array, zero struct
 *
 * @param sequence Sequence to free
 *
 * SAFETY: Safe to call with NULL
 *
 * COMPLEXITY: O(n) where n=sequence length
 * THREAD-SAFE: Yes
 */
void pattern_sequence_free(pattern_sequence_t* sequence);

/**
 * WHAT: Free pattern match result
 * WHY: Release matched pattern if allocated
 * HOW: Free pattern, zero struct
 *
 * @param result Match result to free
 *
 * SAFETY: Safe to call with NULL
 *
 * COMPLEXITY: O(1) - matched_pattern is a pointer, not owned
 * THREAD-SAFE: Yes
 */
void pattern_match_result_free(pattern_match_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TEMPORAL_PATTERNS_H */
