//=============================================================================
// nimcp_synapse_lifecycle.h - Synapse Lifecycle Manager (Pruning/Growth/GC)
//=============================================================================
/**
 * @file nimcp_synapse_lifecycle.h
 * @brief Dynamic synapse lifecycle: pruning, growth, metadata GC, compaction
 *
 * WHAT: Manages synapse population dynamics during training
 * WHY:  Enables structural plasticity — weak synapses die, active regions grow
 * HOW:  Periodic sweeps over network neurons; operates via accessor stubs
 *
 * PHASES:
 * - Phase 2: Pruning — remove low-weight/inactive synapses
 * - Phase 3: Growth  — add synapses to active neurons
 * - Phase 4: Metadata GC + compaction — reclaim freed slots, defragment
 *
 * INTEGRATION:
 * Call nimcp_synapse_lifecycle_step() once per training step.
 * It internally decides which operations are due based on interval config.
 */

#ifndef NIMCP_SYNAPSE_LIFECYCLE_H
#define NIMCP_SYNAPSE_LIFECYCLE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration
//=============================================================================

typedef struct {
    /* Pruning config */
    float weight_prune_threshold;      /**< Remove if |weight| < this (default 0.001) */
    float activity_prune_threshold;    /**< Remove if activity_hz < this (default 0.1) */
    uint32_t prune_interval_steps;     /**< Steps between prune sweeps (default 1000) */
    uint32_t max_prune_per_sweep;      /**< Max removals per sweep (default 5% of total) */
    uint32_t min_synapses_per_neuron;  /**< Never prune below this (default 16) */

    /* Growth config */
    float activity_growth_threshold;   /**< Grow if ema_activity > this (default 0.8) */
    uint32_t max_new_per_sweep;        /**< Max new synapses per neuron per sweep (default 8) */
    uint32_t growth_interval_steps;    /**< Steps between growth sweeps (default 2000) */
    float initial_weight;              /**< Weight for new synapses (default 0.01) */
    uint32_t max_synapses_per_neuron;  /**< Cap (default 512) */

    /* Metadata GC */
    bool enable_metadata_gc;           /**< Return freed slots to pool (default true) */
    uint32_t gc_interval_steps;        /**< Steps between GC (default 5000) */
    uint32_t metadata_pool_cap;        /**< Max metadata slots (default 50M) */

    /* Compaction */
    bool enable_compaction;            /**< Move overflow to embedded after prune (default true) */
} nimcp_synapse_lifecycle_config_t;

//=============================================================================
// Report / Statistics
//=============================================================================

typedef struct {
    uint64_t synapses_pruned;
    uint64_t synapses_grown;
    uint64_t metadata_freed;
    uint64_t metadata_orphans_collected;
    uint64_t overflow_compacted;
    float avg_weight_pruned;
    float avg_synapses_per_neuron;
    uint64_t total_synapses;
    uint32_t metadata_pool_usage;
    uint32_t metadata_pool_cap;
} nimcp_synapse_lifecycle_report_t;

//=============================================================================
// Opaque handle
//=============================================================================

typedef struct nimcp_synapse_lifecycle nimcp_synapse_lifecycle_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create a synapse lifecycle manager
 * @param config  Configuration (NULL for defaults)
 * @return Manager handle, or NULL on failure
 */
nimcp_synapse_lifecycle_t* nimcp_synapse_lifecycle_create(
    const nimcp_synapse_lifecycle_config_t* config);

/**
 * @brief Destroy a synapse lifecycle manager
 * @param mgr  Manager handle (NULL-safe)
 */
void nimcp_synapse_lifecycle_destroy(nimcp_synapse_lifecycle_t* mgr);

/**
 * @brief Called every training step; performs whichever operation is due
 * @param mgr           Manager handle
 * @param network       Opaque network pointer (passed to accessor stubs)
 * @param training_step Current training step counter
 * @param report        Output report (zeroed then populated)
 * @return 0 on success, -1 on error
 */
int nimcp_synapse_lifecycle_step(nimcp_synapse_lifecycle_t* mgr,
    void* network, uint32_t training_step,
    nimcp_synapse_lifecycle_report_t* report);

/**
 * @brief Prune weak/inactive synapses
 */
int nimcp_synapse_lifecycle_prune(nimcp_synapse_lifecycle_t* mgr,
    void* network, nimcp_synapse_lifecycle_report_t* report);

/**
 * @brief Grow new synapses on active neurons
 */
int nimcp_synapse_lifecycle_grow(nimcp_synapse_lifecycle_t* mgr,
    void* network, nimcp_synapse_lifecycle_report_t* report);

/**
 * @brief Collect orphaned metadata entries
 */
int nimcp_synapse_lifecycle_gc(nimcp_synapse_lifecycle_t* mgr,
    void* network, nimcp_synapse_lifecycle_report_t* report);

/**
 * @brief Get cumulative statistics
 */
int nimcp_synapse_lifecycle_get_report(const nimcp_synapse_lifecycle_t* mgr,
    nimcp_synapse_lifecycle_report_t* report);

/**
 * @brief Return default configuration
 */
nimcp_synapse_lifecycle_config_t nimcp_synapse_lifecycle_config_default(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SYNAPSE_LIFECYCLE_H */
