/**
 * @file nimcp_wm_transfer.h
 * @brief Phase M3: Working Memory to Engram Transfer System
 *
 * WHAT: Manages transfer of information from working memory to long-term memory (engrams)
 * WHY:  Implements selective consolidation based on rehearsal, attention, and emotion
 * HOW:  Multi-factor scoring determines which working memory items become engrams
 *
 * BIOLOGICAL BASIS:
 * - Atkinson-Shiffrin model (1968): Working memory → Long-term memory transfer
 * - Baddeley & Hitch (1974): Working memory capacity limitations (7±2 items)
 * - Miller's law (1956): Limited capacity requires selective transfer
 * - Rehearsal strengthens transfer probability
 * - Attention determines encoding priority
 * - Emotional arousal enhances consolidation (McGaugh, 2000)
 *
 * INTEGRATION POINTS:
 * - Phase 10.1 Working Memory: Source of temporary information
 * - Phase M1 Engrams: Destination for transferred memories
 * - Emotional Tagging: Enhances encoding of salient items
 * - Brain Learning Pipeline: Adds learned items to working memory
 * - Brain Cognitive Pipeline: Updates attention and triggers transfer
 *
 * @version Phase M3 Working Memory Transfer
 * @date 2025-11-13
 */

#ifndef NIMCP_WM_TRANSFER_H
#define NIMCP_WM_TRANSFER_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/memory/nimcp_engram.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @struct wm_transfer_criteria_t
 * @brief Criteria for determining when to transfer working memory to engrams
 *
 * WHAT: Multi-factor thresholds for transfer decisions
 * WHY:  Not all working memory items should become long-term memories
 * HOW:  Items must meet sufficient criteria across multiple factors
 */
typedef struct {
    uint32_t rehearsal_threshold;     // Min rehearsals for transfer (default: 3)
    float attention_threshold;         // Min attention for transfer (0.5)
    float emotional_threshold;         // Min emotional salience (0.3)
    uint64_t time_threshold_ms;       // Min time in WM for transfer (5000ms)
    float decay_rate;                 // Decay per second (0.1)
} wm_transfer_criteria_t;

/**
 * @struct wm_transfer_stats_t
 * @brief Statistics for working memory transfer system
 *
 * WHAT: Tracks transfer operations and triggers
 * WHY:  Monitoring and debugging transfer behavior
 * HOW:  Counters updated during transfer evaluation
 */
typedef struct {
    uint64_t total_transfers;         // Total items transferred to engrams
    uint64_t rehearsal_triggered;     // Transfers triggered by rehearsal
    uint64_t attention_triggered;     // Transfers triggered by attention
    uint64_t emotion_triggered;       // Transfers triggered by emotion
    uint64_t time_triggered;          // Transfers triggered by time
    uint64_t total_decayed;           // Items that decayed without transfer
    uint32_t current_wm_items;        // Current working memory item count
} wm_transfer_stats_t;

/**
 * @struct wm_transfer_system_t
 * @brief System managing working memory to engram transfers
 *
 * WHAT: Central coordinator for WM → LTM transfer
 * WHY:  Implements selective consolidation mechanism
 * HOW:  Evaluates criteria and triggers engram encoding
 */
typedef struct {
    wm_transfer_criteria_t criteria;  // Transfer decision criteria
    wm_transfer_stats_t stats;        // System statistics

    // System references (not owned by this system)
    void* working_memory;             // Working memory system (Phase 10.1)
    engram_system_t* engram_system;   // Engram system (Phase M1)
    void* emotional_system;           // Emotional tagging system

    // Internal state
    float* last_attention_weights;    // Track attention over time
    uint32_t attention_weight_count;  // Number of attention weights
    uint64_t last_update_time_ms;     // Last system update time

    // Unified memory integration (CoW support for brain cloning)
    void* mem_manager;                /**< unified_mem_manager_t */
    void* attention_handle;           /**< CoW handle for attention weights */

    // Bio-async integration
    void* bio_ctx;                    /**< bio_module_context_t pointer */
    bool bio_async_enabled;           /**< Bio-async registration status */

} wm_transfer_system_t;

//=============================================================================
// System Management API
//=============================================================================

/**
 * @brief Create working memory transfer system
 * @return New system, or NULL on failure
 *
 * WHAT: Initialize transfer system with default criteria
 * WHY:  Prepare system for managing WM → engram transfers
 * HOW:  Allocate system, set default thresholds, initialize stats
 */
wm_transfer_system_t* wm_transfer_create(void);

/**
 * @brief Destroy working memory transfer system
 * @param system System to destroy (can be NULL)
 *
 * WHAT: Clean up transfer system and free resources
 * WHY:  Prevent memory leaks when brain is destroyed
 * HOW:  Free attention tracking arrays and system struct
 */
void wm_transfer_destroy(wm_transfer_system_t* system);

/**
 * @brief Reset transfer system (clear stats, keep criteria)
 * @param system System to reset
 *
 * WHAT: Clear statistics while preserving configuration
 * WHY:  Allow reuse of system with fresh state
 * HOW:  Zero stats, reset attention tracking
 */
