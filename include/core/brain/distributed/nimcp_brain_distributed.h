//=============================================================================
// nimcp_brain_distributed.h - Brain Distributed Operations API
//=============================================================================
/**
 * @file nimcp_brain_distributed.h
 * @brief Brain distributed operations: Copy-on-Write and P2P synchronization
 *
 * WHAT: Distributed brain operations combining COW cloning and P2P coordination
 * WHY:  Enable efficient brain replication and multi-brain collaborative learning
 * HOW:  Combines COW optimization with distributed cognition for scalable systems
 *
 * ARCHITECTURE:
 * - Copy-on-Write (COW): Efficient brain cloning with shared networks
 * - Distributed Coordination: P2P multi-brain synchronization
 * - Reference Counting: Safe shared resource management
 *
 * MODULES INTEGRATED:
 * 1. Copy-on-Write Brain Cloning (Phase 2)
 *    - brain_clone_cow(): Lightweight brain cloning (86% memory savings)
 *    - ensure_writable_network(): COW trigger for write operations
 *    - brain_mark_as_snapshot(): Preserve stats for snapshots
 *
 * 2. Distributed Brain API (Phase 3)
 *    - brain_create_distributed(): Create brain with P2P coordination
 *    - brain_enable_distributed(): Convert standalone to distributed mode
 *    - brain_sync_neuromodulators(): Explicit neuromodulator sync
 *    - brain_get_distributed_stats(): Monitor distributed performance
 *    - brain_is_distributed(): Query brain distribution status
 *
 * PERFORMANCE:
 * - COW cloning: <10ms vs ~350ms for full copy
 * - COW memory: ~1MB overhead vs ~50MB for full copy
 * - Distributed sync: O(P) where P = peer count
 * - Neuromod broadcast: O(P × N) where N = neuromod types
 *
 * THREAD SAFETY:
 * - COW operations: Not thread-safe, caller ensures exclusive access
 * - Distributed operations: Thread-safe with internal locks
 * - Reference counting: Atomic operations with mutex protection
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 2.7
 */

#ifndef NIMCP_BRAIN_DISTRIBUTED_H
#define NIMCP_BRAIN_DISTRIBUTED_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"
#include "core/brain/nimcp_brain.h"
#include "networking/distributed/nimcp_distributed_cognition.h"
#include "networking/p2p/nimcp_p2pnode.h"

