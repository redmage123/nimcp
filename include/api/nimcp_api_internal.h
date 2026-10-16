//=============================================================================
// nimcp_api_internal.h - Internal API Structure Definitions (CANONICAL)
//=============================================================================
/**
 * @file nimcp_api_internal.h
 * @brief Canonical definitions for NIMCP API internal handle structures
 *
 * This header is the SINGLE SOURCE OF TRUTH for all API handle structures.
 * It is shared between nimcp.c (the compiled API) and any auxiliary API files.
 * NOT for external use - these structures are opaque to users.
 *
 * CRITICAL: Do NOT duplicate these struct definitions elsewhere.
 * Any file needing access to handle internals MUST include this header.
 */

#ifndef NIMCP_API_INTERNAL_H
#define NIMCP_API_INTERNAL_H

#include "core/brain/nimcp_brain.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "cognitive/ethics/nimcp_ethics.h"
#include "cognitive/knowledge/nimcp_knowledge.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Internal Handle Structures
//=============================================================================

/**
 * @brief Canonical brain handle definition (3 fields).
 *
 * CRITICAL: This is the ONLY valid definition. The last_loss and
 * last_gradient_norm fields are written by nimcp_brain_learn_example()
 * and read by nimcp_brain_get_last_loss() / nimcp_brain_get_last_gradient_norm().
 *
 * Allocation sites MUST use nimcp_calloc() to zero-initialize these floats.
 */
struct nimcp_brain_handle {
    brain_t internal_brain;      // Wraps internal brain_t
    float last_loss;             // Loss from most recent learn_example call
    float last_gradient_norm;    // Gradient L2 norm from most recent learn_example call
};

struct nimcp_network_handle {
    neural_network_t internal_network;  // Wraps internal neural_network_t
};

struct nimcp_ethics_handle {
    ethics_engine_t internal_ethics;  // Wraps internal ethics_engine_t
};

struct nimcp_knowledge_handle {
    knowledge_system_t internal_knowledge;  // Wraps internal knowledge_system_t
};

/**
 * @brief Brain snapshot handle for COW save/restore
 */
struct nimcp_brain_snapshot_handle {
    brain_t internal_brain_snapshot;  // Snapshot of brain state
    uint64_t timestamp_us;            // Snapshot creation time
    size_t shared_memory_size;        // Size of shared memory (for tracking)
    uint32_t snapshot_refcount;       // Reference count for this snapshot
    bool is_isolated;                 // Isolation flag (true if snapshot is independent)
};

//=============================================================================
// Public Status Code Mapping
//=============================================================================

/**
 * @brief Map any internal error code to a valid nimcp_status_t value.
 *
 * The public API (nimcp.h) defines only 6 status codes:
 *   NIMCP_OK(0), NIMCP_ERROR(1000), NIMCP_ERROR_NULL_ARG(1003),
 *   NIMCP_ERROR_INVALID(1004), NIMCP_ERROR_MEMORY(2000), NIMCP_ERROR_IO(4000)
 *
 * Internal codes like NIMCP_ERROR_BBB_REJECTED, NIMCP_ERROR_LEARNING_FAILED,
 * NIMCP_ERROR_OPERATION_FAILED, etc. MUST NOT leak through the public API.
 */
static inline nimcp_status_t nimcp_map_to_public_status(int internal_code) {
    switch (internal_code) {
        case 0:    return NIMCP_OK;
        case 1000: return NIMCP_ERROR;
        case 1003: return NIMCP_ERROR_NULL_ARG;
        case 1004: return NIMCP_ERROR_INVALID;
        case 2000: return NIMCP_ERROR_MEMORY;
        case 4000: return NIMCP_ERROR_IO;
        default:   return NIMCP_ERROR;  /* All unknown codes → generic error */
    }
}

#ifdef __cplusplus
}
#endif

#endif // NIMCP_API_INTERNAL_H
