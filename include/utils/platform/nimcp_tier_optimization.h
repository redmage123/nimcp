//=============================================================================
// nimcp_tier_optimization.h - Memory and Resource Optimization by Platform Tier
//=============================================================================
/**
 * @file nimcp_tier_optimization.h
 * @brief Tier-based memory optimization constants and macros
 *
 * WHAT: Centralized tier-based constants for memory footprint reduction
 * WHY:  Enable NIMCP to run on resource-constrained platforms (64MB-8GB RAM)
 * HOW:  Provide compile-time and runtime constants that adapt to platform tier
 *
 * OPTIMIZATION TARGETS:
 * - Bio-async inbox sizing: 150KB savings on MINIMAL tier
 * - History buffer sizing: 50KB savings on MINIMAL tier
 * - Statistics conditionals: 10KB savings when disabled
 * - Mutex pooling: 5.6KB savings with shared pool
 * - Fixed array sizing: 100KB savings with tier-based arrays
 * - String buffer reduction: 25KB savings with smaller buffers
 *
 * TOTAL POTENTIAL SAVINGS: 400-760KB (50-60% reduction)
 *
 * USAGE:
 * @code
 * // Use tier-based constants instead of hardcoded values
 * bio_module_info_t info = {
 *     .inbox_capacity = NIMCP_BIO_INBOX_CAPACITY,  // Tier-optimized
 *     ...
 * };
 *
 * // Conditional statistics
 * #if NIMCP_ENABLE_STATISTICS
 * my_struct.stats = compute_stats();
 * #endif
 * @endcode
 *
 * @author NIMCP Development Team
 * @date 2024-12-22
 * @version 1.0.0
 */

#ifndef NIMCP_TIER_OPTIMIZATION_H
#define NIMCP_TIER_OPTIMIZATION_H

#include "utils/platform/nimcp_platform_tier.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Build-Time Tier Selection
//=============================================================================

/**
 * @brief Current platform tier (can be overridden at compile time)
 *
 * Set via: -DNIMCP_BUILD_TIER=PLATFORM_TIER_MINIMAL_VALUE
 * If not set, defaults to PLATFORM_TIER_FULL
 */
#ifndef NIMCP_BUILD_TIER
    #define NIMCP_BUILD_TIER PLATFORM_TIER_FULL_VALUE
#endif

//=============================================================================
// Feature Enable/Disable Flags
//=============================================================================

/**
 * @brief Statistics collection control
 *
 * WHAT: Enable/disable statistics struct allocation and computation
 * WHY:  Statistics consume 40-100 bytes per struct, ~100 structs = 4-10KB
 * HOW:  Wrap statistics code in #if NIMCP_ENABLE_STATISTICS
 *
 * SAVINGS: ~10KB on MINIMAL tier
 */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_MINIMAL_VALUE
    #ifndef NIMCP_ENABLE_STATISTICS
        #define NIMCP_ENABLE_STATISTICS 0
    #endif
#else
    #ifndef NIMCP_ENABLE_STATISTICS
        #define NIMCP_ENABLE_STATISTICS 1
    #endif
#endif

/**
 * @brief Detailed history tracking control
 *
 * WHAT: Enable/disable detailed history buffers
 * WHY:  History buffers consume 50+ bytes per entry, 100+ entries = 5KB+ each
 * HOW:  Use reduced history sizes when disabled
 *
 * SAVINGS: ~50KB on MINIMAL tier
 */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_MINIMAL_VALUE || NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED_VALUE
    #ifndef NIMCP_ENABLE_DETAILED_HISTORY
        #define NIMCP_ENABLE_DETAILED_HISTORY 0
    #endif
#else
    #ifndef NIMCP_ENABLE_DETAILED_HISTORY
        #define NIMCP_ENABLE_DETAILED_HISTORY 1
    #endif
#endif

