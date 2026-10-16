//=============================================================================
// nimcp_phase_coded_buffer.h - Phase-Coded Working Memory Buffer
//=============================================================================

#ifndef NIMCP_PHASE_CODED_BUFFER_H
#define NIMCP_PHASE_CODED_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_phase_coded_buffer.h
 * @brief Phase-coded working memory buffer with neural phasor support
 *
 * WHAT: Working memory buffer that stores items with phase tags for ordering
 * WHY:  Hippocampus uses theta phase to encode item order in working memory
 * HOW:  Complex phasors tag each item; coherence determines retrieval order
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Theta Phase Precession in Working Memory:
 *   ┌─────────────────────────────────────────────────────────────┐
 *   │  Item sequence encoded in theta phase (6-10 Hz):            │
 *   │                                                              │
 *   │    Item 1    Item 2    Item 3    Item 4                     │
 *   │      ↓         ↓         ↓         ↓                        │
 *   │   Phase 0°   Phase 90°  Phase 180° Phase 270°               │
 *   │      │         │         │         │                        │
 *   │    ╱─╲       ╱─╲       ╱─╲       ╱─╲                       │
 *   │   ╱   ╲     ╱   ╲     ╱   ╲     ╱   ╲   Theta (8 Hz)       │
 *   │  ╱     ╲   ╱     ╲   ╱     ╲   ╱     ╲                     │
 *   │         ╲ ╱       ╲ ╱       ╲ ╱       ╲ ╱                   │
 *   │                                                              │
 *   │  Earlier items = earlier phases within theta cycle          │
 *   │  Later items = later phases within theta cycle              │
 *   │  Phase coherence determines memory strength                 │
 *   └──────────────────────────────────────────────────────────────┘
 *
 * OPERATIONS:
 * - Store: Add item with phase tag (auto-incremented or explicit)
 * - Retrieve: Get items in phase order (natural sequence recall)
 * - Pattern Match: Find items by phase coherence
 * - Reorder: Resort by phase relationships
 *
 * PERFORMANCE:
 * - Store: O(1) with phase tagging
 * - Retrieve by phase: O(1) with index lookup
 * - Pattern match: O(N) with vectorized coherence
 * - Memory: 12 bytes per item (4 bytes data + 8 bytes phasor)
 */

// ============================================================================
// CONSTANTS
// ============================================================================

#define PHASE_BUFFER_DEFAULT_CAPACITY 64    // Items
#define PHASE_BUFFER_MAX_CAPACITY 4096      // Max items

// ============================================================================
// TYPES
// ============================================================================

/**
 * @brief Phase-coded buffer item
 */
typedef struct {
    float data;                 // Item value
    float phase;                // Phase tag (radians, -π to π)
    float amplitude;            // Phasor amplitude (memory strength)
    double timestamp_ms;        // Storage timestamp
} phase_coded_item_t;

/**
 * @brief Pattern matching result
 */
typedef struct {
    uint32_t* indices;          // Matched item indices
    float* coherences;          // Coherence with pattern
    uint32_t count;             // Number of matches
    float mean_coherence;       // Mean coherence of matches
} phase_pattern_match_t;

/**
 * @brief Phase buffer configuration
 */
typedef struct {
    uint32_t capacity;          // Maximum items
    bool auto_phase_increment;  // Auto-increment phase on store
    float phase_increment;      // Phase increment per item (radians)
    float theta_frequency_hz;   // Theta frequency for phase cycling (default 8 Hz)
    bool enable_coherence_sort; // Sort by phase coherence on retrieval
} phase_buffer_config_t;

/**
 * @brief Opaque phase-coded buffer handle
 */
typedef struct phase_coded_buffer phase_coded_buffer_t;

// ============================================================================
// API
// ============================================================================

/**
 * @brief Create phase-coded buffer
 *
 * WHAT: Initialize phase-coded working memory buffer
 * WHY:  Enable neural-realistic item sequencing
 * HOW:  Allocate buffer with phasor storage
 *
 * @param config Buffer configuration (NULL for defaults)
 * @return Buffer handle or NULL on failure
 */
phase_coded_buffer_t* phase_buffer_create(const phase_buffer_config_t* config);

/**
 * @brief Destroy phase-coded buffer
 */
