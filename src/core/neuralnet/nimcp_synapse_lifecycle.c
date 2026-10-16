/**
 * @file nimcp_synapse_lifecycle.c
 * @brief Synapse lifecycle manager — pruning, growth, metadata GC, compaction
 *
 * WHAT: Manages dynamic synapse population during training
 * WHY:  Structural plasticity: weak synapses die, active regions grow new ones
 * HOW:  Periodic sweeps via configurable intervals; accessor stubs for network
 *
 * PHASES:
 *   Phase 2 — Pruning:   Remove synapses with |weight| < threshold or low activity
 *   Phase 3 — Growth:    Add synapses to highly active neurons
 *   Phase 4 — GC:        Reclaim orphaned metadata; compact overflow into embedded
 */

#include "core/neuralnet/nimcp_synapse_lifecycle.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>
#include <math.h>

#define LOG_MODULE "SYNAPSE_LIFECYCLE"

//=============================================================================
// Network accessor stubs (weak symbols — overridden when wired into network)
//=============================================================================

/**
 * These weak stubs allow the lifecycle manager to compile and link
 * independently. The real implementations are provided when the manager
 * is wired into the neural network module.
 */

__attribute__((weak))
uint32_t nimcp_network_get_neuron_count(const void* network) {
    (void)network;
    return 0;
}

__attribute__((weak))
uint32_t nimcp_neuron_get_outgoing_count(const void* network, uint32_t neuron_id) {
    (void)network; (void)neuron_id;
    return 0;
}

__attribute__((weak))
float nimcp_neuron_get_synapse_weight(const void* network, uint32_t neuron_id, uint32_t syn_idx) {
    (void)network; (void)neuron_id; (void)syn_idx;
    return 0.0f;
}

__attribute__((weak))
int nimcp_neuron_remove_synapse(void* network, uint32_t neuron_id, uint32_t syn_idx) {
    (void)network; (void)neuron_id; (void)syn_idx;
    return -1;
}

__attribute__((weak))
int nimcp_neuron_add_synapse(void* network, uint32_t src_id, uint32_t tgt_id, float weight) {
    (void)network; (void)src_id; (void)tgt_id; (void)weight;
    return -1;
}

__attribute__((weak))
float nimcp_neuron_get_activity(const void* network, uint32_t neuron_id) {
    (void)network; (void)neuron_id;
    return 0.0f;
}

__attribute__((weak))
int nimcp_neuron_has_connection(const void* network, uint32_t src_id, uint32_t tgt_id) {
    (void)network; (void)src_id; (void)tgt_id;
    return 0;
}

__attribute__((weak))
uint32_t nimcp_neuron_get_metadata_pool_usage(const void* network) {
    (void)network;
    return 0;
}

__attribute__((weak))
uint32_t nimcp_neuron_get_metadata_pool_cap(const void* network) {
    (void)network;
    return 0;
}

__attribute__((weak))
int nimcp_neuron_compact_overflow(void* network, uint32_t neuron_id) {
    (void)network; (void)neuron_id;
    return 0;
}

//=============================================================================
// Internal structure
//=============================================================================

struct nimcp_synapse_lifecycle {
    nimcp_synapse_lifecycle_config_t config;

    /* Cumulative counters */
    uint64_t total_pruned;
    uint64_t total_grown;
    uint64_t total_gc;
    uint64_t total_compacted;

    /* Step tracking */
    uint32_t last_prune_step;
    uint32_t last_growth_step;
    uint32_t last_gc_step;

    /* Simple xorshift RNG for growth target selection */
    uint32_t rng_state;
};

//=============================================================================
// RNG helper
//=============================================================================

static uint32_t lifecycle_rng_next(nimcp_synapse_lifecycle_t* mgr) {
    uint32_t x = mgr->rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    mgr->rng_state = x;
    return x;
}

//=============================================================================
// Default configuration
//=============================================================================