/**
 * @brief Mutex pooling control
 *
 * WHAT: Use shared mutex pool instead of per-bridge mutexes
 * WHY:  70+ bridges * 80 bytes = 5.6KB saved
 * HOW:  Route bridge locks through shared pool
 *
 * SAVINGS: ~5KB on MINIMAL tier
 */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_MINIMAL_VALUE
    #ifndef NIMCP_USE_MUTEX_POOL
        #define NIMCP_USE_MUTEX_POOL 1
    #endif
#else
    #ifndef NIMCP_USE_MUTEX_POOL
        #define NIMCP_USE_MUTEX_POOL 0
    #endif
#endif

/**
 * @brief Introspection detail level
 *
 * WHAT: Control introspection subsystem detail
 * WHY:  Full introspection (Phi, temporal patterns, ensemble) is memory-heavy
 * HOW:  Disable detailed introspection on constrained platforms
 *
 * SAVINGS: ~50KB on MINIMAL tier
 */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_MINIMAL_VALUE
    #ifndef NIMCP_ENABLE_INTROSPECTION_DETAILED
        #define NIMCP_ENABLE_INTROSPECTION_DETAILED 0
    #endif
#else
    #ifndef NIMCP_ENABLE_INTROSPECTION_DETAILED
        #define NIMCP_ENABLE_INTROSPECTION_DETAILED 1
    #endif
#endif

/**
 * @brief Shadow emotions and bias detection
 *
 * WHAT: Control advanced emotional/cognitive systems
 * WHY:  These are optional meta-cognitive features
 * HOW:  Disable on constrained platforms
 *
 * SAVINGS: ~30KB on MINIMAL tier
 */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_MINIMAL_VALUE || NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED_VALUE
    #ifndef NIMCP_ENABLE_SHADOW_EMOTIONS
        #define NIMCP_ENABLE_SHADOW_EMOTIONS 0
    #endif
    #ifndef NIMCP_ENABLE_BIAS_DETECTION
        #define NIMCP_ENABLE_BIAS_DETECTION 0
    #endif
#else
    #ifndef NIMCP_ENABLE_SHADOW_EMOTIONS
        #define NIMCP_ENABLE_SHADOW_EMOTIONS 1
    #endif
    #ifndef NIMCP_ENABLE_BIAS_DETECTION
        #define NIMCP_ENABLE_BIAS_DETECTION 1
    #endif
#endif

//=============================================================================
// Bio-Async Inbox Sizing (150KB savings potential)
//=============================================================================

/**
 * @brief Bio-async message inbox capacity per module
 *
 * WHAT: Number of pending messages each bio-async module can queue
 * WHY:  50+ modules * 24 unused slots * 128 bytes = 150KB waste
 * HOW:  Size based on platform tier
 *
 * SIZING:
 * - FULL: 32 messages (4KB per module) - full parallelism
 * - MEDIUM: 16 messages (2KB per module) - moderate parallelism
 * - CONSTRAINED: 8 messages (1KB per module) - limited parallelism
 * - MINIMAL: 4 messages (512B per module) - minimal buffering
 */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_MINIMAL_VALUE
    #define NIMCP_BIO_INBOX_CAPACITY 4
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED_VALUE
    #define NIMCP_BIO_INBOX_CAPACITY 8
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM_VALUE
    #define NIMCP_BIO_INBOX_CAPACITY 16
#else
    #define NIMCP_BIO_INBOX_CAPACITY 256
#endif

/**
 * @brief Bio-async message pool size
 */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_MINIMAL_VALUE
    #define NIMCP_BIO_MESSAGE_POOL_SIZE 64
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED_VALUE
    #define NIMCP_BIO_MESSAGE_POOL_SIZE 256
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM_VALUE
    #define NIMCP_BIO_MESSAGE_POOL_SIZE 512
#else
    #define NIMCP_BIO_MESSAGE_POOL_SIZE 1024
#endif

//=============================================================================
// History Buffer Sizing (50KB savings potential)
//=============================================================================

