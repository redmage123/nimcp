/**
 * @file nimcp_brain_parallel_stages.h
 * @brief Parallel dispatch for independent cognitive stages in brain_decide()
 *
 * WHAT: Enables concurrent execution of independent pre-forward and post-forward stages
 * WHY:  brain_decide() runs 28 stages serially; many are independent and can be parallelized
 * HOW:  Thread pool dispatch for independent stages, main thread runs sequential stages
 *
 * DEPENDENCY ANALYSIS:
 *   Pre-forward independent: wellbeing, engram recall, sleep, curiosity, predictive
 *   Post-forward independent: engram consol, systems consol, WM transfer, semantic,
 *                             glial, ToM, Shannon, quantum-Shannon
 *   Post-forward sequential: sleep noise, degradation, executive, emotional, etc.
 */

#ifndef NIMCP_BRAIN_PARALLEL_STAGES_H
#define NIMCP_BRAIN_PARALLEL_STAGES_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct brain_struct* brain_t;
struct nimcp_thread_pool;

/**
 * @brief Context for pre-forward parallel stage outputs
 *
 * Each field is written by exactly one task — no synchronization needed.
 */
typedef struct {
    float distress_level;       // Stage 0: wellbeing
    bool wellbeing_done;

    float engram_strength;      // Stage 0.4: engram recall
    bool engram_done;

    float sleep_pressure;       // Stage 0.5: sleep/wake
    bool sleep_done;

    float curiosity_score;      // Stage 0.6: curiosity
    bool curiosity_done;

    float prediction_error;     // Stage 1: predictive processing
    bool prediction_done;
} pre_forward_context_t;

/**
 * @brief Context for post-forward parallel stage outputs
 *
 * Independent stages that can run while main thread handles sequential stages.
 */
typedef struct {
    bool engram_consol_done;    // Stage 3.8
    bool systems_consol_done;   // Stage 3.9
    bool wm_transfer_done;      // Stage 3.10
    bool semantic_done;         // Stage 3.11
    bool glial_done;            // Stage 8 (glial)
    bool tom_done;              // Stage 9 (theory of mind)
    bool shannon_done;          // Phase C4
    bool quantum_shannon_done;  // Phase C4.1
    bool physics_done;          // Wave 5: intuitive physics step (one dt per decide cycle)
    void* _internal_args;       // Internal: heap-allocated task args, caller must free after pool_wait
} post_forward_context_t;

/**
 * @brief Run independent pre-forward stages in parallel
 *
 * Dispatches 5 independent stages to thread pool, waits for completion.
 * Results stored in context struct — caller reads them to feed into later stages.
 *
 * @param brain Brain handle
 * @param features Input features
 * @param num_features Feature count
 * @param pool Thread pool to use
 * @param ctx Output context (must be zeroed by caller)
 * @return true on success
 */
bool brain_decide_parallel_pre_forward(
    brain_t brain,
    const float* features,
    uint32_t num_features,
    struct nimcp_thread_pool* pool,
    pre_forward_context_t* ctx);

/**
 * @brief Submit independent post-forward stages to thread pool (non-blocking)
 *
 * Submits 8 independent tasks to thread pool. Caller must call
 * nimcp_pool_wait() after running sequential stages on main thread.
 *
 * @param brain Brain handle
 * @param decision Current decision being built
 * @param features Input features — forwarded to stages that need them (e.g. ToM self-model)
 * @param num_features Length of features array (0 => stages that need features skip)
 * @param pool Thread pool
 * @param ctx Output context
 * @return true on success
 */
bool brain_decide_submit_post_forward(
    brain_t brain,
    void* decision,
    const float* features,
    uint32_t num_features,
    struct nimcp_thread_pool* pool,
    post_forward_context_t* ctx);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_PARALLEL_STAGES_H