#ifdef __cplusplus
extern "C" {
#endif

// NOTE: brain_t, brain_stats_t, brain_size_t, brain_task_t are defined in
//       nimcp_brain.h (included above) - no need for forward declarations

//=============================================================================
// Copy-on-Write Brain Cloning API
//=============================================================================

/**
 * @brief Clone brain using copy-on-write optimization
 *
 * WHAT: Creates lightweight clone sharing network with original
 * WHY:  Enable efficient replication (86% memory savings)
 * HOW:  Shares adaptive_network_t, copies metadata
 *
 * PERFORMANCE: <10ms vs ~350ms for full copy
 * MEMORY: ~1MB overhead vs ~50MB for full copy
 *
 * BEHAVIOR:
 * - Original and clone share network via reference counting
 * - First write triggers COW (copy network)
 * - Safe for concurrent reads, writes require exclusive access
 * - Clone inherits config, labels, strategy from original
 *
 * REFERENCE COUNTING:
 * - Initializes shared refcount on first clone
 * - Increments on additional clones
 * - Decrements on destroy, frees when reaches zero
 *
 * @param original Brain to clone (must be valid, non-NULL)
 * @return Cloned brain or NULL on error
 *
 * ERROR CONDITIONS:
 * - Returns NULL if original is NULL
 * - Returns NULL if original has NULL network
 * - Returns NULL on allocation failure
 *
 * THREAD SAFETY: Not thread-safe - caller must ensure exclusive access
 */
NIMCP_EXPORT brain_t brain_clone_cow(brain_t original);

/**
 * @brief Mark brain as a snapshot with preserved stats
 *
 * WHAT: Sets snapshot flag and preserves current stats
 * WHY:  Snapshots should preserve stats at snapshot time, not reflect future changes
 * HOW:  Stores current stats in brain->snapshot_stats
 *
 * BEHAVIOR:
 * - Sets is_snapshot flag on brain
 * - Copies provided stats to brain->snapshot_stats
 * - Future stat queries return preserved snapshot stats
 *
 * USE CASE: Checkpoint brain state for later comparison or rollback
 *
 * @param brain Brain to mark as snapshot (NULL-safe)
 * @param stats Stats to preserve (NULL-safe)
 *
 * THREAD SAFETY: Not thread-safe - caller must ensure exclusive access
 */
NIMCP_EXPORT void brain_mark_as_snapshot(brain_t brain, const brain_stats_t* stats);

//=============================================================================
// Distributed Brain API
//=============================================================================

/**
 * @brief Create distributed brain with P2P coordination
 *
 * WHAT: Create brain with distributed cognition enabled from start
 * WHY:  Enable multi-brain collaborative learning and shared chemical signals
 * HOW:  Creates standard brain, then attaches distributed cognition coordinator
 *
 * FEATURES:
 * - Neuromodulator network synchronization
 * - Glial cross-node coordination
 * - Brain region state sharing
 * - Automatic sync threads
 *
 * CONFIGURATION:
 * - Neuromod broadcast interval: 100ms
 * - Neuromod diffusion rate: 0.5
 * - Glial sync interval: 100ms
 * - Region sync interval: 100ms
 * - Sync mode: Bidirectional
 *
 * @param task_name Human-readable task name (e.g., "vision", "ethics")
 * @param size Brain size preset (TINY, SMALL, MEDIUM, LARGE, CUSTOM)
 * @param task Brain task type (CLASSIFICATION, REGRESSION, etc.)
 * @param num_inputs Number of input features
 * @param num_outputs Number of output classes/values
 * @param p2p_node P2P network node for message transport (must be valid)
 * @return Distributed brain handle or NULL on error
 *
 * ERROR CONDITIONS:
 * - Returns NULL if p2p_node is NULL
 * - Returns NULL if brain creation fails
 * - Returns NULL if distributed coordination setup fails
 *
 * CLEANUP: Caller must call brain_destroy() to free resources
 * THREAD SAFETY: Thread-safe creation
 * PERFORMANCE: O(n) where n = num_neurons + network initialization
 */
NIMCP_EXPORT brain_t brain_create_distributed(
    const char* task_name,
    brain_size_t size,
    brain_task_t task,
    uint32_t num_inputs,
    uint32_t num_outputs,
    p2p_node_t p2p_node
);

/**
 * @brief Enable distributed coordination on existing brain
 *
 * WHAT: Convert standalone brain to distributed mode
 * WHY:  Allow retrofitting existing brains with P2P coordination
 * HOW:  Creates distrib_cognition coordinator and starts sync threads
 *
 * BEHAVIOR:
 * - Creates distributed cognition coordinator
 * - Starts automatic sync threads
 * - Updates brain config to enable_distributed=true
 * - Stores p2p_node reference in brain config
 *
 * SYNC CONFIGURATION:
 * - Same defaults as brain_create_distributed()
 * - Neuromod sync: 100ms interval, 0.5 diffusion rate
 * - Glial sync: 100ms interval
 * - Region sync: 100ms interval
 *
 * @param brain Brain to enable distributed mode on (must be valid)
 * @param p2p_node P2P network node (must be valid)
 * @return true on success, false on error
 *
 * ERROR CONDITIONS:
 * - Returns false if brain is NULL
 * - Returns false if p2p_node is NULL
 * - Returns false if brain already distributed
 * - Returns false if coordinator creation fails
 *
 * THREAD SAFETY: Thread-safe if brain not actively processing
 */
NIMCP_EXPORT bool brain_enable_distributed(brain_t brain, p2p_node_t p2p_node);

/**
 * @brief Synchronize neuromodulators with peer brains
 *
 * WHAT: Manually trigger neuromodulator broadcast to all peers
 * WHY:  Allow explicit control of sync timing for performance tuning
 * HOW:  Calls distrib_cognition_broadcast_neuromod for all neuromod types
 *
 * NEUROMODULATORS SYNCED:
 * - Dopamine: Reward signaling
 * - Serotonin: Mood regulation
 * - Norepinephrine: Arousal/attention
 * - Acetylcholine: Learning modulation
 *
 * BEHAVIOR:
 * - Broadcasts current neuromod concentrations to all peers
 * - Uses default 0.5 concentration if not reading from pool
 * - Peers receive and apply with diffusion rate
 *
 * USE CASE:
 * - Explicit sync after significant events
 * - Manual control when auto-sync disabled
 * - Performance optimization (batch syncs)
 *
 * @param brain Brain to sync (must be distributed)
 * @return true on success, false on error
 *
 * ERROR CONDITIONS:
 * - Returns false if brain is NULL
 * - Returns false if brain is not distributed
 * - Returns false if any broadcast fails
 *
 * THREAD SAFETY: Thread-safe
 * PERFORMANCE: O(P × N) where P=peers, N=neuromod types (4)
 */
NIMCP_EXPORT bool brain_sync_neuromodulators(brain_t brain);

/**
 * @brief Get distributed cognition statistics
 *
 * WHAT: Query performance and health metrics of distributed brain
 * WHY:  Monitor distributed brain performance, detect issues
 * HOW:  Forwards query to underlying distrib_cognition coordinator
 *
 * STATISTICS PROVIDED:
 * - Network: messages sent/received/dropped, peers connected
 * - Neuromod: broadcasts, updates received, avg latency
 * - Glial: pruning coordinations, calcium propagations
 * - Region: state syncs, activity broadcasts
 * - Timing: last sync timestamps for each subsystem
 *
 * USE CASE:
 * - Performance monitoring dashboards
 * - Debugging distributed issues
 * - Health checks for distributed systems
 *
 * @param brain Brain to query (must be distributed)
 * @param stats Output statistics structure (must be valid)
 * @return true on success, false on error
 *
 * ERROR CONDITIONS:
 * - Returns false if brain is NULL
 * - Returns false if stats is NULL
 * - Returns false if brain is not distributed
 * - Returns false if stats query fails
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT bool brain_get_distributed_stats(
    brain_t brain,
    distrib_cognition_stats_t* stats
);

/**
 * @brief Check if brain is distributed
 *
 * WHAT: Query whether brain has distributed coordination enabled
 * WHY:  Allow callers to query brain mode before calling distributed APIs
 * HOW:  Return true if distributed coordinator exists
 *
 * BEHAVIOR:
 * - Returns true if brain->distributed != NULL
 * - Returns false if brain is NULL or standalone
 * - No side effects, safe to call repeatedly
 *
 * USE CASE:
 * - Conditional distributed operations
 * - API validation before distributed calls
 * - Feature detection in generic code
 *
 * @param brain Brain to check (NULL-safe)
 * @return true if distributed, false otherwise
 *
 * THREAD SAFETY: Thread-safe (read-only)
 */
NIMCP_EXPORT bool brain_is_distributed(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_DISTRIBUTED_H
