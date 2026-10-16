/**
 * @file nimcp_eidetic_memory.h
 * @brief Master header for eidetic memory integration with genius profiles
 *
 * WHAT: Unified API for applying eidetic memory enhancements to all memory systems
 * WHY:  Eidetic (photographic) memory is a cross-cutting enhancement for genius profiles
 * HOW:  Per-system apply functions modify memory system parameters based on eidetic config
 *
 * BIOLOGICAL BASIS:
 * - Tesla: Visual-spatial eidetic, could visualize complete machines mentally
 * - Mozart: Auditory eidetic, replayed Miserere after single hearing
 * - von Neumann: Numerical/verbal eidetic, instant mental calculation
 * - Kim Peek: Encyclopedic eidetic, retained 98% of everything read
 * - Wiltshire: Visual-artistic eidetic, draws cityscapes from memory
 *
 * INTEGRATION:
 * - Working Memory: Extended capacity and decay resistance
 * - Hippocampus: Enhanced encoding speed and pattern separation
 * - Semantic Memory: Larger concept capacity and faster retrieval
 * - Systems Consolidation: Faster transfer, less forgetting
 * - Engram System: More engrams, faster consolidation
 *
 * @version 1.0.0
 * @date 2026-02-03
 */

#ifndef NIMCP_EIDETIC_MEMORY_H
#define NIMCP_EIDETIC_MEMORY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations for memory systems - guard against conflicts with profiles header */
#ifndef WORKING_MEMORY_T_DEFINED
#define WORKING_MEMORY_T_DEFINED
typedef struct working_memory working_memory_t;
#endif

#ifndef SEMANTIC_MEMORY_SYSTEM_T_DEFINED
#define SEMANTIC_MEMORY_SYSTEM_T_DEFINED
typedef struct semantic_memory_system semantic_memory_system_t;
#endif

#ifndef HOPFIELD_MEMORY_T_DEFINED
#define HOPFIELD_MEMORY_T_DEFINED
typedef struct hopfield_memory hopfield_memory_t;
#endif

#ifndef AUTOBIOGRAPHICAL_MEMORY_SYSTEM_T_DEFINED
#define AUTOBIOGRAPHICAL_MEMORY_SYSTEM_T_DEFINED
typedef struct autobiographical_memory_system autobiographical_memory_system_t;
#endif

/* These types are unique to eidetic integration */
typedef struct systems_consolidation_system systems_consolidation_system_t;
typedef struct engram_system engram_system_t;
typedef struct hippocampus_adapter hippocampus_adapter_t;
typedef struct procedural_memory procedural_memory_t;
typedef struct prospective_memory prospective_memory_t;

/* Include the eidetic config from traits header */
#include "core/brain/genius/nimcp_genius_traits.h"

/* ============================================================================
 * EIDETIC ERROR CODES
 * ============================================================================ */

typedef enum {
    EIDETIC_SUCCESS = 0,
    EIDETIC_ERROR_NULL_POINTER,
    EIDETIC_ERROR_INVALID_CONFIG,
    EIDETIC_ERROR_SYSTEM_NOT_SUPPORTED,
    EIDETIC_ERROR_APPLY_FAILED,
    EIDETIC_ERROR_ALREADY_APPLIED
} eidetic_error_t;

/* ============================================================================
 * MASTER APPLY FUNCTION
 * ============================================================================ */

/**
 * @brief Apply eidetic memory configuration to all connected memory systems
 *
 * WHAT: Applies eidetic enhancements to all available memory systems
 * WHY:  Single call to enhance entire memory architecture
 * HOW:  Iterates through systems and applies per-system configs
 *
 * @param config Eidetic memory configuration
 * @param working_memory Working memory system (can be NULL)
 * @param hippocampus Hippocampus adapter (can be NULL)
 * @param semantic Semantic memory system (can be NULL)
 * @param consolidation Systems consolidation (can be NULL)
 * @param engram Engram system (can be NULL)
 * @return EIDETIC_SUCCESS or error code
 */
eidetic_error_t eidetic_apply_to_all(
    const eidetic_memory_config_t* config,
    working_memory_t* working_memory,
    hippocampus_adapter_t* hippocampus,
    semantic_memory_system_t* semantic,
    systems_consolidation_system_t* consolidation,
    engram_system_t* engram);

/* ============================================================================
 * PER-SYSTEM APPLY FUNCTIONS
 * ============================================================================ */

/**
 * @brief Apply eidetic configuration to working memory
 *
 * EFFECTS:
 * - Increases capacity from 7±2 to 12-15 items
 * - Reduces decay time constant (5-10x slower decay)
 * - Enhances refresh effectiveness
 *
 * @param wm Working memory system
 * @param config Eidetic memory configuration
 * @return EIDETIC_SUCCESS or error code
 */
eidetic_error_t eidetic_apply_to_working_memory(
    working_memory_t* wm,
    const eidetic_memory_config_t* config);