/**
 * @brief Generic history buffer size
 */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_MINIMAL_VALUE
    #define NIMCP_HISTORY_SIZE_SMALL 4
    #define NIMCP_HISTORY_SIZE_MEDIUM 8
    #define NIMCP_HISTORY_SIZE_LARGE 16
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED_VALUE
    #define NIMCP_HISTORY_SIZE_SMALL 8
    #define NIMCP_HISTORY_SIZE_MEDIUM 16
    #define NIMCP_HISTORY_SIZE_LARGE 32
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM_VALUE
    #define NIMCP_HISTORY_SIZE_SMALL 16
    #define NIMCP_HISTORY_SIZE_MEDIUM 32
    #define NIMCP_HISTORY_SIZE_LARGE 64
#else
    #define NIMCP_HISTORY_SIZE_SMALL 32
    #define NIMCP_HISTORY_SIZE_MEDIUM 64
    #define NIMCP_HISTORY_SIZE_LARGE 128
#endif

/**
 * @brief Spike history length
 */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_MINIMAL_VALUE
    #define NIMCP_SPIKE_HISTORY_LENGTH 16
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED_VALUE
    #define NIMCP_SPIKE_HISTORY_LENGTH 64
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM_VALUE
    #define NIMCP_SPIKE_HISTORY_LENGTH 128
#else
    #define NIMCP_SPIKE_HISTORY_LENGTH 256
#endif

/**
 * @brief Activity history window
 */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_MINIMAL_VALUE
    #define NIMCP_ACTIVITY_HISTORY_WINDOW 8
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED_VALUE
    #define NIMCP_ACTIVITY_HISTORY_WINDOW 16
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM_VALUE
    #define NIMCP_ACTIVITY_HISTORY_WINDOW 32
#else
    #define NIMCP_ACTIVITY_HISTORY_WINDOW 64
#endif

/**
 * @brief Anomaly history size
 */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_MINIMAL_VALUE
    #define NIMCP_ANOMALY_HISTORY_SIZE 5
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED_VALUE
    #define NIMCP_ANOMALY_HISTORY_SIZE 20
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM_VALUE
    #define NIMCP_ANOMALY_HISTORY_SIZE 50
#else
    #define NIMCP_ANOMALY_HISTORY_SIZE 100
#endif

/**
 * @brief Training history size
 */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_MINIMAL_VALUE
    #define NIMCP_TRAINING_HISTORY_SIZE 10
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED_VALUE
    #define NIMCP_TRAINING_HISTORY_SIZE 50
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM_VALUE
    #define NIMCP_TRAINING_HISTORY_SIZE 100
#else
    #define NIMCP_TRAINING_HISTORY_SIZE 200
#endif

/**
 * @brief Interaction history (shadow emotions, bias detection)
 */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_MINIMAL_VALUE
    #define NIMCP_INTERACTION_HISTORY_SIZE 8
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED_VALUE
    #define NIMCP_INTERACTION_HISTORY_SIZE 16
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM_VALUE
    #define NIMCP_INTERACTION_HISTORY_SIZE 32
#else
    #define NIMCP_INTERACTION_HISTORY_SIZE 64
#endif

/**
 * @brief Decision history size
 */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_MINIMAL_VALUE
    #define NIMCP_DECISION_HISTORY_SIZE 8
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED_VALUE
    #define NIMCP_DECISION_HISTORY_SIZE 32
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM_VALUE
    #define NIMCP_DECISION_HISTORY_SIZE 64
#else
    #define NIMCP_DECISION_HISTORY_SIZE 128
#endif

//=============================================================================
// Fixed Array Sizing (100KB savings potential)
//=============================================================================

/**
 * @brief Global workspace content dimension
 */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_MINIMAL_VALUE
    #define NIMCP_GW_CONTENT_DIM 64
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED_VALUE
    #define NIMCP_GW_CONTENT_DIM 128
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM_VALUE
    #define NIMCP_GW_CONTENT_DIM 192
#else
    #define NIMCP_GW_CONTENT_DIM 256
#endif

/**
 * @brief Dendrite spine array sizes
 */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_MINIMAL_VALUE
    #define NIMCP_MAX_SPINE_IDS 8
    #define NIMCP_MAX_SYNAPSE_IDS 32
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED_VALUE
    #define NIMCP_MAX_SPINE_IDS 16
    #define NIMCP_MAX_SYNAPSE_IDS 64
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM_VALUE
    #define NIMCP_MAX_SPINE_IDS 32
    #define NIMCP_MAX_SYNAPSE_IDS 128
