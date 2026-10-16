//=============================================================================
// nimcp_sequence_detector.h - Temporal Sequence Detection
//=============================================================================

#ifndef NIMCP_SEQUENCE_DETECTOR_H
#define NIMCP_SEQUENCE_DETECTOR_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/encoding/nimcp_positional_encoding.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_sequence_detector.h
 * @brief Detect temporal sequences in neural activity
 *
 * WHAT: Real-time detection of learned temporal patterns in spike trains
 * WHY:  Sequences encode temporal structure, predictions, and episodic memories
 * HOW:  N-gram matching, seq_template correlation, and replay detection
 *
 * BIOLOGICAL BASIS:
 * - Hippocampal place cells replay sequences during rest (Wilson & McNaughton, 1994)
 * - Temporal coding in cortical minicolumns for sequence learning
 * - Forward/backward replay indicates memory consolidation
 * - Sequence compression (temporal credit assignment) for learning
 *
 * ALGORITHMS:
 * - N-gram detection (bi-grams, tri-grams of neuron firing order)
 * - Template matching (learned sequences with temporal tolerance)
 * - Replay detection (forward/backward/time-compressed)
 * - Sequence strength scoring (0-1 based on temporal precision)
 */

// ============================================================================
// CONSTANTS
// ============================================================================

#define SEQUENCE_MAX_LENGTH 100              // Maximum sequence length
#define SEQUENCE_MAX_TEMPLATES 1000          // Maximum learned templates
#define SEQUENCE_TEMPORAL_TOLERANCE_MS 50.0f // Timing tolerance for matching
#define SEQUENCE_MIN_STRENGTH 0.5f           // Minimum match strength
#define SEQUENCE_MAX_NGRAM 5                 // Maximum N-gram size

// ============================================================================
// TYPES
// ============================================================================

/**
 * @brief Sequence element (neuron ID + relative timing + position encoding)
 *
 * WHAT: Single element in temporal sequence with positional information
 * WHY:  Position embeddings enable temporal context-aware pattern matching
 * HOW:  Store both temporal and positional encoding for each element
 *
 * BIOLOGICAL BASIS:
 * - Hippocampal phase precession encodes position in theta cycles
 * - Temporal order information crucial for sequence replay detection
 */
typedef struct {
    uint32_t neuron_id;        // Neuron identifier
    float relative_time_ms;    // Time offset from sequence start
    float* position_embedding; // Position encoding vector (NULL if not using PE)
    uint32_t embedding_dim;    // Dimension of position embedding
} sequence_element_t;

/**
 * @brief Learned sequence seq_template
 */
typedef struct {
    sequence_element_t* elements;  // Sequence elements
    uint32_t length;                // Number of elements
    float duration_ms;              // Total sequence duration
    uint32_t observations;          // Number of times observed
    float avg_strength;             // Average match strength
    uint32_t template_id;           // Unique identifier
} sequence_template_t;

/**
 * @brief Sequence detection result
 */
typedef struct {
    uint32_t template_id;          // Matched seq_template ID
    float strength;                 // Match strength [0.0, 1.0]
    double start_time_ms;           // Sequence start time
    double end_time_ms;             // Sequence end time
    bool is_forward;                // Forward replay
    bool is_backward;               // Backward replay
    float compression_factor;       // Time compression (1.0 = normal speed)
    uint32_t matched_elements;      // Number of elements matched
    uint32_t total_elements;        // Total elements in seq_template
} sequence_detection_t;

/**
 * @brief N-gram pattern
 */
typedef struct {
    uint32_t neurons[SEQUENCE_MAX_NGRAM];  // Neuron sequence
    uint32_t length;                        // N-gram length
    uint32_t count;                         // Observation count
    float avg_interval_ms;                  // Average inter-spike interval
} ngram_pattern_t;

/**
 * @brief Sequence detector configuration
 *
 * WHAT: Configuration for sequence detection with positional encoding support
 * WHY:  Enable position-aware pattern matching for temporal sequences
 * HOW:  Add PE type selection and dimensionality configuration
 */