void wm_transfer_reset(wm_transfer_system_t* system);

//=============================================================================
// Integration API
//=============================================================================

/**
 * @brief Connect to working memory system
 * @param system Transfer system
 * @param working_memory Working memory system (Phase 10.1)
 *
 * WHAT: Link transfer system to working memory source
 * WHY:  Transfer system needs access to WM items
 * HOW:  Store pointer (not owned) to working memory
 */
void wm_transfer_set_working_memory(
    wm_transfer_system_t* system,
    void* working_memory);

/**
 * @brief Connect to engram system
 * @param system Transfer system
 * @param engram_system Engram system (Phase M1)
 *
 * WHAT: Link transfer system to engram destination
 * WHY:  Transferred items must be encoded as engrams
 * HOW:  Store pointer (not owned) to engram system
 */
void wm_transfer_set_engram_system(
    wm_transfer_system_t* system,
    engram_system_t* engram_system);

/**
 * @brief Connect to emotional tagging system
 * @param system Transfer system
 * @param emotional_system Emotional tagging system
 *
 * WHAT: Link transfer system to emotional salience source
 * WHY:  Emotional arousal enhances encoding
 * HOW:  Store pointer (not owned) to emotional system
 */
void wm_transfer_set_emotional_system(
    wm_transfer_system_t* system,
    void* emotional_system);

//=============================================================================
// Transfer Operations API
//=============================================================================

/**
 * @brief Evaluate working memory items for transfer to engrams
 * @param system Transfer system
 * @param time_delta_seconds Time since last evaluation
 * @return Number of items transferred
 *
 * WHAT: Check WM items against transfer criteria, transfer if met
 * WHY:  Implements selective consolidation to long-term memory
 * HOW:  Score each item on rehearsal/attention/emotion/time, transfer if >= 0.5
 *
 * SCORING ALGORITHM:
 * - Rehearsal contribution: 40% weight (if threshold met)
 * - Attention contribution: 30% weight (if threshold met)
 * - Emotional contribution: 20% weight (if threshold met)
 * - Time contribution: 10% weight (if threshold met)
 * - Total score >= 0.5 triggers transfer
 */
uint32_t wm_transfer_evaluate(
    wm_transfer_system_t* system,
    float time_delta_seconds);

/**
 * @brief Force transfer of specific working memory item
 * @param system Transfer system
 * @param wm_slot Working memory slot index
 * @return true if transferred, false otherwise
 *
 * WHAT: Manually trigger transfer of specific item
 * WHY:  Allow explicit encoding (e.g., important events)
 * HOW:  Bypass normal criteria, directly encode to engram
 */
bool wm_transfer_force_item(
    wm_transfer_system_t* system,
    uint32_t wm_slot);

/**
 * @brief Update attention weights for working memory items
 * @param system Transfer system
 * @param attention_weights Attention for each WM slot (0.0-1.0)
 * @param count Number of weights
 *
 * WHAT: Update which items are receiving attention
 * WHY:  Attention determines transfer priority
 * HOW:  Store weights array for use in transfer evaluation
 */
void wm_transfer_update_attention(
    wm_transfer_system_t* system,
    const float* attention_weights,
    uint32_t count);

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Set transfer criteria
 * @param system Transfer system
 * @param criteria New criteria to use
 *
 * WHAT: Update transfer decision thresholds
 * WHY:  Allow customization of transfer behavior
 * HOW:  Copy criteria struct to system
 */
void wm_transfer_set_criteria(
    wm_transfer_system_t* system,
    const wm_transfer_criteria_t* criteria);

/**
 * @brief Get current transfer criteria
 * @param system Transfer system
 * @param criteria_out Output for current criteria
 *
 * WHAT: Retrieve current transfer thresholds
 * WHY:  Allow inspection of configuration
 * HOW:  Copy criteria from system to output
 */
void wm_transfer_get_criteria(
    const wm_transfer_system_t* system,
    wm_transfer_criteria_t* criteria_out);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get transfer statistics
 * @param system Transfer system
 * @param stats_out Output for statistics
 *
 * WHAT: Retrieve transfer operation statistics
 * WHY:  Monitoring and debugging transfer behavior
 * HOW:  Copy stats from system to output
 */
void wm_transfer_get_statistics(
    const wm_transfer_system_t* system,
    wm_transfer_stats_t* stats_out);

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Get default transfer criteria
 * @return Default criteria based on neuroscience literature
 *
 * DEFAULT VALUES:
 * - Rehearsal threshold: 3 (3+ rehearsals trigger transfer)
 * - Attention threshold: 0.5 (50% attention required)
 * - Emotional threshold: 0.3 (30% emotional salience)
 * - Time threshold: 5000ms (5 seconds in working memory)
 * - Decay rate: 0.1 (10% decay per second)
 */
wm_transfer_criteria_t wm_transfer_get_default_criteria(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_WM_TRANSFER_H