nimcp_synapse_lifecycle_config_t nimcp_synapse_lifecycle_config_default(void) {
    nimcp_synapse_lifecycle_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    /* Pruning */
    cfg.weight_prune_threshold    = 0.001f;
    cfg.activity_prune_threshold  = 0.1f;
    cfg.prune_interval_steps      = 1000;
    cfg.max_prune_per_sweep       = 0;     /* 0 = auto (5% of total) */
    cfg.min_synapses_per_neuron   = 16;

    /* Growth */
    cfg.activity_growth_threshold = 0.8f;
    cfg.max_new_per_sweep         = 8;
    cfg.growth_interval_steps     = 2000;
    cfg.initial_weight            = 0.01f;
    cfg.max_synapses_per_neuron   = 512;

    /* Metadata GC */
    cfg.enable_metadata_gc        = true;
    cfg.gc_interval_steps         = 5000;
    cfg.metadata_pool_cap         = 50000000;  /* 50M */

    /* Compaction */
    cfg.enable_compaction         = true;

    return cfg;
}

//=============================================================================
// Create / Destroy
//=============================================================================

nimcp_synapse_lifecycle_t* nimcp_synapse_lifecycle_create(
    const nimcp_synapse_lifecycle_config_t* config)
{
    nimcp_synapse_lifecycle_t* mgr = (nimcp_synapse_lifecycle_t*)nimcp_calloc(
        1, sizeof(nimcp_synapse_lifecycle_t));
    if (!mgr) {
        LOG_ERROR("Failed to allocate synapse lifecycle manager");
        return NULL;
    }

    if (config) {
        mgr->config = *config;
    } else {
        mgr->config = nimcp_synapse_lifecycle_config_default();
    }

    mgr->total_pruned   = 0;
    mgr->total_grown    = 0;
    mgr->total_gc       = 0;
    mgr->total_compacted = 0;
    mgr->last_prune_step  = 0;
    mgr->last_growth_step = 0;
    mgr->last_gc_step     = 0;

    /* Seed RNG with something non-zero */
    mgr->rng_state = 0xDEADBEEF;

    LOG_INFO("Synapse lifecycle manager created: prune_interval=%u, growth_interval=%u, gc_interval=%u",
             mgr->config.prune_interval_steps,
             mgr->config.growth_interval_steps,
             mgr->config.gc_interval_steps);

    return mgr;
}

void nimcp_synapse_lifecycle_destroy(nimcp_synapse_lifecycle_t* mgr) {
    if (!mgr) {
        return;
    }

    LOG_INFO("Synapse lifecycle manager destroyed: pruned=%lu, grown=%lu, gc=%lu, compacted=%lu",
             (unsigned long)mgr->total_pruned,
             (unsigned long)mgr->total_grown,
             (unsigned long)mgr->total_gc,
             (unsigned long)mgr->total_compacted);

    nimcp_free(mgr);
}

//=============================================================================
// Phase 2: Pruning
//=============================================================================