typedef struct {
    uint32_t max_templates;          // Maximum learned sequences
    uint32_t max_sequence_length;    // Maximum elements per sequence
    float temporal_tolerance_ms;     // Timing tolerance for matching
    float min_strength_threshold;    // Minimum match strength
    uint32_t max_ngram;              // Maximum N-gram size (2-5)
    bool enable_replay_detection;    // Detect forward/backward replay
    bool enable_ngram_learning;      // Learn N-gram patterns
    bool enable_compression;         // Detect time-compressed replays

    // Positional Encoding Configuration
    bool enable_positional_encoding; // Enable position-aware matching
    nimcp_pos_encoding_type_t pe_type; // PE type (ROTARY or RELATIVE)
    uint32_t pe_embedding_dim;       // Position embedding dimension
    float pe_similarity_weight;      // Weight for PE similarity [0-1]

    // Quantum Acceleration Configuration
    bool enable_quantum_matching;    // Enable quantum-accelerated pattern matching
    float quantum_match_threshold;   // Threshold for quantum match acceptance [0-1]
} sequence_detector_config_t;

/**
 * @brief Opaque sequence detector handle
 */
typedef struct sequence_detector sequence_detector_t;

// ============================================================================
// API
// ============================================================================

/**
 * @brief Create sequence detector with configuration
 *
 * WHAT: Initialize sequence detection system
 * WHY:  Set up seq_template storage and matching engine
 * HOW:  Allocate seq_template database and spike buffer
 *
 * @param config Detector configuration (NULL for defaults)
 * @return Detector handle or NULL on failure
 */
sequence_detector_t* sequence_detector_create(const sequence_detector_config_t* config);

/**
 * @brief Destroy sequence detector and free resources
 */
void sequence_detector_destroy(sequence_detector_t* detector);

/**
 * @brief Add spike event for sequence analysis
 *
 * WHAT: Record spike for real-time sequence matching
 * WHY:  Build temporal pattern buffer for seq_template matching
 * HOW:  Add to sliding window, trigger seq_template matching
 *
 * @param detector Detector handle
 * @param neuron_id Neuron identifier
 * @param timestamp_ms Spike time in milliseconds
 * @return true on success, false on error
 */
bool sequence_detector_add_spike(sequence_detector_t* detector,
                                  uint32_t neuron_id,
                                  double timestamp_ms);

/**
 * @brief Learn sequence seq_template from spike pattern
 *
 * WHAT: Add new sequence to seq_template library
 * WHY:  Enable detection of this pattern in future activity
 * HOW:  Store sequence with temporal structure and tolerance
 *
 * @param detector Detector handle
 * @param elements Sequence elements (neuron IDs and timings)
 * @param length Number of elements
 * @param template_id Output: assigned seq_template ID
 * @return true on success, false on error
 */
bool sequence_detector_learn_template(sequence_detector_t* detector,
                                       const sequence_element_t* elements,
                                       uint32_t length,
                                       uint32_t* template_id);

/**
 * @brief Detect sequences in recent activity
 *
 * WHAT: Match recent spikes against learned templates
 * WHY:  Identify known temporal patterns in real-time
 * HOW:  Sliding window seq_template correlation with temporal tolerance
 *
 * @param detector Detector handle
 * @param detections Output array for detected sequences
 * @param max_detections Maximum detections to return
 * @param num_detected Output: number of sequences detected
 * @return true on success, false on error
 */
bool sequence_detector_detect(sequence_detector_t* detector,
                               sequence_detection_t* detections,
                               uint32_t max_detections,
                               uint32_t* num_detected);

/**
 * @brief Get learned seq_template by ID
 *
 * @param detector Detector handle
 * @param template_id Template identifier
 * @param seq_template Output: seq_template data
 * @return true on success, false if not found
 */
bool sequence_detector_get_template(const sequence_detector_t* detector,
                                     uint32_t template_id,
                                     sequence_template_t* seq_template);

/**
 * @brief Get all learned N-grams
 *
 * WHAT: Retrieve frequently occurring N-gram patterns
 * WHY:  Identify common subsequences for analysis
 * HOW:  Sort by observation count, return top patterns
 *
 * @param detector Detector handle
 * @param ngrams Output array for N-gram patterns
 * @param max_ngrams Maximum patterns to return
 * @param num_ngrams Output: number of patterns returned
 * @return true on success, false on error
 */
bool sequence_detector_get_ngrams(const sequence_detector_t* detector,
                                   ngram_pattern_t* ngrams,
                                   uint32_t max_ngrams,
                                   uint32_t* num_ngrams);

