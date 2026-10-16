#ifndef NIMCP_BRAIN_PARALLEL_INIT_H
#define NIMCP_BRAIN_PARALLEL_INIT_H

/**
 * @file nimcp_brain_parallel_init.h
 * @brief Parallel wave-based subsystem initialization for brain creation
 *
 * WHAT: Parallelizes the 80+ subsystem inits in brain_create_custom() using
 *       dependency waves executed via the thread pool.
 * WHY:  Sequential init takes 80-250s on large brains. Many subsystems are
 *       independent and can run in parallel within each wave.
 * HOW:  Groups inits into ~28 waves by dependency. Within each wave, all
 *       tasks are submitted to the thread pool and awaited. An atomic error
 *       flag propagates failures across tasks.
 */

#include "core/brain/nimcp_brain_internal.h"

/**
 * @brief Parallel subsystem initialization replacing sequential init block.
 *
 * Initializes all brain subsystems using wave-based parallelism.
 * Falls back gracefully if thread pool creation fails (runs serial).
 *
 * @param brain  The brain instance (already allocated with core structures).
 * @param config The brain configuration.
 * @return true on success, false on any subsystem init failure.
 *
 * @note Caller is responsible for brain_destroy() on failure.
 */
bool nimcp_brain_parallel_init_subsystems(brain_t brain, const brain_config_t* config);

#endif /* NIMCP_BRAIN_PARALLEL_INIT_H */