#else
    #define NIMCP_MAX_SPINE_IDS 64
    #define NIMCP_MAX_SYNAPSE_IDS 256
#endif

/**
 * @brief Message array sizes (bio-async large arrays)
 */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_MINIMAL_VALUE
    #define NIMCP_MAX_CONTRADICTING_AGENTS 16
    #define NIMCP_MAX_WORD_IDS 4
    #define NIMCP_MAX_PROSODY_POINTS 8
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED_VALUE
    #define NIMCP_MAX_CONTRADICTING_AGENTS 32
    #define NIMCP_MAX_WORD_IDS 8
    #define NIMCP_MAX_PROSODY_POINTS 16
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM_VALUE
    #define NIMCP_MAX_CONTRADICTING_AGENTS 64
    #define NIMCP_MAX_WORD_IDS 12
    #define NIMCP_MAX_PROSODY_POINTS 24
#else
    #define NIMCP_MAX_CONTRADICTING_AGENTS 256
    #define NIMCP_MAX_WORD_IDS 16
    #define NIMCP_MAX_PROSODY_POINTS 32
#endif

/**
 * @brief Layer and network sizing
 */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_MINIMAL_VALUE
    #define NIMCP_MAX_LAYERS 4
    #define NIMCP_MAX_NEURONS_PER_LAYER 64
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED_VALUE
    #define NIMCP_MAX_LAYERS 8
    #define NIMCP_MAX_NEURONS_PER_LAYER 128
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM_VALUE
    #define NIMCP_MAX_LAYERS 16
    #define NIMCP_MAX_NEURONS_PER_LAYER 256
#else
    #define NIMCP_MAX_LAYERS 32
    #define NIMCP_MAX_NEURONS_PER_LAYER 512
#endif

//=============================================================================
// String Buffer Sizing (25KB savings potential)
//=============================================================================

/**
 * @brief Module/component name buffer size
 */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_MINIMAL_VALUE
    #define NIMCP_NAME_BUFFER_SIZE 32
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED_VALUE
    #define NIMCP_NAME_BUFFER_SIZE 64
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM_VALUE
    #define NIMCP_NAME_BUFFER_SIZE 128
#else
    #define NIMCP_NAME_BUFFER_SIZE 256
#endif

/**
 * @brief Description/message buffer size
 */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_MINIMAL_VALUE
    #define NIMCP_DESC_BUFFER_SIZE 64
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED_VALUE
    #define NIMCP_DESC_BUFFER_SIZE 128
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM_VALUE
    #define NIMCP_DESC_BUFFER_SIZE 256
#else
    #define NIMCP_DESC_BUFFER_SIZE 512
#endif

/**
 * @brief Error message buffer size
 */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_MINIMAL_VALUE
    #define NIMCP_ERROR_BUFFER_SIZE 64
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED_VALUE
    #define NIMCP_ERROR_BUFFER_SIZE 128
#else
    #define NIMCP_ERROR_BUFFER_SIZE 256
#endif

/**
 * @brief Path buffer size (tier-specific override)
 */
#ifdef NIMCP_PATH_BUFFER_SIZE
#undef NIMCP_PATH_BUFFER_SIZE
#endif
#if NIMCP_BUILD_TIER == PLATFORM_TIER_MINIMAL_VALUE
    #define NIMCP_PATH_BUFFER_SIZE 128
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED_VALUE
    #define NIMCP_PATH_BUFFER_SIZE 256
#else
    #define NIMCP_PATH_BUFFER_SIZE 512
#endif

//=============================================================================
// Mutex Pool Configuration
//=============================================================================

/**
 * @brief Mutex pool size (when NIMCP_USE_MUTEX_POOL is enabled)
 */
#define NIMCP_MUTEX_POOL_SIZE 8

/**
 * @brief Maximum bridges that can share mutex pool
 */