/**
 * @brief Reset detector state
 *
 * WHAT: Clear spike buffer and detection state
 * WHY:  Start fresh analysis (keeps learned templates)
 * HOW:  Zero buffers, maintain seq_template library
 */
void sequence_detector_reset(sequence_detector_t* detector);

/**
 * @brief Clear all learned templates
 *
 * WHAT: Remove all learned sequences
 * WHY:  Start learning from scratch
 * HOW:  Free seq_template storage, reset counters
 */
void sequence_detector_clear_templates(sequence_detector_t* detector);

/**
 * @brief Get detector statistics
 *
 * @param detector Detector handle
 * @param num_templates Output: number of learned templates
 * @param total_detections Output: lifetime detections
 * @param avg_strength Output: average match strength
 * @return true on success, false on error
 */
bool sequence_detector_get_stats(const sequence_detector_t* detector,
                                  uint32_t* num_templates,
                                  uint64_t* total_detections,
                                  float* avg_strength);

/**
 * @brief Get default configuration
 */
sequence_detector_config_t sequence_detector_default_config(void);

/**
 * @brief Configure positional encoding for sequence detector
 *
 * WHAT: Set up position-aware pattern matching system
 * WHY:  Enable temporal context discrimination in sequence detection
 * HOW:  Create PE encoder with specified type and configuration
 *
 * BIOLOGICAL BASIS:
 * - Hippocampal theta phase codes temporal position in sequences
 * - Grid cells provide spatial and temporal context encoding
 * - Phase precession enables position-dependent firing patterns
 *
 * @param detector Detector handle
 * @param pe_config Positional encoding configuration
 * @return true on success, false on error
 */
bool sequence_detector_set_pe_config(sequence_detector_t* detector,
                                      const nimcp_pos_config_t* pe_config);

/**
 * @brief Apply positional encoding to sequence template
 *
 * WHAT: Encode template elements with position information
 * WHY:  Enable position-aware matching against learned sequences
 * HOW:  Apply RoPE or Relative encoding to each element in template
 *
 * BIOLOGICAL BASIS:
 * - Sequence order encoded via relative timing and phase
 * - Replay detection benefits from position-dependent representations
 * - Temporal context critical for memory consolidation
 *
 * @param detector Detector handle
 * @param template_id Template to encode
 * @return true on success, false on error
 */
bool sequence_detector_encode_template(sequence_detector_t* detector,
                                        uint32_t template_id);

/**
 * @brief Match sequences using position-aware similarity
 *
 * WHAT: Detect sequences with temporal position context
 * WHY:  Discriminate similar sequences differing in temporal structure
 * HOW:  Combined temporal + positional encoding similarity scoring
 *
 * BIOLOGICAL BASIS:
 * - Hippocampal sequences distinguished by phase timing
 * - Forward/backward replay uses temporal order information
 * - Position-dependent firing enables sequence disambiguation
 *
 * ALGORITHM:
 * - Compute temporal match score (existing algorithm)
 * - Compute PE cosine similarity for matched elements
 * - Combined score = (1-w)*temporal + w*positional
 *   where w = pe_similarity_weight
 *
 * @param detector Detector handle
 * @param detections Output array for detected sequences
 * @param max_detections Maximum detections to return
 * @param num_detected Output: number of sequences detected
 * @return true on success, false on error
 */
bool sequence_detector_match_with_pe(sequence_detector_t* detector,
                                      sequence_detection_t* detections,
                                      uint32_t max_detections,
                                      uint32_t* num_detected);

/**
 * @brief Get positional encoding statistics
 *
 * WHAT: Query PE-enhanced matching performance metrics
 * WHY:  Monitor effectiveness of position-aware detection
 * HOW:  Return PE hit rates, avg similarity, cache stats
 *
 * @param detector Detector handle
 * @param pe_match_rate Output: fraction of matches using PE
 * @param avg_pe_similarity Output: average PE cosine similarity
 * @param pe_cache_hit_rate Output: PE cache hit rate
 * @return true on success, false on error
 */
bool sequence_detector_get_pe_stats(const sequence_detector_t* detector,
                                     float* pe_match_rate,
                                     float* avg_pe_similarity,
                                     float* pe_cache_hit_rate);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SEQUENCE_DETECTOR_H