int nimcp_synapse_lifecycle_prune(nimcp_synapse_lifecycle_t* mgr,
    void* network, nimcp_synapse_lifecycle_report_t* report)
{
    if (!mgr || !network || !report) {
        return -1;
    }

    uint32_t neuron_count = nimcp_network_get_neuron_count(network);
    if (neuron_count == 0) {
        return 0;
    }

    /* Compute total synapses for max_prune_per_sweep auto calculation */
    uint64_t total_synapses = 0;
    for (uint32_t n = 0; n < neuron_count; n++) {
        total_synapses += nimcp_neuron_get_outgoing_count(network, n);
    }

    uint32_t max_prune = mgr->config.max_prune_per_sweep;
    if (max_prune == 0) {
        /* Default: 5% of total synapses */
        max_prune = (uint32_t)(total_synapses / 20);
        if (max_prune < 1) max_prune = 1;
    }

    uint64_t pruned = 0;
    float weight_sum_pruned = 0.0f;
    uint64_t compacted = 0;

    for (uint32_t n = 0; n < neuron_count && pruned < max_prune; n++) {
        uint32_t syn_count = nimcp_neuron_get_outgoing_count(network, n);

        /* Never prune below minimum */
        if (syn_count <= mgr->config.min_synapses_per_neuron) {
            continue;
        }

        /* Iterate backwards for safe swap-and-pop removal */
        uint32_t removable = syn_count - mgr->config.min_synapses_per_neuron;
        for (uint32_t i = syn_count; i > 0 && pruned < max_prune && removable > 0; ) {
            i--;
            float w = nimcp_neuron_get_synapse_weight(network, n, i);

            if (fabsf(w) < mgr->config.weight_prune_threshold) {
                if (nimcp_neuron_remove_synapse(network, n, i) == 0) {
                    weight_sum_pruned += fabsf(w);
                    pruned++;
                    removable--;
                }
            }
        }

        /* Compaction: move overflow handles into freed embedded slots */
        if (mgr->config.enable_compaction) {
            if (nimcp_neuron_compact_overflow(network, n) == 0) {
                compacted++;
            }
        }
    }

    report->synapses_pruned = pruned;
    report->overflow_compacted = compacted;
    report->avg_weight_pruned = (pruned > 0) ? (weight_sum_pruned / (float)pruned) : 0.0f;
    report->total_synapses = total_synapses - pruned;

    /* Compute average synapses per neuron after pruning */
    if (neuron_count > 0) {
        report->avg_synapses_per_neuron = (float)report->total_synapses / (float)neuron_count;
    }

    mgr->total_pruned += pruned;
    mgr->total_compacted += compacted;

    if (pruned > 0) {
        LOG_INFO("Pruned %lu synapses (avg |w|=%.6f), compacted %lu neurons",
                 (unsigned long)pruned, report->avg_weight_pruned,
                 (unsigned long)compacted);
    }

    return 0;
}

//=============================================================================
// Phase 3: Growth
//=============================================================================

int nimcp_synapse_lifecycle_grow(nimcp_synapse_lifecycle_t* mgr,
    void* network, nimcp_synapse_lifecycle_report_t* report)
{
    if (!mgr || !network || !report) {
        return -1;
    }

    uint32_t neuron_count = nimcp_network_get_neuron_count(network);
    if (neuron_count == 0) {
        return 0;
    }

    uint64_t grown = 0;

    for (uint32_t n = 0; n < neuron_count; n++) {
        float activity = nimcp_neuron_get_activity(network, n);
        if (activity < mgr->config.activity_growth_threshold) {
            continue;
        }

        uint32_t syn_count = nimcp_neuron_get_outgoing_count(network, n);
        if (syn_count >= mgr->config.max_synapses_per_neuron) {
            continue;
        }

        /* Try to add up to max_new_per_sweep new synapses */
        uint32_t added = 0;
        uint32_t attempts = 0;
        uint32_t max_attempts = mgr->config.max_new_per_sweep * 4;  /* Allow some misses */

        while (added < mgr->config.max_new_per_sweep && attempts < max_attempts) {
            attempts++;

            /* Pick a random target neuron */
            uint32_t tgt = lifecycle_rng_next(mgr) % neuron_count;

            /* Skip self-connections */
            if (tgt == n) {
                continue;
            }

            /* Skip if already connected */
            if (nimcp_neuron_has_connection(network, n, tgt)) {
                continue;
            }

            /* Add synapse with small initial weight */
            if (nimcp_neuron_add_synapse(network, n, tgt, mgr->config.initial_weight) == 0) {
                added++;
                grown++;
            }
        }
    }

    report->synapses_grown = grown;
    mgr->total_grown += grown;

    /* Recompute total synapses */
    uint64_t total = 0;
    for (uint32_t n = 0; n < neuron_count; n++) {
        total += nimcp_neuron_get_outgoing_count(network, n);
    }
    report->total_synapses = total;
    if (neuron_count > 0) {
        report->avg_synapses_per_neuron = (float)total / (float)neuron_count;
    }

    if (grown > 0) {
        LOG_INFO("Grew %lu new synapses (total now %lu, avg %.1f/neuron)",
                 (unsigned long)grown, (unsigned long)total,
                 report->avg_synapses_per_neuron);
    }

    return 0;
}