#define NIMCP_MUTEX_POOL_MAX_BRIDGES 256

//=============================================================================
// Statistics Conditional Macros
//=============================================================================

/**
 * @brief Declare statistics field conditionally
 *
 * Usage: NIMCP_STATS_FIELD(my_stats_t, stats);
 */
#if NIMCP_ENABLE_STATISTICS
    #define NIMCP_STATS_FIELD(type, name) type name
    #define NIMCP_STATS_INIT(ptr, init_fn) init_fn(ptr)
    #define NIMCP_STATS_UPDATE(ptr, update_fn) update_fn(ptr)
    #define NIMCP_STATS_GET(ptr, field) ((ptr)->field)
#else
    #define NIMCP_STATS_FIELD(type, name) /* empty */
    #define NIMCP_STATS_INIT(ptr, init_fn) /* empty */
    #define NIMCP_STATS_UPDATE(ptr, update_fn) /* empty */
    #define NIMCP_STATS_GET(ptr, field) (0)
#endif

/**
 * @brief Conditional statistics code block
 *
 * Usage: NIMCP_STATS_BLOCK({ stats.count++; stats.sum += val; });
 */
#if NIMCP_ENABLE_STATISTICS
    #define NIMCP_STATS_BLOCK(code) do { code } while(0)
#else
    #define NIMCP_STATS_BLOCK(code) /* empty */
#endif

//=============================================================================
// Memory Budget Helpers
//=============================================================================

/**
 * @brief Get memory budget in bytes for current tier
 */
static inline size_t nimcp_tier_memory_budget_bytes(void) {
#if NIMCP_BUILD_TIER == PLATFORM_TIER_MINIMAL_VALUE
    return 64 * 1024 * 1024;   // 64MB
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED_VALUE
    return 256 * 1024 * 1024;  // 256MB
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM_VALUE
    return 2UL * 1024 * 1024 * 1024;  // 2GB
#else
    return 8UL * 1024 * 1024 * 1024;  // 8GB
#endif
}

/**
 * @brief Get recommended thread count for current tier
 */
static inline uint32_t nimcp_tier_thread_count(void) {
#if NIMCP_BUILD_TIER == PLATFORM_TIER_MINIMAL_VALUE
    return 1;
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED_VALUE
    return 2;
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM_VALUE
    return 4;
#else
    return 8;
#endif
}

/**
 * @brief Check if feature should be enabled based on tier
 */
static inline bool nimcp_tier_feature_enabled(uint32_t feature_tier) {
    return (uint32_t)NIMCP_BUILD_TIER <= feature_tier;
}

//=============================================================================
// JEPA Tier Optimization Constants
//=============================================================================

/**
 * @brief JEPA latent embedding dimension
 *
 * WHAT: Dimension of JEPA latent space embeddings
 * WHY:  Larger dimensions capture more detail but use more memory
 * HOW:  Scale with tier (64-512)
 */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_MINIMAL_VALUE
    #define NIMCP_JEPA_LATENT_DIM 64
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED_VALUE
    #define NIMCP_JEPA_LATENT_DIM 128
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM_VALUE
    #define NIMCP_JEPA_LATENT_DIM 256
#else
    #define NIMCP_JEPA_LATENT_DIM 512
#endif

/**
 * @brief JEPA number of patches for visual encoding
 *
 * WHAT: Number of spatial patches for visual JEPA
 * WHY:  More patches = finer spatial resolution, more memory
 * HOW:  Scale from 4 (2x2) to 49 (7x7)
 */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_MINIMAL_VALUE
    #define NIMCP_JEPA_NUM_PATCHES 4
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED_VALUE
    #define NIMCP_JEPA_NUM_PATCHES 9
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM_VALUE
    #define NIMCP_JEPA_NUM_PATCHES 16
#else
    #define NIMCP_JEPA_NUM_PATCHES 49
#endif