/**
 * @brief Apply eidetic configuration to hippocampus
 *
 * EFFECTS:
 * - Scales DG/CA3/CA1 cell counts (up to 4x)
 * - Enhances pattern separation ratio
 * - Enables single-exposure learning
 * - Increases replay fidelity
 *
 * @param hippo Hippocampus adapter
 * @param config Eidetic memory configuration
 * @return EIDETIC_SUCCESS or error code
 */
eidetic_error_t eidetic_apply_to_hippocampus(
    hippocampus_adapter_t* hippo,
    const eidetic_memory_config_t* config);

/**
 * @brief Apply eidetic configuration to semantic memory
 *
 * EFFECTS:
 * - Increases concept capacity (up to 8x)
 * - Expands feature dimensions (4x richer)
 * - Enhances spreading activation
 * - Enables one-shot concept formation
 *
 * @param sm Semantic memory system
 * @param config Eidetic memory configuration
 * @return EIDETIC_SUCCESS or error code
 */
eidetic_error_t eidetic_apply_to_semantic(
    semantic_memory_system_t* sm,
    const eidetic_memory_config_t* config);

/**
 * @brief Apply eidetic configuration to systems consolidation
 *
 * EFFECTS:
 * - Faster cortical transfer (3x during SWS)
 * - Earlier semantic extraction
 * - Reduced forgetting (10x slower)
 * - Preserves episodic detail
 *
 * @param sc Systems consolidation system
 * @param config Eidetic memory configuration
 * @return EIDETIC_SUCCESS or error code
 */
eidetic_error_t eidetic_apply_to_consolidation(
    systems_consolidation_system_t* sc,
    const eidetic_memory_config_t* config);

/**
 * @brief Apply eidetic configuration to engram system
 *
 * EFFECTS:
 * - Increases engram capacity (8x)
 * - More neurons per engram (4x)
 * - Faster consolidation (6x)
 * - Extended tagging window (4x)
 *
 * @param es Engram system
 * @param config Eidetic memory configuration
 * @return EIDETIC_SUCCESS or error code
 */
eidetic_error_t eidetic_apply_to_engram(
    engram_system_t* es,
    const eidetic_memory_config_t* config);

/**
 * @brief Apply eidetic configuration to Hopfield associative memory
 *
 * EFFECTS:
 * - Increases pattern capacity (8x)
 * - Higher pattern resolution (4x)
 * - Sharper retrieval (higher inverse temperature)
 * - One-shot storage capability
 *
 * @param hm Hopfield memory
 * @param config Eidetic memory configuration
 * @return EIDETIC_SUCCESS or error code
 */
eidetic_error_t eidetic_apply_to_hopfield(
    hopfield_memory_t* hm,
    const eidetic_memory_config_t* config);

/**
 * @brief Apply eidetic configuration to procedural memory
 *
 * EFFECTS:
 * - Faster skill acquisition (3x)
 * - Larger chunk size (2x)
 * - Faster automation (10x fewer repetitions)
 * - Mental practice effectiveness
 *
 * @param pm Procedural memory
 * @param config Eidetic memory configuration
 * @return EIDETIC_SUCCESS or error code
 */
eidetic_error_t eidetic_apply_to_procedural(
    procedural_memory_t* pm,
    const eidetic_memory_config_t* config);

/**
 * @brief Apply eidetic configuration to prospective memory
 *
 * EFFECTS:
 * - More intention capacity (4x)
 * - Better time precision (30x)
 * - Lower cue detection threshold
 * - Slower intention forgetting
 *
 * @param pm Prospective memory
 * @param config Eidetic memory configuration
 * @return EIDETIC_SUCCESS or error code
 */
eidetic_error_t eidetic_apply_to_prospective(
    prospective_memory_t* pm,
    const eidetic_memory_config_t* config);

/**
 * @brief Apply eidetic configuration to autobiographical memory
 *
 * EFFECTS:
 * - 10x memory capacity
 * - Verbatim detail preservation
 * - Lower importance threshold
 * - Near-perfect retrieval precision
 *
 * @param am Autobiographical memory system
 * @param config Eidetic memory configuration
 * @return EIDETIC_SUCCESS or error code
 */
eidetic_error_t eidetic_apply_to_autobiographical(
    autobiographical_memory_system_t* am,
    const eidetic_memory_config_t* config);

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

/**
 * @brief Get error string for eidetic error code
 *
 * @param error Error code
 * @return Static string description
 */
const char* eidetic_error_string(eidetic_error_t error);

/**
 * @brief Check if eidetic config is valid
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 */
bool eidetic_config_is_valid(const eidetic_memory_config_t* config);

/**
 * @brief Scale a base value by eidetic strength
 *
 * @param base_value Original value
 * @param eidetic_strength Eidetic strength (0.0-3.0)
 * @param max_multiplier Maximum scaling factor
 * @return Scaled value
 */
float eidetic_scale_value(float base_value, float eidetic_strength, float max_multiplier);

/**
 * @brief Compute decay resistance from eidetic strength
 *
 * @param eidetic_strength Eidetic strength (0.0-3.0)
 * @return Decay multiplier (lower = slower decay)
 */
float eidetic_compute_decay_resistance(float eidetic_strength);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EIDETIC_MEMORY_H */