//=============================================================================
// Phase 4: Metadata GC
//=============================================================================

int nimcp_synapse_lifecycle_gc(nimcp_synapse_lifecycle_t* mgr,
    void* network, nimcp_synapse_lifecycle_report_t* report)
{
    if (!mgr || !network || !report) {
        return -1;
    }

    /*
     * Metadata GC strategy:
     *
     * A full orphan scan (checking every metadata slot against every synapse)
     * is O(pool_size * avg_synapses) and prohibitively expensive for 50M slots.
     *
     * Instead, we rely on the fact that pruning already frees metadata via
     * sparse_synapse_remove_with_metadata(). The GC step here:
     *   1. Reports pool usage statistics
     *   2. Logs warnings if usage is high
     *   3. The freed-during-prune count is already tracked
     *
     * True orphan collection (for leaked metadata from crashes/bugs) would
     * require a mark-and-sweep pass. We report pool stats so the operator
     * can detect leaks via growing usage without corresponding synapse growth.
     */

    uint32_t pool_usage = nimcp_neuron_get_metadata_pool_usage(network);
    uint32_t pool_cap   = nimcp_neuron_get_metadata_pool_cap(network);

    report->metadata_pool_usage = pool_usage;
    report->metadata_pool_cap   = pool_cap;
    report->metadata_orphans_collected = 0;  /* No orphan scan in this version */

    /* Track freed metadata from pruning (already freed inline during prune) */
    report->metadata_freed = mgr->total_pruned;  /* Approximation: 1 metadata per pruned synapse */

    mgr->total_gc++;

    if (pool_cap > 0) {
        float utilization = (float)pool_usage / (float)pool_cap;
        if (utilization > 0.9f) {
            LOG_WARN("Metadata pool at %.1f%% capacity (%u / %u) — consider increasing pool cap",
                     utilization * 100.0f, pool_usage, pool_cap);
        } else {
            LOG_DEBUG("Metadata GC: pool %u / %u (%.1f%%)",
                      pool_usage, pool_cap, utilization * 100.0f);
        }
    }

    return 0;
}

//=============================================================================
// Step function (main entry point from training loop)
//=============================================================================

int nimcp_synapse_lifecycle_step(nimcp_synapse_lifecycle_t* mgr,
    void* network, uint32_t training_step,
    nimcp_synapse_lifecycle_report_t* report)
{
    if (!mgr || !network || !report) {
        return -1;
    }

    memset(report, 0, sizeof(*report));

    /* Pruning sweep */
    if (training_step - mgr->last_prune_step >= mgr->config.prune_interval_steps) {
        nimcp_synapse_lifecycle_prune(mgr, network, report);
        mgr->last_prune_step = training_step;
    }

    /* Growth sweep */
    if (training_step - mgr->last_growth_step >= mgr->config.growth_interval_steps) {
        nimcp_synapse_lifecycle_grow(mgr, network, report);
        mgr->last_growth_step = training_step;
    }

    /* Metadata GC */
    if (mgr->config.enable_metadata_gc &&
        training_step - mgr->last_gc_step >= mgr->config.gc_interval_steps) {
        nimcp_synapse_lifecycle_gc(mgr, network, report);
        mgr->last_gc_step = training_step;
    }

    return 0;
}

//=============================================================================
// Cumulative report
//=============================================================================

int nimcp_synapse_lifecycle_get_report(const nimcp_synapse_lifecycle_t* mgr,
    nimcp_synapse_lifecycle_report_t* report)
{
    if (!mgr || !report) {
        return -1;
    }

    memset(report, 0, sizeof(*report));
    report->synapses_pruned = mgr->total_pruned;
    report->synapses_grown  = mgr->total_grown;
    report->overflow_compacted = mgr->total_compacted;

    return 0;
}