/**
 * @brief JEPA predictor hidden dimension
 *
 * WHAT: Hidden layer size in JEPA MLP predictor
 * WHY:  Larger = more expressive prediction, more memory
 * HOW:  Scale with tier (32-256)
 */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_MINIMAL_VALUE
    #define NIMCP_JEPA_PREDICTOR_HIDDEN 32
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED_VALUE
    #define NIMCP_JEPA_PREDICTOR_HIDDEN 64
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM_VALUE
    #define NIMCP_JEPA_PREDICTOR_HIDDEN 128
#else
    #define NIMCP_JEPA_PREDICTOR_HIDDEN 256
#endif

/**
 * @brief JEPA multimodal enable flag
 *
 * WHAT: Enable/disable multimodal JEPA (visual-speech fusion)
 * WHY:  Multimodal requires additional memory for projection layers
 * HOW:  Disable on MINIMAL tier
 */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_MINIMAL_VALUE
    #define NIMCP_JEPA_ENABLE_MULTIMODAL 0
#else
    #define NIMCP_JEPA_ENABLE_MULTIMODAL 1
#endif

/**
 * @brief JEPA speech sequence length
 *
 * WHAT: Number of frames in speech JEPA sequence
 * WHY:  Longer sequences capture more temporal context
 * HOW:  Scale from 16 to 64 frames
 */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_MINIMAL_VALUE
    #define NIMCP_JEPA_SPEECH_SEQUENCE_LEN 16
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED_VALUE
    #define NIMCP_JEPA_SPEECH_SEQUENCE_LEN 32
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM_VALUE
    #define NIMCP_JEPA_SPEECH_SEQUENCE_LEN 50
#else
    #define NIMCP_JEPA_SPEECH_SEQUENCE_LEN 64
#endif

/**
 * @brief JEPA context encoder dimension
 *
 * WHAT: Dimension of context vectors for context-conditioned encoding
 * WHY:  Larger context = more task discrimination
 * HOW:  Scale from 32 to 128
 */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_MINIMAL_VALUE
    #define NIMCP_JEPA_CONTEXT_DIM 32
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED_VALUE
    #define NIMCP_JEPA_CONTEXT_DIM 64
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM_VALUE
    #define NIMCP_JEPA_CONTEXT_DIM 96
#else
    #define NIMCP_JEPA_CONTEXT_DIM 128
#endif

/**
 * @brief JEPA joint embedding dimension (multimodal)
 *
 * WHAT: Dimension of joint visual-speech embedding space
 * WHY:  Larger = more capacity for cross-modal alignment
 * HOW:  Scale from 64 to 512
 */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_MINIMAL_VALUE
    #define NIMCP_JEPA_JOINT_DIM 64
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED_VALUE
    #define NIMCP_JEPA_JOINT_DIM 128
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM_VALUE
    #define NIMCP_JEPA_JOINT_DIM 256
#else
    #define NIMCP_JEPA_JOINT_DIM 512
#endif

//=============================================================================
// Tier-Aware Allocation Helpers
//=============================================================================

/**
 * @brief Calculate tier-appropriate buffer size
 *
 * @param full_size Size needed for PLATFORM_TIER_FULL
 * @return Scaled size for current tier
 */
static inline size_t nimcp_tier_scale_size(size_t full_size) {
#if NIMCP_BUILD_TIER == PLATFORM_TIER_MINIMAL_VALUE
    return full_size / 8;
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED_VALUE
    return full_size / 4;
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM_VALUE
    return full_size / 2;
#else
    return full_size;
#endif
}

/**
 * @brief Get tier-appropriate array count
 *
 * @param full_count Count for PLATFORM_TIER_FULL
 * @return Scaled count for current tier
 */
static inline uint32_t nimcp_tier_scale_count(uint32_t full_count) {
#if NIMCP_BUILD_TIER == PLATFORM_TIER_MINIMAL_VALUE
    return (full_count + 7) / 8;  // Round up
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED_VALUE
    return (full_count + 3) / 4;
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM_VALUE
    return (full_count + 1) / 2;
#else
    return full_count;
#endif
}

#ifdef __cplusplus
}
#endif

#endif // NIMCP_TIER_OPTIMIZATION_H