void phase_buffer_destroy(phase_coded_buffer_t* buffer);

/**
 * @brief Store item with automatic phase tagging
 *
 * WHAT: Add item to buffer with auto-incremented phase
 * WHY:  Preserve temporal order via phase
 * HOW:  Assign next phase in sequence
 *
 * @param buffer Buffer handle
 * @param data Item value
 * @param amplitude Memory strength (1.0 = strong)
 * @param timestamp_ms Storage time
 * @return true on success, false on error (buffer full)
 */
bool phase_buffer_store(phase_coded_buffer_t* buffer,
                        float data,
                        float amplitude,
                        double timestamp_ms);

/**
 * @brief Store item with explicit phase
 *
 * WHAT: Add item with specific phase tag
 * WHY:  Enable custom phase relationships
 * HOW:  Use provided phase directly
 *
 * @param buffer Buffer handle
 * @param data Item value
 * @param phase Phase tag (radians)
 * @param amplitude Memory strength
 * @param timestamp_ms Storage time
 * @return true on success, false on error
 */
bool phase_buffer_store_with_phase(phase_coded_buffer_t* buffer,
                                    float data,
                                    float phase,
                                    float amplitude,
                                    double timestamp_ms);

/**
 * @brief Retrieve items in phase order
 *
 * WHAT: Get items sorted by phase (temporal sequence)
 * WHY:  Natural recall follows phase precession
 * HOW:  Sort by phase angle, return ordered array
 *
 * @param buffer Buffer handle
 * @param items Output item array (pre-allocated)
 * @param max_items Maximum items to retrieve
 * @param num_retrieved Output: actual items retrieved
 * @return true on success, false on error
 */
bool phase_buffer_retrieve_ordered(const phase_coded_buffer_t* buffer,
                                    phase_coded_item_t* items,
                                    uint32_t max_items,
                                    uint32_t* num_retrieved);

/**
 * @brief Find items matching phase pattern
 *
 * WHAT: Pattern-match based on phase coherence
 * WHY:  Associate items by phase relationships
 * HOW:  Compute coherence with pattern phasor array
 *
 * @param buffer Buffer handle
 * @param pattern_phases Array of pattern phases
 * @param pattern_count Number of pattern elements
 * @param min_coherence Minimum coherence threshold [0, 1]
 * @param result Output match results (caller owns memory)
 * @return true on success, false on error
 */
bool phase_buffer_pattern_match(const phase_coded_buffer_t* buffer,
                                 const float* pattern_phases,
                                 uint32_t pattern_count,
                                 float min_coherence,
                                 phase_pattern_match_t* result);

/**
 * @brief Compute buffer coherence
 *
 * WHAT: Measure overall phase consistency
 * WHY:  Assess memory organization quality
 * HOW:  Inter-trial phase coherence (ITPC)
 *
 * @param buffer Buffer handle
 * @return Coherence value [0, 1] (1 = perfect alignment)
 */
float phase_buffer_coherence(const phase_coded_buffer_t* buffer);

/**
 * @brief Get mean phase of buffer contents
 *
 * WHAT: Circular mean of all item phases
 * WHY:  Identify buffer's dominant phase
 * HOW:  Phasor averaging
 *
 * @param buffer Buffer handle
 * @return Mean phase in radians (-π to π)
 */
float phase_buffer_mean_phase(const phase_coded_buffer_t* buffer);

/**
 * @brief Clear buffer contents
 *
 * WHAT: Remove all items, reset state
 * WHY:  Prepare for new sequence
 * HOW:  Zero count, reset phase counter
 */
void phase_buffer_clear(phase_coded_buffer_t* buffer);

/**
 * @brief Get buffer statistics
 *
 * @param buffer Buffer handle
 * @param count Output: current item count
 * @param capacity Output: maximum capacity
 * @param mean_coherence Output: mean coherence
 * @return true on success, false on error
 */
bool phase_buffer_get_stats(const phase_coded_buffer_t* buffer,
                             uint32_t* count,
                             uint32_t* capacity,
                             float* mean_coherence);

/**
 * @brief Get default configuration
 */
phase_buffer_config_t phase_buffer_default_config(void);

/**
 * @brief Free pattern match result
 */
void phase_pattern_match_free(phase_pattern_match_t* result);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PHASE_CODED_BUFFER_H
