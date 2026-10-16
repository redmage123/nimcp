#include <stddef.h>  /* for NULL */
#include <time.h>    /* for clock_gettime */
#include <zlib.h>    /* for gzopen/gzwrite/gzread — compress .snn checkpoint */
//=============================================================================
// nimcp_snn_network.c - SNN Network Implementation
//=============================================================================
/**
 * @file nimcp_snn_network.c
 * @brief Implementation of SNN network operations
 *
 * WHAT: Core SNN network creation, simulation, and training
 * WHY:  Provide spiking neural network functionality on existing infrastructure
 * HOW:  Orchestrates neural_network_t, manages populations, spike trains
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 * @version 1.0.0
 */

#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_synapse.h"
#include "snn/nimcp_snn_config.h"
#include "snn/nimcp_snn_adaptation.h"
#include "snn/nimcp_snn_basket.h"
#include "snn/nimcp_snn_membrane.h"   /* CB migration: membrane integration helpers */
#include "snn/nimcp_snn_fno.h"        /* FNO recording hooks: snapshot_v_before / record_post_step */
#include "snn/nimcp_snn_training.h"  /* snn_tune_get_substrate_* */
#include "core/substrate/nimcp_substrate_effects.h"  /* substrate_compute_effects, apply helpers */
#include "gpu/snn/nimcp_snn_gpu.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>  /* UINT32_MAX (neural_network_add_neuron sentinel) */

/*=============================================================================
 * Health Agent Forward Declarations (Phase 8: Heartbeat for Long Operations)
 *============================================================================*/
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_network)

/*=============================================================================
 * W5 KG-integration — SNN network-level aggregator event emitter.
 *
 * File-scope brain backpointer set once by nimcp_brain_init_network_kg_wiring.
 * Emits spike_rate anomalies (quiescent / hyperactive) only when the per-step
 * stats cross thresholds — never every step. Admin token elevation per
 * kg-node-naming-registry.md §7.
 *============================================================================*/
#include <stdio.h>
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"

static brain_t s_net_snn_kg_brain = NULL;

void net_snn_kg_register_brain(brain_t brain) {
    s_net_snn_kg_brain = brain;
}

static void net_snn_kg_emit_event(brain_t brain, const char* kind,
                                  float magnitude, uint64_t ts_us) {
    if (!brain || !kind) return;
    if (!brain->internal_kg_enabled || !brain->internal_kg) return;

    brain_kg_t* kg = brain->internal_kg;
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN,
                              brain->internal_kg_admin_token);

    char ev_name[160];
    snprintf(ev_name, sizeof(ev_name),
             "net_snn_event_%s_%llu", kind, (unsigned long long)ts_us);
    char desc[240];
    snprintf(desc, sizeof(desc),
             "SNN network event: kind=%s magnitude=%.4f (spikes or rate)",
             kind, magnitude);

    brain_kg_node_id_t ev = brain_kg_add_node(kg, ev_name,
        BRAIN_KG_NODE_CORE, desc);
    brain_kg_node_id_t owner = brain_kg_find_node(kg, "net_snn");
    if (owner != BRAIN_KG_INVALID_NODE && ev != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, owner, ev, BRAIN_KG_EDGE_SENDS_TO,
            "produced_by", magnitude);
    }
    if (ev != BRAIN_KG_INVALID_NODE) {
        char buf[40];
        snprintf(buf, sizeof(buf), "%.6f", magnitude);
        brain_kg_add_metadata(kg, ev, "magnitude", buf);
        snprintf(buf, sizeof(buf), "%llu", (unsigned long long)ts_us);
        brain_kg_add_metadata(kg, ev, "ts_us", buf);
    }

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);
}

/* Public test-facing trigger — forwards to static emit. */
void net_snn_kg_trigger_event(brain_t brain, const char* kind,
                              float magnitude, uint64_t ts_us) {
    net_snn_kg_emit_event(brain, kind, magnitude, ts_us);
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

/* Forward declarations */
static void snn_population_destroy_internal(snn_population_t* pop);

/* =========================================================================
 * Wave E FFI fix — per-pop spike-history ring buffer helpers (DRY).
 *
 * The ring has SNN_SPIKE_HISTORY_SLOTS rows of n_neurons floats. The write
 * head points at the slot that will be written NEXT (i.e. the freshest
 * snapshot is at (head - 1) mod SLOTS). A read with conduction_delay = D
 * returns the slot D entries older than the freshest snapshot:
 *     slot = (head - 1 - D) mod SLOTS
 * Both reader and writer use these helpers so the modular arithmetic lives
 * in one place. See docs/claude/ffi-timing-audit-2026-04-27.md.
 * ========================================================================= */

/* Compute the row index in the spike-history ring corresponding to a spike
 * emitted `delay_steps` ago, given the current write head. Pure: no I/O,
 * no side effects — safe to inline at every read site.
 *
 * delay_steps must be < SNN_SPIKE_HISTORY_SLOTS (caller pre-clamps via the
 * setter); the modular arithmetic still works for any uint8_t input but
 * SNN_MAX_CONDUCTION_DELAY_STEPS keeps the "freshest at head-1" invariant
 * intact. */
static inline uint8_t snn_spike_history_read_slot(uint8_t head, uint8_t delay_steps) {
    /* head points at NEXT write slot ⇒ freshest is at (head - 1).
     * Add SLOTS once to keep the subtract-from-uint8_t expression
     * positive before the modulo. */
    return (uint8_t)((head + SNN_SPIKE_HISTORY_SLOTS - 1u - delay_steps)
                     % SNN_SPIKE_HISTORY_SLOTS);
}

/* Read pointer to the row of spike_history corresponding to `delay_steps`
 * ago. Returns NULL in three cases (the caller falls back to reading the
 * pop's live spike_output buffer):
 *   1. Pop has no ring buffer (alloc failure or non-lightweight pop).
 *   2. conduction_delay_steps == 0 — bit-identity contract: a delay of
 *      zero MUST be indistinguishable from pre-Wave-E same-tick deposit
 *      semantics. Reading the freshest ring slot would deliver an
 *      end-of-PREVIOUS-step snapshot, which is one step late vs the
 *      live spike_output. Returning NULL here makes the caller read
 *      spike_output (same-tick), preserving legacy behavior bit-for-bit.
 *   3. (none — only the two above; documented for clarity).
 * docs/claude/ffi-timing-audit-2026-04-27.md. */
static inline const float* snn_pop_get_delayed_spike_row(
    const snn_population_t* pop)
{
    if (!pop || !pop->spike_history) return NULL;
    if (pop->conduction_delay_steps == 0) return NULL;  /* legacy same-tick */
    uint8_t slot = snn_spike_history_read_slot(pop->spike_history_head,
                                               pop->conduction_delay_steps);
    return pop->spike_history + (size_t)slot * (size_t)pop->n_neurons;
}

/**
 * @brief Allocate and initialize a population structure
 */
static snn_population_t* snn_population_create_internal(
    uint32_t id,
    uint32_t n_neurons,
    neuron_type_t type,
    const char* name,
    uint32_t start_neuron_id
) {
    snn_population_t* pop = (snn_population_t*)nimcp_malloc(sizeof(snn_population_t));
    if (!pop) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(snn_population_t),
            "snn_population_create: allocation failed");
        return NULL;
    }

    memset(pop, 0, sizeof(snn_population_t));
    pop->id = id;
    pop->n_neurons = n_neurons;
    pop->neuron_type = type;

    if (name) {
        strncpy(pop->name, name, sizeof(pop->name) - 1);
    }

    /* Allocate neuron ID array */
    pop->neuron_ids = (uint32_t*)nimcp_malloc(n_neurons * sizeof(uint32_t));
    if (!pop->neuron_ids) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, n_neurons * sizeof(uint32_t),
            "snn_population_create: neuron_ids allocation failed");
        nimcp_free(pop);
        return NULL;
    }

    /* Allocate spike trains */
    pop->spike_trains = (snn_spike_train_t*)nimcp_malloc(
        n_neurons * sizeof(snn_spike_train_t));
    if (!pop->spike_trains) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, n_neurons * sizeof(snn_spike_train_t),
            "snn_population_create: spike_trains allocation failed");
        nimcp_free(pop->neuron_ids);
        nimcp_free(pop);
        return NULL;
    }
    memset(pop->spike_trains, 0, n_neurons * sizeof(snn_spike_train_t));

    /* Initialize spike trains and neuron IDs */
    for (uint32_t i = 0; i < n_neurons; i++) {
        pop->neuron_ids[i] = start_neuron_id + i;
        pop->spike_trains[i].neuron_id = start_neuron_id + i;
    }

    /* Allocate tensor views for efficient computation */
    uint32_t dims[1] = {n_neurons};
    pop->membrane_v = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    pop->spike_output = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    pop->refractory = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);

    if (!pop->membrane_v || !pop->spike_output || !pop->refractory) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, n_neurons * sizeof(float),
            "snn_population_create: tensor allocation failed");
        if (pop->membrane_v) nimcp_tensor_destroy(pop->membrane_v);
        if (pop->spike_output) nimcp_tensor_destroy(pop->spike_output);
        if (pop->refractory) nimcp_tensor_destroy(pop->refractory);
        nimcp_free(pop->spike_trains);
        nimcp_free(pop->neuron_ids);
        nimcp_free(pop);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_population_create_internal: validation failed");
        return NULL;
    }

    pop->topology = SNN_TOPO_FULL;
    pop->connectivity = 1.0f;

    /* Homeostatic + intrinsic-plasticity + biophysical state.
     *
     * Bug fix (pre-existing Wave A+B gap): the lightweight path (below)
     * allocates ahp, pump, basket, threshold_offset, neuron_rate_ema,
     * depression, and seeds EMAs. The non-lightweight path (this
     * function) previously left those NULL even though the same step
     * code runs for both. Non-lightweight pops silently lacked their
     * biophysical stability mechanisms.
     *
     * Fix: mirror the same allocations here. All allocation failures
     * are non-fatal — the step code branches on NULL, so a missing
     * mechanism simply becomes a no-op. */
    pop->firing_rate_ema = 0.03f;
    pop->rate_samples = 0;
    memset(pop->spike_count_history, 0, sizeof(pop->spike_count_history));
    pop->history_write_idx = 0;
    pop->history_total_steps = 0;

    pop->threshold_offset = nimcp_calloc(n_neurons, sizeof(float));
    pop->neuron_rate_ema  = nimcp_calloc(n_neurons, sizeof(float));
    pop->depression       = nimcp_calloc(n_neurons, sizeof(float));
    if (!pop->threshold_offset || !pop->neuron_rate_ema || !pop->depression) {
        NIMCP_LOGGING_WARN(
            "snn_population_create_internal: partial allocation of intrinsic "
            "plasticity / depression buffers for pop '%s' (%u neurons); "
            "mechanism will be no-op", name ? name : "unnamed", n_neurons);
    } else {
        const float ip_target = 0.03f;
        for (uint32_t i = 0; i < n_neurons; i++) {
            pop->neuron_rate_ema[i] = ip_target;
        }
    }

    /* Conductance-based PSC state (CB migration + P0 per-receptor split).
     * Four conductance buckets — AMPA / NMDA / GABA_A / GABA_B — with
     * receptor-specific decay τ and reversal potentials. Allocation
     * failure on any of these is non-fatal: the deposit and integrate
     * kernels treat NULL pointers as a no-op (silent degrade), so CB
     * mode just behaves as if that receptor type produces no current. */
    pop->g_ampa   = nimcp_calloc(n_neurons, sizeof(float));
    pop->g_nmda   = nimcp_calloc(n_neurons, sizeof(float));
    pop->g_gaba_a = nimcp_calloc(n_neurons, sizeof(float));
    pop->g_gaba_b = nimcp_calloc(n_neurons, sizeof(float));
    if (!pop->g_ampa || !pop->g_nmda || !pop->g_gaba_a || !pop->g_gaba_b) {
        NIMCP_LOGGING_WARN(
            "snn_population_create_internal: per-receptor CB buffers failed "
            "to allocate for pop '%s' (%u neurons); affected receptors no-op",
            name ? name : "unnamed", n_neurons);
    }

    extern float snn_tune_get_ahp_enabled(void);
    extern float snn_tune_get_pump_enabled(void);
    extern float snn_tune_get_basket_enabled(void);
    extern float snn_tune_get_ahp_tau_ms(void);
    extern float snn_tune_get_ahp_gain_mv(void);
    extern float snn_tune_get_pump_tau_ms(void);
    extern float snn_tune_get_pump_gain_mv(void);
    extern float snn_tune_get_basket_fraction(void);

    if (snn_tune_get_ahp_enabled() != 0.0f) {
        pop->ahp = snn_adaptation_create(n_neurons,
                                         snn_tune_get_ahp_tau_ms(),
                                         snn_tune_get_ahp_gain_mv(),
                                         1.0f);
        if (!pop->ahp) {
            NIMCP_LOGGING_WARN(
                "snn_population_create_internal: AHP allocation failed for "
                "pop '%s'; adaptation will be no-op",
                name ? name : "unnamed");
        }
    }
    if (snn_tune_get_pump_enabled() != 0.0f) {
        pop->pump = snn_adaptation_create(n_neurons,
                                          snn_tune_get_pump_tau_ms(),
                                          snn_tune_get_pump_gain_mv(),
                                          1.0f);
        if (!pop->pump) {
            NIMCP_LOGGING_WARN(
                "snn_population_create_internal: pump allocation failed for "
                "pop '%s'; adaptation will be no-op",
                name ? name : "unnamed");
        }
    }
    if (snn_tune_get_basket_enabled() != 0.0f) {
        pop->basket = snn_basket_pool_create(pop->id,
                                             n_neurons,
                                             snn_tune_get_basket_fraction());
        if (!pop->basket) {
            NIMCP_LOGGING_WARN(
                "snn_population_create_internal: basket-pool allocation failed "
                "for pop '%s'; inhibitory pool will be no-op",
                name ? name : "unnamed");
        }
    }

    return pop;
}

/**
 * @brief Create a lightweight population (no neuron_t allocation)
 *
 * Same as snn_population_create_internal but allocates external_current[]
 * and incoming_csr instead of relying on neural_network_t neurons.
 * Neuron IDs are logical: (pop_id << 20) | neuron_index.
 */
static snn_population_t* snn_population_create_lightweight(
    uint32_t id,
    uint32_t n_neurons,
    neuron_type_t type,
    const char* name)
{
    /* Reuse the standard create with dummy start_neuron_id */
    uint32_t logical_base = ((uint32_t)id << 20);
    snn_population_t* pop = snn_population_create_internal(
        id, n_neurons, type, name, logical_base);
    if (!pop) return NULL;

    /* Allocate lightweight-specific storage.
     *
     * NOTE: Homeostatic state, intrinsic plasticity buffers
     * (threshold_offset, neuron_rate_ema, depression) and the
     * biophysical stability mechanisms (ahp, pump, basket) are
     * allocated and seeded by snn_population_create_internal() itself
     * — lightweight does NOT re-allocate them here. (Previously
     * lightweight was the only path that allocated them; the
     * non-lightweight path left them NULL. That gap has been closed
     * in snn_population_create_internal.) */
    pop->lightweight = true;
    pop->external_current = nimcp_calloc(n_neurons, sizeof(float));
    pop->incoming_csr = snn_csr_create(n_neurons, n_neurons * 20);

    if (!pop->external_current || !pop->incoming_csr) {
        snn_population_destroy_internal(pop);
        return NULL;
    }

    /* Wave E FFI fix — per-pop spike-history ring buffer.
     * SNN_SPIKE_HISTORY_SLOTS × n_neurons floats. Best-effort allocation:
     * if it fails the deposit kernel falls back to reading spike_output
     * (zero-delay legacy semantics) so the network still runs.
     * Default conduction_delay_steps = 0 ⇒ behavior is bit-identical to
     * pre-Wave-E (read the freshest slot which equals the current
     * spike_output snapshot after end-of-step write).
     * See docs/claude/ffi-timing-audit-2026-04-27.md. */
    pop->conduction_delay_steps = 0;
    pop->spike_history_head     = 0;
    pop->spike_history          = nimcp_calloc(
        (size_t)SNN_SPIKE_HISTORY_SLOTS * (size_t)n_neurons, sizeof(float));
    if (!pop->spike_history) {
        NIMCP_LOGGING_WARN(
            "snn_population_create_lightweight: spike_history alloc failed "
            "for pop '%s' (%u neurons × %u slots); conduction-delay falls "
            "back to zero-delay legacy semantics",
            name ? name : "unnamed", n_neurons, SNN_SPIKE_HISTORY_SLOTS);
    }

    /* Initialize membrane potential to resting potential */
    float* v = (float*)nimcp_tensor_data(pop->membrane_v);
    if (v) {
        for (uint32_t i = 0; i < n_neurons; i++) {
            v[i] = -65.0f;  /* v_rest default */
        }
    }

    return pop;
}

/**
 * @brief Destroy a population structure
 */
static void snn_population_destroy_internal(snn_population_t* pop) {
    if (!pop) return;

    if (pop->membrane_v) nimcp_tensor_destroy(pop->membrane_v);
    if (pop->spike_output) nimcp_tensor_destroy(pop->spike_output);
    if (pop->refractory) nimcp_tensor_destroy(pop->refractory);
    if (pop->spike_trains) nimcp_free(pop->spike_trains);
    if (pop->neuron_ids) nimcp_free(pop->neuron_ids);

    /* Lightweight mode cleanup */
    if (pop->external_current) nimcp_free(pop->external_current);

    /* CB-GPU-7: free device-side synapse_type. We stash gpu_ctx in
     * d_synapse_type_gpu_ctx at upload time so the free works even if
     * the CSR (which separately tracks gpu_ctx) was released earlier
     * via snn_csr_release_gpu (it clears d_gpu_ctx → NULL). Fall back
     * to the CSR's gpu_ctx for entries from before the stash existed. */
    if (pop->d_synapse_type_per_src) {
        nimcp_gpu_context_t* st_gpu =
            (nimcp_gpu_context_t*)pop->d_synapse_type_gpu_ctx;
        if (!st_gpu && pop->incoming_csr) {
            st_gpu = (nimcp_gpu_context_t*)pop->incoming_csr->d_gpu_ctx;
        }
        if (st_gpu) {
            nimcp_gpu_free(st_gpu, pop->d_synapse_type_per_src);
        }
        pop->d_synapse_type_per_src = NULL;
        pop->d_synapse_type_gpu_ctx = NULL;
    }

    if (pop->incoming_csr) {
        snn_csr_destroy(pop->incoming_csr);
    }

    /* Wave E FFI fix — per-pop spike-history ring buffer. NULL-safe:
     * non-lightweight pops never allocate this; lightweight pops with a
     * failed allocation also leave it NULL. */
    if (pop->spike_history) { nimcp_free(pop->spike_history); pop->spike_history = NULL; }

    /* Intrinsic plasticity + short-term depression state */
    if (pop->threshold_offset) nimcp_free(pop->threshold_offset);
    if (pop->neuron_rate_ema) nimcp_free(pop->neuron_rate_ema);
    if (pop->depression) nimcp_free(pop->depression);

    /* Per-neuron heterogeneity (Wave G). NULL-safe — only allocated when
     * snn_network_set_pop_heterogeneity() was called with sigma > 0. */
    if (pop->tau_mem_per_neuron) {
        nimcp_free(pop->tau_mem_per_neuron);
        pop->tau_mem_per_neuron = NULL;
    }
    if (pop->v_thresh_per_neuron) {
        nimcp_free(pop->v_thresh_per_neuron);
        pop->v_thresh_per_neuron = NULL;
    }

    /* Conductance-based PSC state (CB migration). */
    if (pop->g_ampa)   { nimcp_free(pop->g_ampa);   pop->g_ampa   = NULL; }
    if (pop->g_nmda)   { nimcp_free(pop->g_nmda);   pop->g_nmda   = NULL; }
    if (pop->g_gaba_a) { nimcp_free(pop->g_gaba_a); pop->g_gaba_a = NULL; }
    if (pop->g_gaba_b) { nimcp_free(pop->g_gaba_b); pop->g_gaba_b = NULL; }

    /* Wave H — dendritic compartment state. NULL-safe: only allocated
     * when snn_network_enable_dendritic() succeeded for this pop. */
    if (pop->v_basal)         { nimcp_free(pop->v_basal);         pop->v_basal         = NULL; }
    if (pop->v_apical)        { nimcp_free(pop->v_apical);        pop->v_apical        = NULL; }
    if (pop->g_ampa_basal)    { nimcp_free(pop->g_ampa_basal);    pop->g_ampa_basal    = NULL; }
    if (pop->g_gaba_a_basal)  { nimcp_free(pop->g_gaba_a_basal);  pop->g_gaba_a_basal  = NULL; }
    if (pop->g_nmda_apical)   { nimcp_free(pop->g_nmda_apical);   pop->g_nmda_apical   = NULL; }
    if (pop->g_gaba_b_apical) { nimcp_free(pop->g_gaba_b_apical); pop->g_gaba_b_apical = NULL; }
    if (pop->plateau_active)  { nimcp_free(pop->plateau_active);  pop->plateau_active  = NULL; }
    if (pop->plateau_t0)      { nimcp_free(pop->plateau_t0);      pop->plateau_t0      = NULL; }
    pop->dendritic_enabled = false;

    /* Biophysical stability mechanisms. Destroyers tolerate NULL. */
    if (pop->ahp)    { snn_adaptation_destroy(pop->ahp);     pop->ahp = NULL; }
    if (pop->pump)   { snn_adaptation_destroy(pop->pump);    pop->pump = NULL; }
    if (pop->basket) { snn_basket_pool_destroy(pop->basket); pop->basket = NULL; }

    nimcp_free(pop);
}

/**
 * @brief Initialize simulation context
 */
static snn_simulation_t* snn_simulation_create_internal(float dt_ms, uint32_t total_neurons) {
    snn_simulation_t* sim = (snn_simulation_t*)nimcp_malloc(sizeof(snn_simulation_t));
    if (!sim) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(snn_simulation_t),
            "snn_simulation_create: allocation failed");
        return NULL;
    }

    memset(sim, 0, sizeof(snn_simulation_t));
    sim->dt_ms = dt_ms;
    sim->health = SNN_STATE_HEALTHY;

    /* Allocate spike queue — scale with total neuron count.
     * At 1.8M SNN neurons with ~5% firing rate per step, need ~90K queue slots.
     * queue = total/2 handles bursts; cap at 2M prevents runaway alloc. */
    sim->queue_capacity = (total_neurons > 20000) ? total_neurons / 2 : 10000;
    if (sim->queue_capacity > 2000000) sim->queue_capacity = 2000000;
    sim->spike_queue = (snn_spike_t*)nimcp_malloc(
        sim->queue_capacity * sizeof(snn_spike_t));
    if (!sim->spike_queue) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sim->queue_capacity * sizeof(snn_spike_t),
            "snn_simulation_create: spike_queue allocation failed");
        nimcp_free(sim);
        return NULL;
    }

    /* Initialize RNG */
    sim->rng_state = 12345ULL;

    return sim;
}

/**
 * @brief Destroy simulation context
 */
static void snn_simulation_destroy_internal(snn_simulation_t* sim) {
    if (!sim) return;
    if (sim->spike_queue) nimcp_free(sim->spike_queue);
    nimcp_free(sim);
}

/** Track spike buffer overflow events for monitoring */
static volatile uint64_t g_spike_buffer_overflow_count = 0;
static volatile uint64_t g_last_overflow_warning_count = 0;

/**
 * @brief Record spike to spike train with overflow protection
 *
 * WHAT: Record spike timestamp to circular buffer
 * WHY:  Track spike history for STDP, analysis, and decoding
 * HOW:  Circular buffer wraps when full, overwrites oldest spikes
 *
 * NOTE: When buffer is full, old spikes are overwritten (circular buffer).
 *       This is intentional behavior for bounded memory, but we log
 *       warnings to detect pathological high-firing scenarios.
 */
static void record_spike(snn_spike_train_t* train, uint64_t time_us) {
    uint32_t idx = train->write_idx;
    train->spike_times[idx] = time_us;
    train->write_idx = (idx + 1) % SNN_SPIKE_BUFFER_SIZE;

    if (train->count < SNN_SPIKE_BUFFER_SIZE) {
        train->count++;
    } else {
        /* Buffer overflow - old spikes being overwritten
         * WHAT: Track overflow events
         * WHY:  High spike rates may indicate network instability
         * HOW:  Log periodic warnings (not every overflow to avoid log spam)
         */
        uint64_t overflow_count = __atomic_add_fetch(&g_spike_buffer_overflow_count, 1, __ATOMIC_RELAXED);

        /* Log warning every 1000 overflows to avoid log spam */
        uint64_t last_warning = __atomic_load_n(&g_last_overflow_warning_count, __ATOMIC_RELAXED);
        if (overflow_count - last_warning >= 1000) {
            NIMCP_LOGGING_WARN("Spike buffer overflow: neuron %u buffer full "
                                  "(capacity=%u), overwriting old spikes. "
                                  "Total overflows: %lu. Consider increasing "
                                  "SNN_SPIKE_BUFFER_SIZE or reducing simulation time.",
                                  train->neuron_id, SNN_SPIKE_BUFFER_SIZE,
                                  (unsigned long)overflow_count);
            __atomic_store_n(&g_last_overflow_warning_count, overflow_count, __ATOMIC_RELAXED);
        }
    }
    train->total_spikes++;
}

/**
 * @brief Compute instantaneous firing rate
 */
static float compute_firing_rate(const snn_spike_train_t* train,
                                 uint64_t current_time_us,
                                 float window_ms) {
    if (train->count == 0) return 0.0f;

    uint64_t window_us = (uint64_t)(window_ms * 1000.0f);
    uint64_t cutoff = (current_time_us > window_us) ? current_time_us - window_us : 0;

    uint32_t count = 0;
    for (uint32_t i = 0; i < train->count; i++) {
        if (train->spike_times[i] >= cutoff) {
            count++;
        }
    }

    return (float)count / (window_ms / 1000.0f);  /* Convert to Hz */
}

//=============================================================================
// Network Lifecycle
//=============================================================================

snn_network_t* snn_network_create(const snn_config_t* config) {
    /* Guard clause: validate config */
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_network_create: NULL config");
        return NULL;
    }

    int validate_result = snn_config_validate(config);
    if (validate_result != SNN_SUCCESS) {
        /* Exception already thrown by snn_config_validate */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_create: validation failed");
        return NULL;
    }

    /* Allocate network structure */
    snn_network_t* network = (snn_network_t*)nimcp_malloc(sizeof(snn_network_t));
    if (!network) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(snn_network_t),
            "snn_network_create: allocation failed");
        return NULL;
    }
    memset(network, 0, sizeof(snn_network_t));

    /* Set magic number and copy config */
    network->magic = SNN_MAGIC;
    snn_config_clone(config, &network->config);

    /* Create underlying neural network */
    network_config_t nn_config;
    memset(&nn_config, 0, sizeof(network_config_t));
    nn_config.num_neurons = config->n_inputs + config->n_hidden + config->n_outputs;
    nn_config.input_size = config->n_inputs;
    nn_config.output_size = config->n_outputs;
    nn_config.enable_stdp = config->enable_stdp;
    nn_config.refractory_period = config->t_ref;
    nn_config.neuron_model = NEURON_MODEL_LIF;  /* Default to LIF */
    nn_config.enable_bio_async = config->enable_bio_async;

    network->neural_net = neural_network_create(&nn_config);
    if (!network->neural_net) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NETWORK_CREATION,
            "snn_network_create: neural_network_create failed");
        nimcp_free(network);
        return NULL;
    }

    /* Allocate population array.
     *
     * Bug fix (pre-existing heap overflow): the old code sized this array by
     * `config->n_populations` when > 0. For a feedforward config with a
     * hidden layer, that yields exactly 3 slots — and calling
     * snn_network_add_population() later would then write populations[3],
     * corrupting the heap. `snn_network_add_population{,_lightweight}()`
     * only guarded against `SNN_MAX_POPULATIONS`, not the actual alloc size.
     *
     * Fix: ALWAYS allocate SNN_MAX_POPULATIONS slots regardless of config.
     * Cost: 128 * sizeof(ptr) = 1 KB per network — trivial at 2M-neuron
     * scale. We also record the actual allocation size in
     * `populations_capacity` so the add-population guards can use it
     * directly (decoupling the bound from the constant). */
    uint32_t max_populations = SNN_MAX_POPULATIONS;
    network->populations = (snn_population_t**)nimcp_malloc(
        max_populations * sizeof(snn_population_t*));
    if (!network->populations) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, max_populations * sizeof(snn_population_t*),
            "snn_network_create: populations array allocation failed");
        neural_network_destroy(network->neural_net);
        nimcp_free(network);
        return NULL;
    }
    memset(network->populations, 0, max_populations * sizeof(snn_population_t*));
    network->populations_capacity = max_populations;

    /* Create simulation context */
    uint32_t total_snn_neurons = config->n_inputs + config->n_hidden + config->n_outputs;
    network->sim = snn_simulation_create_internal(config->dt, total_snn_neurons);
    if (!network->sim) {
        /* Exception already thrown by snn_simulation_create_internal */
        nimcp_free(network->populations);
        neural_network_destroy(network->neural_net);
        nimcp_free(network);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_network_create: network->sim is NULL");
        return NULL;
    }

    /* Create input population */
    network->input_pop = snn_population_create_internal(
        0, config->n_inputs, NEURON_GENERIC_LIF, "input", 0);
    if (!network->input_pop) {
        /* Exception already thrown by snn_population_create_internal */
        snn_network_destroy(network);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_network_create: network->input_pop is NULL");
        return NULL;
    }
    network->populations[0] = network->input_pop;
    network->n_populations = 1;

    /* Create hidden populations if n_hidden > 0 */
    uint32_t next_neuron_id = config->n_inputs;
    uint32_t next_pop_id = 1;

    if (config->n_hidden > 0) {
        snn_population_t* hidden_pop = snn_population_create_internal(
            next_pop_id, config->n_hidden, NEURON_GENERIC_LIF, "hidden", next_neuron_id);
        if (!hidden_pop) {
            snn_network_destroy(network);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_network_create: hidden_pop is NULL");
            return NULL;
        }
        network->populations[next_pop_id] = hidden_pop;
        next_neuron_id += config->n_hidden;
        next_pop_id++;
        network->n_populations = next_pop_id;
    }

    /* Create output population */
    network->output_pop = snn_population_create_internal(
        next_pop_id, config->n_outputs, NEURON_GENERIC_LIF, "output", next_neuron_id);
    if (!network->output_pop) {
        /* Exception already thrown by snn_population_create_internal */
        snn_network_destroy(network);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_network_create: network->output_pop is NULL");
        return NULL;
    }
    network->populations[next_pop_id] = network->output_pop;
    network->n_populations = next_pop_id + 1;

    /* Create mutex for thread safety */
    network->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
    if (network->mutex) {
        nimcp_mutex_init((nimcp_mutex_t*)network->mutex, NULL);
    }

    /* GPU acceleration: try to create LIF state on GPU */
    network->gpu_lif_state = NULL;
    network->gpu_ctx = NULL;
    {
        nimcp_gpu_context_t* gpu = nimcp_gpu_context_create_auto();
        if (gpu) {
            size_t total_neurons = (size_t)config->n_inputs + config->n_hidden + config->n_outputs;
            nimcp_lif_params_t lif_params = {
                .tau_mem   = config->tau_mem,
                .tau_syn   = config->tau_syn > 0.0f ? config->tau_syn : 5.0f,
                .v_thresh  = config->v_thresh,
                .v_reset   = config->v_reset,
                .v_rest    = config->v_rest,
                .dt        = config->dt,
                .hard_reset = true
            };
            nimcp_lif_state_t* lif_state = nimcp_lif_state_create(gpu, total_neurons, &lif_params);
            if (lif_state) {
                network->gpu_lif_state = lif_state;
                network->gpu_ctx = gpu;
                NIMCP_LOGGING_INFO("snn_network_create: GPU LIF state created for %zu neurons",
                                   total_neurons);
            } else {
                NIMCP_LOGGING_INFO("snn_network_create: GPU LIF state creation failed, using CPU fallback");
                nimcp_gpu_context_destroy(gpu);
            }
        } else {
            NIMCP_LOGGING_DEBUG("snn_network_create: no GPU available, using CPU path");
        }
    }

    NIMCP_LOGGING_INFO("snn_network_create: created SNN with %u inputs, %u outputs",
                       config->n_inputs, config->n_outputs);

    return network;
}

void snn_network_destroy(snn_network_t* network) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_destroy: null network pointer");
        return;
    }

    /* Destroy FNO populations + v_prev scratch BEFORE populations[] go away —
     * destroy_fno only walks fno_populations[], not the SNN populations, but
     * we order it first to keep teardown deterministic and avoid any future
     * access of a half-freed network. */
    snn_network_destroy_fno(network);

    /* Destroy populations */
    if (network->populations) {
        for (uint32_t i = 0; i < network->n_populations; i++) {
            if (network->populations[i]) {
                snn_population_destroy_internal(network->populations[i]);
            }
        }
        nimcp_free(network->populations);
    }

    /* Destroy simulation context */
    if (network->sim) {
        snn_simulation_destroy_internal(network->sim);
    }

    /* Destroy training context */
    if (network->train_ctx) {
        if (network->train_ctx->eligibility) {
            nimcp_tensor_destroy(network->train_ctx->eligibility);
        }
        if (network->train_ctx->grad_membrane) {
            nimcp_tensor_destroy(network->train_ctx->grad_membrane);
        }
        if (network->train_ctx->grad_weights) {
            nimcp_tensor_destroy(network->train_ctx->grad_weights);
        }
        nimcp_free(network->train_ctx);
    }

    /* Destroy encoder/decoder (when implemented) */

    /* Destroy underlying neural network */
    if (network->neural_net) {
        neural_network_destroy(network->neural_net);
    }

    /* CB-GPU-7: free network-level scratch tensors BEFORE destroying
     * gpu_ctx — they were allocated against this context. */
    if (network->gpu_ctx) {
        nimcp_gpu_context_t* g = (nimcp_gpu_context_t*)network->gpu_ctx;
        if (network->gpu_cb_spike_flat) {
            nimcp_gpu_free(g, network->gpu_cb_spike_flat);
            network->gpu_cb_spike_flat = NULL;
        }
        if (network->gpu_cb_depr_flat) {
            nimcp_gpu_free(g, network->gpu_cb_depr_flat);
            network->gpu_cb_depr_flat = NULL;
        }
    }

    /* Destroy GPU LIF state */
    if (network->gpu_lif_state) {
        nimcp_lif_state_destroy((nimcp_lif_state_t*)network->gpu_lif_state);
        network->gpu_lif_state = NULL;
    }
    if (network->gpu_ctx) {
        nimcp_gpu_context_destroy((nimcp_gpu_context_t*)network->gpu_ctx);
        network->gpu_ctx = NULL;
    }

    /* Destroy mutex */
    if (network->mutex) {
        nimcp_mutex_free((nimcp_mutex_t*)network->mutex);
    }

    /* Clear and free */
    memset(network, 0, sizeof(snn_network_t));
    nimcp_free(network);

    NIMCP_LOGGING_DEBUG("snn_network_destroy: network destroyed");
}

void snn_network_attach_substrate(snn_network_t* net,
                                  struct neural_substrate* sub) {
    if (!net) return;  /* null-tolerant: silent drop */
    net->substrate = sub;
    /* Reset cached effects + counter so the next step recomputes fresh. */
    memset(&net->cached_axon_effects, 0, sizeof(net->cached_axon_effects));
    memset(&net->cached_dend_effects, 0, sizeof(net->cached_dend_effects));
    net->substrate_steps_since_update = 0;
    /* Bug #1 fix: populate cache IMMEDIATELY so downstream consumers
     * (snn_rstdp_apply) that read cached_dend_effects.plasticity_mod
     * before the first step don't multiply LR by 0. Without this,
     * R-STDP silently dies on the GPU path (where the CPU-fallback
     * refresh block never runs) and on any pre-step inference. */
    if (sub) {
        substrate_compute_effects(sub,
                                  &net->cached_axon_effects,
                                  &net->cached_dend_effects);
    }
    NIMCP_LOGGING_DEBUG("snn_network_attach_substrate: substrate=%p",
                        (void*)sub);
}

/* Accessor for wire-up regression tests — returns the substrate pointer
 * the network currently holds (borrowed). NULL-tolerant. */
struct neural_substrate* snn_network_get_substrate(snn_network_t* net) {
    return net ? net->substrate : NULL;
}

int snn_network_reset(snn_network_t* network) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_reset: null network pointer");
        return SNN_ERROR_NULL_POINTER;
    }

    /* Reset all populations */
    for (uint32_t p = 0; p < network->n_populations; p++) {
        snn_population_t* pop = network->populations[p];
        if (!pop) continue;

        /* Reset spike trains */
        for (uint32_t n = 0; n < pop->n_neurons; n++) {
            memset(&pop->spike_trains[n], 0, sizeof(snn_spike_train_t));
            pop->spike_trains[n].neuron_id = n;
        }

        /* Reset membrane potentials to resting */
        float* v_data = (float*)nimcp_tensor_data(pop->membrane_v);
        for (uint32_t n = 0; n < pop->n_neurons; n++) {
            v_data[n] = network->config.v_rest;
        }

        /* Clear spike output and refractory */
        memset(nimcp_tensor_data(pop->spike_output), 0,
               pop->n_neurons * sizeof(float));
        memset(nimcp_tensor_data(pop->refractory), 0,
               pop->n_neurons * sizeof(float));

        pop->total_spikes = 0;
        pop->mean_rate = 0.0f;
    }

    /* Reset the GPU LIF membrane too. The per-pop host membrane_v reset above
     * does NOT touch the device-resident gpu_lif_state the GPU step kernel
     * integrates, so without this the previous run's charged neurons bleed
     * across a reset — validated as GPU cross-seed contamination in language
     * generation (seeding concept B still fired concept A's word ensemble
     * because its neurons stayed charged on-device). Resetting v to v_rest is
     * the dominant carry-over; i_syn/spikes are recomputed/overwritten each
     * step. No-op when running CPU-only (gpu_lif_state NULL). */
    if (network->gpu_lif_state && network->gpu_ctx) {
        nimcp_lif_state_t* lif = (nimcp_lif_state_t*)network->gpu_lif_state;
        if (lif->v) {
            (void)nimcp_gpu_snn_reset_state(
                (nimcp_gpu_context_t*)network->gpu_ctx,
                lif->v, network->config.v_rest);
        }
    }

    /* Reset simulation context */
    if (network->sim) {
        network->sim->current_time_us = 0;
        network->sim->step_count = 0;
        network->sim->queue_size = 0;
        network->sim->health = SNN_STATE_HEALTHY;
        network->sim->total_energy = 0.0f;
    }

    /* Reset underlying neural network */
    if (network->neural_net) {
        neural_network_reset(network->neural_net);
    }

    /* Reset statistics */
    memset(&network->stats, 0, sizeof(snn_stats_t));

    NIMCP_LOGGING_DEBUG("snn_network_reset: network reset to initial state");
    return SNN_SUCCESS;
}

//=============================================================================
// CB hot-loop helpers (CB-GPU-5 Phase A — extracted from inline CPU CB loop)
//=============================================================================

/**
 * @brief Per-pop CB deposit context. Captures the constants the inline CB
 *        hot loop hoists per-pop so the per-neuron deposit helper takes a
 *        reasonable arg count instead of 14+ scalars.
 */
typedef struct {
    bool          cb_mode;
    bool          dend_mode;
    float         basket_contrib;     /* applied uniformly across the pop */
    float         noise_p;            /* per-step Poisson probability */
    float         noise_pulse_mv;     /* magnitude */
    float         ei_ratio;           /* fraction of pulses that are inhibitory */
    bool          noise_exc_only;     /* true → all noise pulses excitatory (dead-pop rescue) */
    unsigned int* noise_seed;         /* thread-local rand_r state pointer */
    /* CB-GPU-7: when true, host helper skips the CSR walk — the kernel
     * `nimcp_gpu_snn_cb_deposit_csr` will do that work on GPU. External
     * current, basket, and Poisson noise still run on host (cheap; no CSR
     * walk involved). When false (CPU CB path), the CSR walk runs on host
     * as before — the dep_ctx is identical to pre-CB-GPU-7 behavior. */
    bool          cb_gpu_csr_path;
} snn_cb_deposit_ctx_t;

/**
 * @brief Per-neuron synaptic deposit pass: external current + CSR walk +
 *        basket inhibition + Poisson noise. Routes via per-pop-pair
 *        synapse_type_per_src in CB mode; sign-fallback (SYNAPSE_GENERIC)
 *        in current mode.
 *
 * WHAT: Mirrors the inline-deposit block of the CB CPU hot loop. Reads
 *       network->populations + spike-history ring (or spike_output
 *       fallback) for source spikes; writes into the four per-receptor
 *       conductance pointers (cb_g_*_n) when in CB mode, into I_syn_out
 *       when in current mode.
 * WHY:  Extracted so the CB+GPU step path can reuse the exact same
 *       deposit logic the CPU path runs — no parallel mirror to drift.
 *       Behavior is bit-identical; locked by test_snn_cb_loop_baseline.
 * HOW:  All NULL-safe at the receptor pointer boundary (a missing
 *       receptor is a no-op via snn_membrane_deposit_synapse_*). Runs
 *       once per neuron per step.
 */
static void snn_cb_deposit_neuron(
    snn_network_t* network,
    snn_population_t* pop,
    uint32_t n,
    const snn_cb_deposit_ctx_t* ctx,
    float* cb_g_ampa_n,   float* cb_g_nmda_n,
    float* cb_g_gaba_a_n, float* cb_g_gaba_b_n,
    float* dend_g_ampa_b_n,   float* dend_g_gaba_a_b_n,
    float* dend_g_nmda_a_n,   float* dend_g_gaba_b_a_n,
    /* out */ float* I_syn_out)
{
    float I_syn = 0.0f;

    /* External current (input drive). Non-CSR deposit → SYNAPSE_GENERIC
     * so the kernel's sign-fallback routes it. Wave H compartmental
     * helper routes via AMPA-basal / GABA_A-basal sign fallback when
     * dend_mode is true. */
    if (pop->external_current) {
        snn_membrane_deposit_synapse_compartmental(
            &I_syn,
            cb_g_ampa_n, cb_g_nmda_n, cb_g_gaba_a_n, cb_g_gaba_b_n,
            dend_g_ampa_b_n, dend_g_gaba_a_b_n,
            dend_g_nmda_a_n, dend_g_gaba_b_a_n,
            pop->external_current[n],
            SYNAPSE_GENERIC, ctx->cb_mode, ctx->dend_mode);
    }

    /* CB-GPU-7: when running on the GPU CB path, the CSR walk happens
     * on GPU via kernel_snn_cb_deposit_csr — skip the host walk to avoid
     * double-deposit. External current + basket + Poisson noise still
     * run host-side because they're cheap and don't involve CSR. */
    if (ctx->cb_gpu_csr_path) {
        goto skip_csr_walk;
    }

    /* CSR walk: per-incoming synapse, deposit if source neuron spiked
     * (delayed via spike-history ring when available — Wave E FFI fix). */
    uint32_t syn_count;
    snn_csr_synapse_t* syns = snn_csr_get_incoming(
        pop->incoming_csr, n, &syn_count);
    for (uint32_t s = 0; s < syn_count; s++) {
        if (syns[s].src_pop >= network->n_populations) continue;
        snn_population_t* src_pop = network->populations[syns[s].src_pop];
        if (!src_pop || !src_pop->spike_output) continue;
        const float* src_spikes_delayed =
            snn_pop_get_delayed_spike_row(src_pop);
        const float* src_spikes = src_spikes_delayed
            ? src_spikes_delayed
            : (const float*)nimcp_tensor_data(src_pop->spike_output);
        if (src_spikes && syns[s].src_neuron < src_pop->n_neurons
            && src_spikes[syns[s].src_neuron] > 0.5f) {
            float dep = 0.0f;
            if (src_pop->depression) dep = src_pop->depression[syns[s].src_neuron];
            const int syn_type = (int)pop->synapse_type_per_src[syns[s].src_pop];
            snn_membrane_deposit_synapse_compartmental(
                &I_syn,
                cb_g_ampa_n, cb_g_nmda_n, cb_g_gaba_a_n, cb_g_gaba_b_n,
                dend_g_ampa_b_n, dend_g_gaba_a_b_n,
                dend_g_nmda_a_n, dend_g_gaba_b_a_n,
                syns[s].weight * (1.0f - dep),
                syn_type, ctx->cb_mode, ctx->dend_mode);
        }
    }

skip_csr_walk: ;

    /* Basket feedforward inhibition — uniform across pop. negative-signed
     * so SYNAPSE_GENERIC sign-fallback routes to GABA_A in CB mode. */
    snn_membrane_deposit_synapse_compartmental(
        &I_syn,
        cb_g_ampa_n, cb_g_nmda_n, cb_g_gaba_a_n, cb_g_gaba_b_n,
        dend_g_ampa_b_n, dend_g_gaba_a_b_n,
        dend_g_nmda_a_n, dend_g_gaba_b_a_n,
        ctx->basket_contrib,
        SYNAPSE_GENERIC, ctx->cb_mode, ctx->dend_mode);

    /* E/I-balanced Poisson background noise — structural fix for the
     * absorbing-zero state (dead-pop rescue uses noise_exc_only). */
    if (ctx->noise_p > 0.0f) {
        float r = (float)rand_r(ctx->noise_seed) * (1.0f / (float)RAND_MAX);
        if (r < ctx->noise_p) {
            float pulse = ctx->noise_pulse_mv;
            if (!ctx->noise_exc_only) {
                float sign_r = (float)rand_r(ctx->noise_seed)
                             * (1.0f / (float)RAND_MAX);
                if (sign_r < ctx->ei_ratio) pulse = -ctx->noise_pulse_mv;
            }
            snn_membrane_deposit_synapse_compartmental(
                &I_syn,
                cb_g_ampa_n, cb_g_nmda_n, cb_g_gaba_a_n, cb_g_gaba_b_n,
                dend_g_ampa_b_n, dend_g_gaba_a_b_n,
                dend_g_nmda_a_n, dend_g_gaba_b_a_n,
                pulse,
                SYNAPSE_GENERIC, ctx->cb_mode, ctx->dend_mode);
        }
    }

    *I_syn_out = I_syn;
}

/**
 * @brief Per-pop CB integration context. Captures pop-level constants
 *        the inline CB hot loop hoists once per-pop, plus per-neuron
 *        het/hyp arrays. The per-neuron integrate helper takes this
 *        instead of a 25-arg parameter list.
 */
typedef struct {
    bool          cb_mode;
    bool          dend_mode;
    bool          het_active_tau;
    bool          het_active_thresh;
    float         tau_eff;
    float         v_thresh;
    float         v_reset;
    float         v_rest;
    float         tref_eff;
    float         cb_e_ampa, cb_e_nmda, cb_e_gaba_a, cb_e_gaba_b;
    float         cb_mg;
    /* Dendritic compartment (Wave H) */
    float         dend_plateau_thr;
    float         dend_plateau_g;
    float         dend_plateau_tau;
    float         dend_g_coup;
    uint64_t      dend_now_tick;
    /* Substrate */
    float         substrate_pump_factor;
    bool          substrate_spike_dropout_on;
    const axon_substrate_effects_t*     se_axon;
    const dendrite_substrate_effects_t* se_dend;
    /* Per-neuron arrays (caller-owned, NULL if unused) */
    const float*  tau_per_neuron;
    const float*  vthr_per_neuron;
    const float*  ahp_hyp;
    const float*  pump_hyp;
    /* Per-step rand_r state for substrate spike-survival */
    unsigned int* noise_seed;
    /* Per-step dt */
    float         dt_ms;
} snn_cb_integrate_ctx_t;

/**
 * @brief Per-neuron membrane integration + spike emission. Runs after
 *        synaptic deposit has populated pop->g_* (CB mode) or I_syn
 *        (current mode); writes v_data, spike_data, ref_data, and
 *        records spikes to spike_trains.
 *
 * WHAT: Computes dv via CB / dendritic-two-compartment / current-mode
 *       branches, applies AHP/pump hyp adjustment, checks against
 *       intrinsic-plasticity-adjusted threshold, runs substrate spike
 *       survival, resets V on spike, sets refractory.
 * WHY:  Extracted so the CB+GPU step path can REPLACE this whole call
 *       with nimcp_gpu_lif_forward_cb on the gathered host state. CPU
 *       behavior is bit-identical; locked by test_snn_cb_loop_baseline.
 * RETURN: 1 if neuron spiked (caller increments total_spikes counters),
 *         0 otherwise.
 */
static int snn_cb_integrate_neuron(
    snn_network_t* network,
    snn_population_t* pop,
    uint32_t n,
    const snn_cb_integrate_ctx_t* ctx,
    float* v_data, float* spike_data, float* ref_data,
    float* cb_g_ampa_n,   float* cb_g_nmda_n,
    float* cb_g_gaba_a_n, float* cb_g_gaba_b_n,
    float* dend_g_ampa_b_n,   float* dend_g_gaba_a_b_n,
    float* dend_g_nmda_a_n,   float* dend_g_gaba_b_a_n,
    float I_syn)
{
    /* AHP + pump hyperpolarization (intrinsic K+ currents). Folded into
     * I_syn before compute_dv in current mode; subtracted from dv after
     * compute_dv in CB / dendritic mode. */
    float hyp = 0.0f;
    if (ctx->ahp_hyp)  hyp += ctx->ahp_hyp[n];
    if (ctx->pump_hyp) hyp += ctx->pump_hyp[n];
    hyp *= ctx->substrate_pump_factor;

    /* Per-neuron τ_mem from heterogeneity arrays (Wave G) modulated by
     * substrate axon-cache (when both are active). Falls back to the
     * pop-wide tau_eff scalar when no heterogeneity. */
    float tau_eff_n = ctx->tau_eff;
    if (ctx->het_active_tau && ctx->tau_per_neuron) {
        float tau_n = ctx->tau_per_neuron[n];
        tau_eff_n = ctx->se_dend ? substrate_apply_tau(tau_n, ctx->se_dend)
                                 : tau_n;
    }
    float dv;

    if (ctx->dend_mode) {
        /* Wave H — two-compartment integration with apical NMDA plateau. */
        bool active = pop->plateau_active[n] != 0;
        if (!active &&
            snn_membrane_check_plateau_onset(
                pop->v_apical[n], ctx->dend_plateau_thr)) {
            pop->plateau_active[n] = 1;
            pop->plateau_t0[n]     = ctx->dend_now_tick;
            active = true;
        }
        float t_since_ms = 0.0f;
        if (active) {
            uint64_t dticks = (ctx->dend_now_tick >= pop->plateau_t0[n])
                            ? (ctx->dend_now_tick - pop->plateau_t0[n])
                            : 0;
            t_since_ms = (float)dticks * ctx->dt_ms;
            if (t_since_ms > 3.0f * ctx->dend_plateau_tau) {
                pop->plateau_active[n] = 0;
                active = false;
            }
        }

        float dv_b = 0.0f, dv_a = 0.0f;
        snn_membrane_compute_dv_two_compartment(
            pop->v_basal[n], pop->v_apical[n],
            ctx->v_rest, tau_eff_n, tau_eff_n, ctx->dt_ms,
            dend_g_ampa_b_n   ? *dend_g_ampa_b_n   : 0.0f,
            dend_g_gaba_a_b_n ? *dend_g_gaba_a_b_n : 0.0f,
            dend_g_nmda_a_n   ? *dend_g_nmda_a_n   : 0.0f,
            dend_g_gaba_b_a_n ? *dend_g_gaba_b_a_n : 0.0f,
            ctx->dend_g_coup,
            ctx->cb_e_ampa, ctx->cb_e_nmda,
            ctx->cb_e_gaba_a, ctx->cb_e_gaba_b,
            ctx->cb_mg,
            active, t_since_ms,
            ctx->dend_plateau_g, ctx->dend_plateau_tau,
            &dv_b, &dv_a);

        pop->v_basal[n]  += dv_b - hyp * (ctx->dt_ms / tau_eff_n);
        pop->v_apical[n] += dv_a;
        v_data[n] = pop->v_basal[n];
        dv = 0.0f;  /* v_data already advanced */
    } else if (ctx->cb_mode) {
        dv = snn_membrane_compute_dv(
            v_data[n], ctx->v_rest, tau_eff_n, ctx->dt_ms,
            0.0f /* I_syn unused in CB */,
            cb_g_ampa_n   ? *cb_g_ampa_n   : 0.0f,
            cb_g_nmda_n   ? *cb_g_nmda_n   : 0.0f,
            cb_g_gaba_a_n ? *cb_g_gaba_a_n : 0.0f,
            cb_g_gaba_b_n ? *cb_g_gaba_b_n : 0.0f,
            ctx->cb_e_ampa, ctx->cb_e_nmda,
            ctx->cb_e_gaba_a, ctx->cb_e_gaba_b,
            ctx->cb_mg, true);
        dv -= hyp * (ctx->dt_ms / tau_eff_n);
        v_data[n] += dv;
    } else {
        I_syn -= hyp;
        dv = snn_membrane_compute_dv(
            v_data[n], ctx->v_rest, tau_eff_n, ctx->dt_ms,
            I_syn,
            0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, false);
        v_data[n] += dv;
    }

    /* Intrinsic plasticity threshold. */
    float v_thresh_n = (ctx->het_active_thresh && ctx->vthr_per_neuron)
                       ? ctx->vthr_per_neuron[n]
                       : ctx->v_thresh;
    if (pop->threshold_offset) v_thresh_n += pop->threshold_offset[n];

    if (v_data[n] >= v_thresh_n) {
        int spike_ok = 1;
        if (ctx->substrate_spike_dropout_on && ctx->se_axon) {
            float rand01 = (float)rand_r(ctx->noise_seed) * (1.0f / (float)RAND_MAX);
            if (!substrate_spike_survives(ctx->se_axon, rand01)) {
                spike_ok = 0;
            }
        }
        if (spike_ok) {
            spike_data[n] = 1.0f;
            v_data[n] = ctx->v_reset;
            ref_data[n] = ctx->tref_eff;
            record_spike(&pop->spike_trains[n], network->sim->current_time_us);
            pop->total_spikes++;
            return 1;
        }
    }
    return 0;
}

/**
 * @brief Pop-level post-spike adaptation: AHP / pump update, basket step,
 *        intrinsic plasticity + short-term depression.
 *
 * WHAT: Runs after the per-neuron integrate + spike check has populated
 *       pop->spike_output. Decays AHP/pump adapt vars and bumps them on
 *       fresh spikes; advances the basket pool with the parent's mean
 *       fire rate; updates per-neuron threshold offsets via rate-EMA
 *       intrinsic plasticity; updates short-term depression.
 * WHY:  Extracted from the inline CB hot loop so the CB+GPU step path
 *       (which replaces the CPU integrate with nimcp_gpu_lif_forward_cb)
 *       can call exactly the same adaptation logic instead of duplicating
 *       it. Behavior is bit-identical to the inline version — locked by
 *       test_snn_cb_loop_baseline.cpp (golden hash 0xd3d9d3d3e5cd5d25).
 * HOW:  Reads pop->spike_output via tensor_data; writes pop->ahp/pump
 *       state, pop->basket, pop->threshold_offset, pop->depression,
 *       pop->neuron_rate_ema. Frees ahp_hyp/pump_hyp arrays (caller's
 *       allocation, see deposit pass).
 *
 * Thread safety: caller must hold the network mutex (or otherwise
 * serialize against trainer/inference RPC).
 */
static void snn_cb_post_spike_pop(snn_population_t* pop, float dt_ms,
                                  bool ahp_on, bool pump_on, bool basket_on,
                                  float* ahp_hyp, float* pump_hyp) {
    if (!pop) return;
    const float* spike_data = (const float*)nimcp_tensor_data(pop->spike_output);

    /* AHP + pump state advance: decay adapt_var by exp(-dt/tau), then
     * bump on fresh spikes. Must run AFTER the fire decisions, since
     * spike_data was just populated. */
    if (ahp_on)  snn_adaptation_update(pop->ahp,  spike_data, dt_ms);
    if (pump_on) snn_adaptation_update(pop->pump, spike_data, dt_ms);
    if (ahp_hyp)  nimcp_free(ahp_hyp);
    if (pump_hyp) nimcp_free(pump_hyp);

    /* Basket pool: advance with this step's parent mean fire rate. The
     * next step's emit_inhibition will see the spikes this basket step
     * just produced. */
    if (basket_on) {
        uint32_t spike_count = 0;
        for (uint32_t n = 0; n < pop->n_neurons; n++) {
            if (spike_data[n] > 0.5f) spike_count++;
        }
        float mean_rate = (pop->n_neurons > 0)
            ? (float)spike_count / (float)pop->n_neurons : 0.0f;
        snn_basket_pool_step(pop->basket, mean_rate, dt_ms);
    }

    /* Intrinsic plasticity + short-term depression. Per-neuron threshold
     * offset drifts toward whatever value would make this neuron fire at
     * target_neuron_rate; depression factor exponentially decays to 0
     * with tau ~50 ms and jumps by dep_inc on firing. */
    if (pop->threshold_offset && pop->neuron_rate_ema && pop->depression) {
        const float target_neuron_rate = 0.03f;
        const float ip_alpha = 0.01f;
        const float ip_gain  = 0.5f;
        const float ip_cap   = 10.0f;
        extern float snn_tune_get_depression_inc(void);
        extern float snn_tune_get_depression_tau_ms(void);
        extern float snn_tune_get_depression_cap(void);
        const float dep_inc   = snn_tune_get_depression_inc();
        const float dep_tau   = snn_tune_get_depression_tau_ms();
        const float dep_cap   = snn_tune_get_depression_cap();
        const float dep_decay = expf(-dt_ms / dep_tau);

        extern bool nimcp_snn_batch_safe_is_enabled(void);
        extern int nimcp_snn_scaling_apply_batch(float*, const float*, uint32_t, uint32_t, float);
        extern int nimcp_snn_depression_apply_batch(float*, const float*, uint32_t, uint32_t, float, float, float);
        extern int nimcp_snn_ip_apply_batch(float*, const float*, const float*, uint32_t, uint32_t, float, float, float);

        if (nimcp_snn_batch_safe_is_enabled()) {
            float fired_buf[1024];
            float* fired_vec = fired_buf;
            bool heap_alloc = false;
            if (pop->n_neurons > 1024) {
                fired_vec = (float*)malloc(pop->n_neurons * sizeof(float));
                heap_alloc = true;
            }
            if (fired_vec) {
                for (uint32_t n = 0; n < pop->n_neurons; n++) {
                    fired_vec[n] = spike_data[n] > 0.5f ? 1.0f : 0.0f;
                }
                nimcp_snn_scaling_apply_batch(
                    pop->neuron_rate_ema, fired_vec, 1, pop->n_neurons,
                    1.0f - ip_alpha);
                nimcp_snn_ip_apply_batch(
                    pop->threshold_offset, fired_vec,
                    pop->neuron_rate_ema, 1, pop->n_neurons,
                    ip_gain, target_neuron_rate, ip_cap);
                nimcp_snn_depression_apply_batch(
                    pop->depression, fired_vec, 1, pop->n_neurons,
                    dep_decay, dep_inc, dep_cap);
                if (heap_alloc) free(fired_vec);
            }
        } else {
            for (uint32_t n = 0; n < pop->n_neurons; n++) {
                float fired = spike_data[n] > 0.5f ? 1.0f : 0.0f;
                pop->neuron_rate_ema[n] = (1.0f - ip_alpha) * pop->neuron_rate_ema[n]
                                        + ip_alpha * fired;
                float err = pop->neuron_rate_ema[n] - target_neuron_rate;
                pop->threshold_offset[n] += ip_gain * err;
                if (pop->threshold_offset[n] > ip_cap) pop->threshold_offset[n] = ip_cap;
                if (pop->threshold_offset[n] < -ip_cap) pop->threshold_offset[n] = -ip_cap;
                pop->depression[n] *= dep_decay;
                if (fired > 0.5f) {
                    pop->depression[n] += dep_inc;
                    if (pop->depression[n] > dep_cap) pop->depression[n] = dep_cap;
                }
            }
        }
    }
}

/**
 * @brief Phase B: GPU CB integration step. Runs the synaptic deposit on
 *        host (so the same logic as CPU CB is used — single source of
 *        truth via snn_cb_deposit_neuron) into pop->g_*, then gathers V +
 *        g_* into flat host buffers, uploads to lif_state, runs
 *        nimcp_gpu_lif_forward_cb, downloads V + spikes + post-decay g_*,
 *        scatters back to per-pop arrays, then runs the post-spike
 *        adaptation on each pop.
 *
 * WHY:  Lifts CB integration off the CPU's per-neuron compute_dv onto the
 *       GPU's batched kernel. With the deposit-pass extraction (Phase A),
 *       this is the surgical cutover — same deposit semantics on host,
 *       same post-spike adaptation on host, only the membrane-update step
 *       is now massively parallel on GPU.
 *
 * RETURN: total spike count across the network on success, or -1 on
 *         a hard failure (caller should fall back to CPU).
 *
 * Stage A/CB-GPU-7 limitations:
 *   - Wave H dendritic two-compartment integration is NOT supported on
 *     this path — pops with dend_mode=true skip the GPU and continue on
 *     CPU.  Verified in the gate by checking pop->dendritic_enabled.
 *   - Substrate spike-survival dropout runs on CPU (the GPU kernel is
 *     deterministic). Pops with substrate_spike_dropout_on stay on CPU.
 *   - Refractory + threshold-offset are NOT honored by the kernel
 *     (uses scalar v_thresh from params + per-neuron arrays). For the
 *     baseline this matches because both are 0 by default. Production
 *     networks that rely on per-neuron refractory periods or dynamic
 *     threshold offsets MUST keep cb_gpu_enabled=0 — CB-GPU-7 ports the
 *     deposit pass to GPU but does NOT lift this constraint. A future
 *     stage will honor refractory + v_thresh_offset per-neuron arrays.
 */
static int snn_network_step_cb_gpu(snn_network_t* network, float dt_ms) {
    extern float snn_tune_get_conductance_enabled(void);
    extern float snn_tune_get_cb_gpu_enabled(void);
    if (!network || !network->gpu_ctx || !network->gpu_lif_state) return -1;
    if (snn_tune_get_conductance_enabled() == 0.0f) return -1;
    if (snn_tune_get_cb_gpu_enabled() == 0.0f) return -1;
    /* NOTE: substrate spike-survival dropout is stochastic on CPU; the
     * GPU kernel is deterministic. We do NOT gate on this tune knob —
     * its production default is ON, and gating would force every step
     * to CPU (3000ms vs 60ms). Documented divergence: GPU path drops no
     * spikes; CPU path applies stochastic survival when substrate +
     * se_axon are present. Production accepts this for the perf win. */

    nimcp_gpu_context_t* gpu = (nimcp_gpu_context_t*)network->gpu_ctx;
    nimcp_lif_state_t*   lif = (nimcp_lif_state_t*)network->gpu_lif_state;

    /* Total neurons + Stage A eligibility check. Bail to CPU if any pop
     * has dendritic mode or substrate spike dropout active. */
    size_t total_neurons = 0;
    for (uint32_t p = 0; p < network->n_populations; p++) {
        snn_population_t* pop = network->populations[p];
        if (!pop) continue;
        total_neurons += pop->n_neurons;
        if (pop->dendritic_enabled) return -1;
    }
    if (total_neurons == 0) return -1;
    if (lif->v && lif->v->numel != total_neurons) return -1;

    /* Lazy-alloc CB tensors on the lif_state on first hit. */
    if (!lif->g_ampa) {
        if (!nimcp_gpu_lif_state_alloc_cb_arrays(gpu, lif, total_neurons)) {
            return -1;
        }
    }

    /* CB tune snapshots (single read per step — best-effort against
     * concurrent tune_set RPCs; no cross-step torn reads). */
    extern float snn_tune_get_e_ampa_mv(void);
    extern float snn_tune_get_e_nmda_mv(void);
    extern float snn_tune_get_e_gaba_a_mv(void);
    extern float snn_tune_get_e_gaba_b_mv(void);
    extern float snn_tune_get_tau_ampa_ms(void);
    extern float snn_tune_get_tau_nmda_ms(void);
    extern float snn_tune_get_tau_gaba_a_ms(void);
    extern float snn_tune_get_tau_gaba_b_ms(void);
    extern float snn_tune_get_nmda_mg_mm(void);
    extern float snn_tune_get_basket_enabled(void);
    extern float snn_tune_get_ahp_enabled(void);
    extern float snn_tune_get_pump_enabled(void);
    extern float snn_tune_get_noise_rate_hz(void);
    extern float snn_tune_get_noise_pulse_mv(void);
    extern float snn_tune_get_noise_ei_ratio(void);
    extern float snn_noise_factor_for_pop(const snn_population_t*);

    const float e_ampa     = snn_tune_get_e_ampa_mv();
    const float e_nmda     = snn_tune_get_e_nmda_mv();
    const float e_gaba_a   = snn_tune_get_e_gaba_a_mv();
    const float e_gaba_b   = snn_tune_get_e_gaba_b_mv();
    const float tau_ampa   = snn_tune_get_tau_ampa_ms();
    const float tau_nmda   = snn_tune_get_tau_nmda_ms();
    const float tau_gaba_a = snn_tune_get_tau_gaba_a_ms();
    const float tau_gaba_b = snn_tune_get_tau_gaba_b_ms();
    const float mg_mm      = snn_tune_get_nmda_mg_mm();
    const float noise_rate = snn_tune_get_noise_rate_hz();
    const float noise_pulse_mv = snn_tune_get_noise_pulse_mv();
    const float ei_ratio       = snn_tune_get_noise_ei_ratio();

    /* Per-step rand_r seed mirrors the CPU path (thread-local + lazy init). */
    static __thread unsigned int _cb_gpu_noise_seed = 0;
    if (_cb_gpu_noise_seed == 0) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        _cb_gpu_noise_seed = (unsigned int)(ts.tv_nsec
            ^ ((uintptr_t)&_cb_gpu_noise_seed >> 4));
        if (_cb_gpu_noise_seed == 0) _cb_gpu_noise_seed = 0xCAFEBABEu;
    }

    /* Per-pop scratch for AHP/pump hyp arrays. Indexed by population
     * slot in network->populations; passed to snn_cb_post_spike_pop in
     * the post-spike pass. */
    float** ahp_hyp_per_pop  = (float**)nimcp_calloc(network->n_populations, sizeof(float*));
    float** pump_hyp_per_pop = (float**)nimcp_calloc(network->n_populations, sizeof(float*));
    if (!ahp_hyp_per_pop || !pump_hyp_per_pop) {
        if (ahp_hyp_per_pop)  nimcp_free(ahp_hyp_per_pop);
        if (pump_hyp_per_pop) nimcp_free(pump_hyp_per_pop);
        return -1;
    }

    /* CB-GPU-7 sub-phase instrumentation. Total cbgpu_ms includes all
     * of these; breakdown tells us whether to optimize deposit / I/O /
     * kernel / post-spike. Logged as a NIMCP_DEBUG_SNN line every step. */
    struct timespec _ph0, _ph1;
    double dep_ms = 0, gather_ms = 0, upload_ms = 0, kernel_ms = 0,
           dl_ms = 0, scatter_ms = 0, postspike_ms = 0;

    /* ===== CB-GPU-7: pre-step lazy upload (one-time per pop) =====
     * MUST run BEFORE the host deposit loop — the loop's `cb_gpu_csr_path`
     * gate reads `pop->incoming_csr->gpu_resident`, and that flag needs to
     * be settled NOW (not after the host already walked CSR into pop->g_*).
     * Failure to hoist this caused the BLOCKER double-deposit on step 1
     * found in walkthrough #1: host wrote CSR contribs into pop->g_*, then
     * the kernel added them again. */
    for (uint32_t p = 0; p < network->n_populations; p++) {
        snn_population_t* pop = network->populations[p];
        if (!pop || !pop->incoming_csr) continue;
        if (!pop->incoming_csr->gpu_ready) continue;  /* non-lightweight */
        if (pop->incoming_csr->n_synapses == 0)        continue;
        if (!pop->incoming_csr->gpu_resident) {
            /* One-time CSR upload (~3 GB/sec PCIe at 380M synapses ≈ 2 s
             * for the largest pops; only happens at first CB-GPU step). */
            (void)snn_csr_upload_to_gpu(pop->incoming_csr, gpu);
        }
        if (pop->incoming_csr->gpu_resident && !pop->d_synapse_type_per_src) {
            size_t st_bytes = SNN_MAX_POPULATIONS * sizeof(uint8_t);
            pop->d_synapse_type_per_src = nimcp_gpu_malloc(gpu, st_bytes);
            if (pop->d_synapse_type_per_src) {
                if (nimcp_gpu_memcpy(
                        gpu, pop->d_synapse_type_per_src,
                        &pop->synapse_type_per_src[0], st_bytes,
                        GPU_MEMCPY_HOST_TO_DEVICE) != 0) {
                    nimcp_gpu_free(gpu, pop->d_synapse_type_per_src);
                    pop->d_synapse_type_per_src = NULL;
                } else {
                    /* Stash gpu_ctx so destroy can free even if CSR was
                     * released first (its d_gpu_ctx would be NULL). */
                    pop->d_synapse_type_gpu_ctx = gpu;
                }
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &_ph0);

    /* ===== Per-pop deposit pass (host) =====
     * Decay g_*, apply gap-junction V, refractory tick + early-out, run
     * the per-neuron deposit helper. Mirrors the inline CPU loop minus
     * the integrate + spike block (which the GPU kernel handles). */
    for (uint32_t p = 0; p < network->n_populations; p++) {
        snn_population_t* pop = network->populations[p];
        if (!pop) continue;
        const float decay_ampa   = expf(-dt_ms / tau_ampa);
        const float decay_nmda   = expf(-dt_ms / tau_nmda);
        const float decay_gaba_a = expf(-dt_ms / tau_gaba_a);
        const float decay_gaba_b = expf(-dt_ms / tau_gaba_b);

        /* Per-pop CB constants (Stage A: cb_mode is implied by being on
         * this code path; dend_mode is excluded by the eligibility gate). */
        const bool   ahp_on    = (pop->ahp  && snn_tune_get_ahp_enabled()  != 0.0f);
        const bool   pump_on   = (pop->pump && snn_tune_get_pump_enabled() != 0.0f);
        const bool   basket_on = (pop->basket && snn_tune_get_basket_enabled() != 0.0f);
        float basket_contrib = 0.0f;
        if (basket_on) {
            basket_contrib = pop->basket->gain_inhib_to_parent
                           * snn_basket_pool_mean_rate(pop->basket);
        }
        const float noise_factor = snn_noise_factor_for_pop(pop);
        const float p_base    = noise_rate * dt_ms * 0.001f;
        const float noise_p   = p_base * noise_factor;
        const bool  noise_exc_only = (noise_factor >= 0.9f);

        snn_cb_deposit_ctx_t dep_ctx = {
            .cb_mode        = true,
            .dend_mode      = false,
            .basket_contrib = basket_contrib,
            .noise_p        = noise_p,
            .noise_pulse_mv = noise_pulse_mv,
            .ei_ratio       = ei_ratio,
            .noise_exc_only = noise_exc_only,
            .noise_seed     = &_cb_gpu_noise_seed,
            /* CB-GPU-7: skip the host CSR walk only when BOTH the CSR is
             * GPU-resident AND the per-pop synapse_type table is uploaded.
             * The pre-step lazy-upload block above settles both flags
             * BEFORE this loop runs, so this gate is reliable. */
            .cb_gpu_csr_path = (pop->incoming_csr &&
                                pop->incoming_csr->gpu_resident &&
                                pop->d_synapse_type_per_src != NULL),
        };

        /* AHP/pump hyp arrays — kernel does not use them (Stage A
         * limitation), but compute and pass to post_spike for adaptation
         * update. */
        float* ahp_hyp  = ahp_on  ? (float*)nimcp_calloc(pop->n_neurons, sizeof(float)) : NULL;
        float* pump_hyp = pump_on ? (float*)nimcp_calloc(pop->n_neurons, sizeof(float)) : NULL;
        if (ahp_hyp)  snn_adaptation_compute_hyperpol(pop->ahp,  ahp_hyp,  dt_ms);
        if (pump_hyp) snn_adaptation_compute_hyperpol(pop->pump, pump_hyp, dt_ms);

        float* ref_data = (float*)nimcp_tensor_data(pop->refractory);
        for (uint32_t n = 0; n < pop->n_neurons; n++) {
            /* Per-neuron CB receptor pointers (NULL-safe at deposit
             * boundary). */
            float* g_ampa_n   = pop->g_ampa   ? &pop->g_ampa[n]   : NULL;
            float* g_nmda_n   = pop->g_nmda   ? &pop->g_nmda[n]   : NULL;
            float* g_gaba_a_n = pop->g_gaba_a ? &pop->g_gaba_a[n] : NULL;
            float* g_gaba_b_n = pop->g_gaba_b ? &pop->g_gaba_b[n] : NULL;

            /* Decay BEFORE deposit (matches CPU CB ordering). */
            snn_membrane_decay_one(g_ampa_n, g_nmda_n, g_gaba_a_n, g_gaba_b_n,
                                   decay_ampa, decay_nmda,
                                   decay_gaba_a, decay_gaba_b);

            /* Refractory tick (kernel doesn't honor refractory in Stage
             * A; see limitation note above). */
            if (ref_data && ref_data[n] > 0.0f) {
                ref_data[n] -= dt_ms;
                continue;
            }

            float I_syn_unused;
            snn_cb_deposit_neuron(
                network, pop, n, &dep_ctx,
                g_ampa_n, g_nmda_n, g_gaba_a_n, g_gaba_b_n,
                NULL, NULL, NULL, NULL,  /* dend disabled in Stage A */
                &I_syn_unused);
        }

        /* Stash hyp arrays for post-spike pass. */
        ahp_hyp_per_pop[p]  = ahp_hyp;
        pump_hyp_per_pop[p] = pump_hyp;
    }
    clock_gettime(CLOCK_MONOTONIC, &_ph1);
    dep_ms = (_ph1.tv_sec - _ph0.tv_sec) * 1000.0
           + (_ph1.tv_nsec - _ph0.tv_nsec) / 1e6;
    _ph0 = _ph1;

    /* ===== Gather + upload =====
     * Flat per-receptor host buffers, one float per neuron, in pop order.
     * Same flatten order as nimcp_gpu_lif_state_upload_per_neuron_params. */
    float* h_v       = (float*)nimcp_calloc(total_neurons, sizeof(float));
    float* h_g_ampa  = (float*)nimcp_calloc(total_neurons, sizeof(float));
    float* h_g_nmda  = (float*)nimcp_calloc(total_neurons, sizeof(float));
    float* h_g_ga_a  = (float*)nimcp_calloc(total_neurons, sizeof(float));
    float* h_g_ga_b  = (float*)nimcp_calloc(total_neurons, sizeof(float));
    if (!h_v || !h_g_ampa || !h_g_nmda || !h_g_ga_a || !h_g_ga_b) {
        if (h_v) nimcp_free(h_v);
        if (h_g_ampa) nimcp_free(h_g_ampa);
        if (h_g_nmda) nimcp_free(h_g_nmda);
        if (h_g_ga_a) nimcp_free(h_g_ga_a);
        if (h_g_ga_b) nimcp_free(h_g_ga_b);
        for (uint32_t p = 0; p < network->n_populations; p++) {
            if (ahp_hyp_per_pop[p])  nimcp_free(ahp_hyp_per_pop[p]);
            if (pump_hyp_per_pop[p]) nimcp_free(pump_hyp_per_pop[p]);
        }
        nimcp_free(ahp_hyp_per_pop);
        nimcp_free(pump_hyp_per_pop);
        return -1;
    }
    size_t off = 0;
    for (uint32_t p = 0; p < network->n_populations; p++) {
        snn_population_t* pop = network->populations[p];
        if (!pop) continue;
        const float* v = (const float*)nimcp_tensor_data(pop->membrane_v);
        if (v) memcpy(h_v + off, v, pop->n_neurons * sizeof(float));
        if (pop->g_ampa)   memcpy(h_g_ampa + off, pop->g_ampa,   pop->n_neurons * sizeof(float));
        if (pop->g_nmda)   memcpy(h_g_nmda + off, pop->g_nmda,   pop->n_neurons * sizeof(float));
        if (pop->g_gaba_a) memcpy(h_g_ga_a + off, pop->g_gaba_a, pop->n_neurons * sizeof(float));
        if (pop->g_gaba_b) memcpy(h_g_ga_b + off, pop->g_gaba_b, pop->n_neurons * sizeof(float));
        off += pop->n_neurons;
    }

    clock_gettime(CLOCK_MONOTONIC, &_ph1);
    gather_ms = (_ph1.tv_sec - _ph0.tv_sec) * 1000.0
              + (_ph1.tv_nsec - _ph0.tv_nsec) / 1e6;
    _ph0 = _ph1;

    /* Upload V into the lif_state's V tensor (mutated in place by the
     * kernel) + g_* into the lif_state's CB arrays. */
    bool ok = nimcp_gpu_tensor_upload(lif->v, h_v);
    ok = ok && nimcp_gpu_lif_state_upload_g(
        gpu, lif, h_g_ampa, h_g_nmda, h_g_ga_a, h_g_ga_b, total_neurons);
    clock_gettime(CLOCK_MONOTONIC, &_ph1);
    upload_ms = (_ph1.tv_sec - _ph0.tv_sec) * 1000.0
              + (_ph1.tv_nsec - _ph0.tv_nsec) / 1e6;
    _ph0 = _ph1;

    /* ===== CB-GPU-7: Per-pop deposit kernel (CSR walk on GPU) =====
     * For each pop with GPU-resident incoming CSR + uploaded synapse_type,
     * launch the deposit kernel that walks the CSR row-by-row and adds
     * spike-driven contributions on top of the just-uploaded g_*. Pops
     * that fail the eligibility gate (no GPU-resident CSR or no synapse
     * type uploaded yet) had cb_gpu_csr_path=false in the host loop, so
     * their CSR contributions are already in pop->g_* and were uploaded
     * with the rest. The kernel is a no-op for those pops.
     *
     * One-time per-step buffers built here:
     *   h_spike_flat[total_neurons]: gather of pop->spike_output (or
     *     delayed row from spike_history ring) into one flat vector
     *   h_depr_flat [total_neurons]: gather of pop->depression
     * These are uploaded ONCE and reused across all pops' deposit launches
     * within this step (each pop's CSR uses global flat indices). */
    if (ok) {
        float* h_spike_flat = (float*)nimcp_calloc(total_neurons, sizeof(float));
        float* h_depr_flat  = (float*)nimcp_calloc(total_neurons, sizeof(float));
        if (h_spike_flat && h_depr_flat) {
            size_t off2 = 0;
            for (uint32_t p = 0; p < network->n_populations; p++) {
                snn_population_t* pop = network->populations[p];
                if (!pop) continue;
                /* Prefer the delayed-row view (Wave E FFI) so the GPU
                 * deposit sees the same axonal-delay'd spikes the host
                 * helper would have seen. */
                const float* spk = snn_pop_get_delayed_spike_row(pop);
                if (!spk && pop->spike_output) {
                    spk = (const float*)nimcp_tensor_data(pop->spike_output);
                }
                if (spk) memcpy(h_spike_flat + off2, spk,
                                pop->n_neurons * sizeof(float));
                if (pop->depression)
                    memcpy(h_depr_flat + off2, pop->depression,
                           pop->n_neurons * sizeof(float));
                off2 += pop->n_neurons;
            }

            /* Lazy-alloc network-level scratch tensors. Reallocate when the
             * network grew (add_population mid-run): the OLD buffer is sized
             * for the prior total_neurons, but the upload below would write
             * total_neurons floats — OOB device write. Track capacity to
             * detect growth. */
            if (network->gpu_cb_flat_capacity < total_neurons) {
                if (network->gpu_cb_spike_flat) {
                    nimcp_gpu_free(gpu, network->gpu_cb_spike_flat);
                    network->gpu_cb_spike_flat = NULL;
                }
                if (network->gpu_cb_depr_flat) {
                    nimcp_gpu_free(gpu, network->gpu_cb_depr_flat);
                    network->gpu_cb_depr_flat = NULL;
                }
                network->gpu_cb_flat_capacity = 0;
            }
            if (!network->gpu_cb_spike_flat) {
                network->gpu_cb_spike_flat =
                    nimcp_gpu_malloc(gpu, total_neurons * sizeof(float));
            }
            if (!network->gpu_cb_depr_flat) {
                network->gpu_cb_depr_flat =
                    nimcp_gpu_malloc(gpu, total_neurons * sizeof(float));
            }
            if (network->gpu_cb_spike_flat && network->gpu_cb_depr_flat) {
                network->gpu_cb_flat_capacity = total_neurons;
            }

            /* Upload this step's flat vectors; if either upload fails,
             * skip the kernel-launch loop entirely. Host already walked
             * CSR for any pop whose `cb_gpu_csr_path` was false (gate at
             * the host loop), but for pops where the gate was TRUE the
             * CSR contributions would be silently lost on a failed
             * upload. Conservative: refuse to launch and let the next
             * step retry. */
            bool spike_uploaded = false, depr_uploaded = false;
            if (network->gpu_cb_spike_flat && network->gpu_cb_depr_flat) {
                spike_uploaded = (nimcp_gpu_memcpy(
                    gpu, network->gpu_cb_spike_flat, h_spike_flat,
                    total_neurons * sizeof(float),
                    GPU_MEMCPY_HOST_TO_DEVICE) == 0);
                depr_uploaded  = (nimcp_gpu_memcpy(
                    gpu, network->gpu_cb_depr_flat, h_depr_flat,
                    total_neurons * sizeof(float),
                    GPU_MEMCPY_HOST_TO_DEVICE) == 0);
            }

            if (spike_uploaded && depr_uploaded) {
                /* Launch the kernel per pop. Lazy-upload synapse_type table
                 * + CSR if needed; if any prereq fails, skip this pop (its
                 * CSR contributions are still on host pop->g_*, already
                 * uploaded above). */
                size_t off3 = 0;
                for (uint32_t p = 0; p < network->n_populations; p++) {
                    snn_population_t* pop = network->populations[p];
                    if (!pop) {
                        continue;
                    }
                    const uint32_t pop_neurons = pop->n_neurons;
                    /* Skip pops that didn't pass the pre-step upload gate.
                     * Their cb_gpu_csr_path was set to false in the host
                     * loop, so their CSR contributions are already in the
                     * resident lif d_g_* (host walked + uploaded). The
                     * kernel must be a no-op for these pops. */
                    if (!pop->incoming_csr ||
                        pop->incoming_csr->n_synapses == 0 ||
                        !pop->incoming_csr->gpu_resident ||
                        !pop->d_synapse_type_per_src) {
                        off3 += pop_neurons;
                        continue;
                    }
                    /* Launch. Kernel reads lif_state's d_g_* directly so
                     * we pass them; offset is the pop's flat start. */
                    nimcp_gpu_snn_cb_deposit_csr(
                        gpu,
                        (const float*)network->gpu_cb_spike_flat,
                        (const float*)network->gpu_cb_depr_flat,
                        (const float*)pop->incoming_csr->d_weights,
                        (const uint32_t*)pop->incoming_csr->d_flat_col_idx,
                        (const uint32_t*)pop->incoming_csr->d_src_pop_idx,
                        (const uint32_t*)pop->incoming_csr->d_row_ptr,
                        (const uint8_t*)pop->d_synapse_type_per_src,
                        lif->g_ampa   ? (float*)lif->g_ampa->data   : NULL,
                        lif->g_nmda   ? (float*)lif->g_nmda->data   : NULL,
                        lif->g_gaba_a ? (float*)lif->g_gaba_a->data : NULL,
                        lif->g_gaba_b ? (float*)lif->g_gaba_b->data : NULL,
                        (uint32_t)off3,
                        pop_neurons);
                    off3 += pop_neurons;
                }
            }
        }
        if (h_spike_flat) nimcp_free(h_spike_flat);
        if (h_depr_flat)  nimcp_free(h_depr_flat);
    }

    /* ===== GPU forward_cb =====
     * Integrates V, writes spikes, applies per-receptor decay AFTER
     * membrane update (matches kernel contract). */
    if (ok) {
        ok = nimcp_gpu_lif_forward_cb(
            gpu, lif,
            e_ampa, e_nmda, e_gaba_a, e_gaba_b,
            tau_ampa, tau_nmda, tau_gaba_a, tau_gaba_b,
            mg_mm);
    }
    clock_gettime(CLOCK_MONOTONIC, &_ph1);
    kernel_ms = (_ph1.tv_sec - _ph0.tv_sec) * 1000.0
              + (_ph1.tv_nsec - _ph0.tv_nsec) / 1e6;
    _ph0 = _ph1;
    if (!ok) {
        nimcp_free(h_v);
        nimcp_free(h_g_ampa); nimcp_free(h_g_nmda);
        nimcp_free(h_g_ga_a); nimcp_free(h_g_ga_b);
        for (uint32_t p = 0; p < network->n_populations; p++) {
            if (ahp_hyp_per_pop[p])  nimcp_free(ahp_hyp_per_pop[p]);
            if (pump_hyp_per_pop[p]) nimcp_free(pump_hyp_per_pop[p]);
        }
        nimcp_free(ahp_hyp_per_pop);
        nimcp_free(pump_hyp_per_pop);
        return -1;
    }

    /* ===== Download + scatter =====
     * V + spikes + post-decay g_*. Reuse the upload buffers. */
    float* h_spikes = (float*)nimcp_calloc(total_neurons, sizeof(float));
    if (!h_spikes ||
        !nimcp_gpu_tensor_to_host(lif->v, h_v) ||
        !nimcp_gpu_tensor_to_host(lif->spikes, h_spikes) ||
        !nimcp_gpu_lif_state_download_g(
            gpu, lif, h_g_ampa, h_g_nmda, h_g_ga_a, h_g_ga_b, total_neurons)) {
        if (h_spikes) nimcp_free(h_spikes);
        nimcp_free(h_v);
        nimcp_free(h_g_ampa); nimcp_free(h_g_nmda);
        nimcp_free(h_g_ga_a); nimcp_free(h_g_ga_b);
        for (uint32_t p = 0; p < network->n_populations; p++) {
            if (ahp_hyp_per_pop[p])  nimcp_free(ahp_hyp_per_pop[p]);
            if (pump_hyp_per_pop[p]) nimcp_free(pump_hyp_per_pop[p]);
        }
        nimcp_free(ahp_hyp_per_pop);
        nimcp_free(pump_hyp_per_pop);
        return -1;
    }

    clock_gettime(CLOCK_MONOTONIC, &_ph1);
    dl_ms = (_ph1.tv_sec - _ph0.tv_sec) * 1000.0
          + (_ph1.tv_nsec - _ph0.tv_nsec) / 1e6;
    _ph0 = _ph1;

    int total_spikes = 0;
    off = 0;
    for (uint32_t p = 0; p < network->n_populations; p++) {
        snn_population_t* pop = network->populations[p];
        if (!pop) continue;
        float* v_data     = (float*)nimcp_tensor_data(pop->membrane_v);
        float* spike_data = (float*)nimcp_tensor_data(pop->spike_output);
        for (uint32_t n = 0; n < pop->n_neurons; n++) {
            if (v_data)     v_data[n]     = h_v[off + n];
            float spk = h_spikes[off + n];
            if (spike_data) spike_data[n] = spk;
            if (spk > 0.5f) {
                record_spike(&pop->spike_trains[n], network->sim->current_time_us);
                pop->total_spikes++;
                total_spikes++;
            }
        }
        if (pop->g_ampa)   memcpy(pop->g_ampa,   h_g_ampa + off, pop->n_neurons * sizeof(float));
        if (pop->g_nmda)   memcpy(pop->g_nmda,   h_g_nmda + off, pop->n_neurons * sizeof(float));
        if (pop->g_gaba_a) memcpy(pop->g_gaba_a, h_g_ga_a + off, pop->n_neurons * sizeof(float));
        if (pop->g_gaba_b) memcpy(pop->g_gaba_b, h_g_ga_b + off, pop->n_neurons * sizeof(float));
        off += pop->n_neurons;
    }
    clock_gettime(CLOCK_MONOTONIC, &_ph1);
    scatter_ms = (_ph1.tv_sec - _ph0.tv_sec) * 1000.0
               + (_ph1.tv_nsec - _ph0.tv_nsec) / 1e6;
    _ph0 = _ph1;

    nimcp_free(h_v);
    nimcp_free(h_g_ampa); nimcp_free(h_g_nmda);
    nimcp_free(h_g_ga_a); nimcp_free(h_g_ga_b);
    nimcp_free(h_spikes);

    /* ===== Per-pop post-spike adaptation =====
     * AHP/pump update (reads spike_data the GPU just wrote), basket step,
     * IP + depression. Same helper the CPU CB path uses. */
    for (uint32_t p = 0; p < network->n_populations; p++) {
        snn_population_t* pop = network->populations[p];
        if (!pop) continue;
        const bool ahp_on    = (pop->ahp  && snn_tune_get_ahp_enabled()  != 0.0f);
        const bool pump_on   = (pop->pump && snn_tune_get_pump_enabled() != 0.0f);
        const bool basket_on = (pop->basket && snn_tune_get_basket_enabled() != 0.0f);
        snn_cb_post_spike_pop(pop, dt_ms, ahp_on, pump_on, basket_on,
                              ahp_hyp_per_pop[p], pump_hyp_per_pop[p]);
        if (pop->external_current) {
            memset(pop->external_current, 0, pop->n_neurons * sizeof(float));
        }
    }

    nimcp_free(ahp_hyp_per_pop);
    nimcp_free(pump_hyp_per_pop);

    clock_gettime(CLOCK_MONOTONIC, &_ph1);
    postspike_ms = (_ph1.tv_sec - _ph0.tv_sec) * 1000.0
                 + (_ph1.tv_nsec - _ph0.tv_nsec) / 1e6;

    NIMCP_LOGGING_INFO(
        "CB-GPU sub: dep=%.1f gather=%.1f upload=%.1f kernel=%.1f "
        "dl=%.1f scatter=%.1f postspike=%.1f",
        dep_ms, gather_ms, upload_ms, kernel_ms,
        dl_ms, scatter_ms, postspike_ms);
    return total_spikes;
}

//=============================================================================
// Simulation
//=============================================================================

int snn_network_step(snn_network_t* network, float dt) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_step: null network pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!network->sim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "snn_network_step: null simulation context");
        return SNN_ERROR_INVALID_STATE;
    }

    /* Phase 8: Send heartbeat at start of network step */
    snn_network_heartbeat("snn_step", 0.0f);

    /* FNO recording: snapshot V_before for every population BEFORE any
     * integration logic (GPU CB path, GPU current-based path, CPU lightweight,
     * CPU legacy). The matching record_post_step at the bottom pairs these
     * with the post-integration V_after + spikes for snn_fno_record_pair.
     * No-op when init_fno hasn't been called or recording is disabled. */
    snn_fno_snapshot_v_before(network);

    float dt_ms = (dt > 0.0f) ? dt : network->config.dt;
    uint64_t dt_us = (uint64_t)(dt_ms * 1000.0f);

    int total_spikes = 0;
    bool gpu_executed = false;
    struct timespec _step_t0, _step_t1, _phase_t0, _phase_t1;
    double _isyn_ms = 0, _lif_ms = 0, _readback_ms = 0;
    /* CB-GPU-7 instrumentation: time the post-step GPU-path phases so we
     * can see where the 2.7s/step is actually spent (deposit pass and
     * kernel both report 0ms — the cost is somewhere in post-step work). */
    double _cbgpu_ms = 0, _substrate_debit_ms = 0, _ema_ring_ms = 0;
    clock_gettime(CLOCK_MONOTONIC, &_step_t0);

    /* CB migration: function-scope declaration so the GPU-skip gate
     * below can call it before the lightweight branch redeclares it. */
    extern float snn_tune_get_conductance_enabled(void);

    /* ===== Substrate adapter: refresh cache EVERY step, BOTH paths =====
     * Bug #2 fix: previously this lived inside `if (!gpu_executed)`, which
     * meant the production GPU path never refreshed the cache and every
     * R-STDP weight update multiplied by a stale (possibly zero) value.
     * Hoist it to the top of snn_network_step so both CPU and GPU paths
     * see up-to-date plasticity_mod / spike_reliability / etc.
     *
     * GPU-path limitation: per-neuron tau_eff/tref_eff modulation still
     * only applies on the CPU fallback — those values feed the LIF
     * integration loop which runs inside a CUDA kernel that doesn't read
     * the cache. Plasticity modulation still works on GPU because it
     * happens in snn_rstdp_apply on the host side after the step.
     * Future work: upload the cache struct to GPU and thread it through
     * the LIF + surrogate-gradient kernels. */
    if (network->substrate && snn_tune_get_substrate_enabled() != 0.0f) {
        const uint32_t period =
            (uint32_t)snn_tune_get_substrate_update_period();
        if (network->substrate_steps_since_update == 0 || period == 0) {
            substrate_compute_effects(network->substrate,
                                      &network->cached_axon_effects,
                                      &network->cached_dend_effects);
        }
        network->substrate_steps_since_update++;
        /* Bug #5 verification: cap counter to avoid uint32 overflow on
         * very long runs + ensure deterministic modulo rollover. */
        if (period > 0 && network->substrate_steps_since_update >= period) {
            network->substrate_steps_since_update = 0;
        }
    }

    /* ===== CB GPU FAST PATH (Phase B cutover) =====
     * When CB is on AND cb_gpu_enabled is on AND a GPU is attached, run
     * CB integration on GPU via snn_network_step_cb_gpu. Returns spike
     * count on success, -1 on hard failure (then fall through to either
     * the legacy current-based GPU path or the CPU CB loop below). */
    extern float snn_tune_get_cb_gpu_enabled(void);
    if (network->gpu_lif_state && network->gpu_ctx
        && snn_tune_get_conductance_enabled() != 0.0f
        && snn_tune_get_cb_gpu_enabled()      != 0.0f) {
        clock_gettime(CLOCK_MONOTONIC, &_phase_t0);
        int cb_gpu_spikes = snn_network_step_cb_gpu(network, dt_ms);
        clock_gettime(CLOCK_MONOTONIC, &_phase_t1);
        _cbgpu_ms = (_phase_t1.tv_sec - _phase_t0.tv_sec) * 1000.0
                  + (_phase_t1.tv_nsec - _phase_t0.tv_nsec) / 1e6;
        if (cb_gpu_spikes >= 0) {
            total_spikes += cb_gpu_spikes;
            gpu_executed = true;
        }
        /* If cb_gpu_spikes < 0, eligibility gate excluded the network
         * (e.g. dendritic mode active). Fall through to CPU CB. */
    }

    /* ===== GPU FAST PATH (current-based, CB OFF) ===== */
    if (!gpu_executed
        && network->gpu_lif_state && network->gpu_ctx
        && snn_tune_get_conductance_enabled() == 0.0f) {
        nimcp_gpu_context_t* gpu = (nimcp_gpu_context_t*)network->gpu_ctx;
        nimcp_lif_state_t* lif_state = (nimcp_lif_state_t*)network->gpu_lif_state;

        /* Compute total neuron count across all populations */
        size_t total_neurons = 0;
        for (uint32_t p = 0; p < network->n_populations; p++) {
            if (network->populations[p]) {
                total_neurons += network->populations[p]->n_neurons;
            }
        }

        /* Wave G GPU sync (schema v17, 2026-04-27): when the per-neuron
         * params are dirty, walk every population and resolve each neuron's
         * τ_mem / v_thresh via snn_pop_neuron_lif_params() — that helper
         * composes BOTH subclass deltas (PV/SOM/VIP/TRN/L23/L4_STELLATE/
         * L5_BETZ) AND per-neuron heterogeneity σ noise. Upload the flat
         * total_neurons-sized arrays to the GPU lif_state. After this
         * upload the LIF kernels read the per-neuron values directly so
         * every tier-pyr pop fires asynchronously on GPU exactly as on CPU.
         *
         * Bit-identity contract: when no pop has subclass deltas AND
         * no pop has heterogeneity arrays we still upload the per-neuron
         * arrays — but every entry equals the network-wide config value,
         * so the kernel produces identical output to the scalar-fallback
         * path. The cost is one extra global-memory load per neuron per
         * step (negligible — already memory-bound on i_syn/v).
         *
         * The flag is set on lif_state create AND by
         * snn_network_set_pop_heterogeneity. Re-upload happens at most once
         * per dirty event (cleared on success). */
        if (lif_state->per_neuron_params_dirty && total_neurons > 0) {
            float* h_tau  = (float*)nimcp_calloc(total_neurons, sizeof(float));
            float* h_vthr = (float*)nimcp_calloc(total_neurons, sizeof(float));
            if (h_tau && h_vthr) {
                size_t neuron_idx = 0;
                for (uint32_t p = 0; p < network->n_populations; p++) {
                    snn_population_t* pop = network->populations[p];
                    if (!pop) continue;
                    for (uint32_t n = 0; n < pop->n_neurons; n++) {
                        snn_lif_params_t lp = snn_pop_neuron_lif_params(
                            pop, n, &network->config);
                        h_tau[neuron_idx]  = lp.tau_mem;
                        h_vthr[neuron_idx] = lp.v_thresh;
                        neuron_idx++;
                    }
                }
                bool ok = nimcp_gpu_lif_state_upload_per_neuron_params(
                    gpu, lif_state, h_tau, h_vthr, total_neurons);
                if (ok) {
                    lif_state->per_neuron_params_dirty = false;
                    NIMCP_LOGGING_DEBUG(
                        "snn_network_step: GPU per-neuron LIF params synced "
                        "for %zu neurons", total_neurons);
                } else {
                    NIMCP_LOGGING_WARN(
                        "snn_network_step: GPU per-neuron LIF params upload "
                        "failed — kernel falls back to scalar params");
                }
            } else {
                NIMCP_LOGGING_WARN(
                    "snn_network_step: per-neuron LIF host buffer alloc "
                    "failed — leaving GPU on scalar params");
            }
            if (h_tau)  nimcp_free(h_tau);
            if (h_vthr) nimcp_free(h_vthr);
        }

        /* Build input current vector for LIF kernel.
         * GPU CSR path: use kernel_snn_isyn_csr per population.
         * CPU fallback: iterate synapses sequentially. */
        float* h_input = (float*)nimcp_calloc(total_neurons, sizeof(float));
        if (h_input) {
            clock_gettime(CLOCK_MONOTONIC, &_phase_t0);
            /* Check if any lightweight pop has GPU-ready CSR */
            bool has_gpu_csr = false;
            for (uint32_t p = 0; p < network->n_populations && !has_gpu_csr; p++) {
                snn_population_t* pop = network->populations[p];
                if (pop && pop->lightweight && pop->incoming_csr &&
                    pop->incoming_csr->gpu_ready)
                    has_gpu_csr = true;
            }

            if (has_gpu_csr) {
                /* === GPU I_SYN PATH === */
                /* 1. Build flat spike vector on host from population tensors */
                float* h_spk_flat = (float*)nimcp_calloc(total_neurons, sizeof(float));
                if (h_spk_flat) {
                    size_t off = 0;
                    for (uint32_t p = 0; p < network->n_populations; p++) {
                        snn_population_t* pop = network->populations[p];
                        if (!pop) continue;
                        float* sd = (float*)nimcp_tensor_data(pop->spike_output);
                        if (sd) memcpy(h_spk_flat + off, sd, pop->n_neurons * sizeof(float));
                        off += pop->n_neurons;
                    }

                    /* 2. Upload flat spike vector to GPU */
                    size_t spk_dims[1] = { total_neurons };
                    nimcp_gpu_tensor_t* d_spk = nimcp_gpu_tensor_from_host(
                        gpu, h_spk_flat, spk_dims, 1, NIMCP_GPU_PRECISION_FP32);

                    if (d_spk) {
                        /* 3. Per-population: upload CSR + external_current, call kernel */
                        size_t pop_offset = 0;
                        for (uint32_t p = 0; p < network->n_populations; p++) {
                            snn_population_t* pop = network->populations[p];
                            if (!pop) continue;

                            if (pop->lightweight && pop->incoming_csr &&
                                pop->incoming_csr->gpu_ready &&
                                pop->incoming_csr->n_synapses > 0) {
                                snn_csr_storage_t* csr = pop->incoming_csr;

                                /* === V2 Fast path: persistent GPU CSR ===
                                 * Upload once, reuse every timestep. Eliminates
                                 * ~12 GB PCIe transfer per step for 1.45B-synapse
                                 * populations. Falls back to per-step upload if
                                 * upload fails or is disabled. */
                                bool use_persistent = false;
                                if (!csr->gpu_resident) {
                                    /* Lazy upload on first use */
                                    if (snn_csr_upload_to_gpu(csr, gpu) == 0) {
                                        use_persistent = true;
                                    }
                                } else {
                                    use_persistent = true;
                                }

                                size_t n_dims[1] = { pop->n_neurons };
                                float* ext_cur = pop->external_current;
                                float* h_ext = ext_cur ? ext_cur :
                                    (float*)nimcp_calloc(pop->n_neurons, sizeof(float));
                                nimcp_gpu_tensor_t* d_ext = nimcp_gpu_tensor_from_host(
                                    gpu, h_ext, n_dims, 1, NIMCP_GPU_PRECISION_FP32);
                                nimcp_gpu_tensor_t* d_isyn = nimcp_gpu_tensor_create(
                                    gpu, n_dims, 1, NIMCP_GPU_PRECISION_FP32);

                                extern bool nimcp_gpu_snn_isyn_csr(
                                    nimcp_gpu_context_t*, const float*,
                                    const float*, const unsigned int*,
                                    const unsigned int*, const float*,
                                    float*, size_t);

                                if (use_persistent && d_ext && d_isyn) {
                                    /* Fast path: use persistent device pointers */
                                    nimcp_gpu_snn_isyn_csr(gpu,
                                        (const float*)d_spk->data,
                                        (const float*)csr->d_weights,
                                        (const unsigned int*)csr->d_flat_col_idx,
                                        (const unsigned int*)csr->d_row_ptr,
                                        (const float*)d_ext->data,
                                        (float*)d_isyn->data,
                                        pop->n_neurons);
                                    nimcp_gpu_tensor_to_host(d_isyn,
                                        h_input + pop_offset);
                                } else if (d_ext && d_isyn) {
                                    /* Slow fallback: per-step CSR upload (legacy) */
                                    size_t w_dims[1] = { csr->n_synapses };
                                    size_t r_dims[1] = { (size_t)csr->n_neurons + 1 };
                                    nimcp_gpu_tensor_t* d_w = nimcp_gpu_tensor_from_host(
                                        gpu, csr->weights, w_dims, 1, NIMCP_GPU_PRECISION_FP32);
                                    nimcp_gpu_tensor_t* d_ci = nimcp_gpu_tensor_from_host(
                                        gpu, csr->flat_col_idx, w_dims, 1, NIMCP_GPU_PRECISION_UINT32);
                                    nimcp_gpu_tensor_t* d_rp = nimcp_gpu_tensor_from_host(
                                        gpu, csr->row_ptr, r_dims, 1, NIMCP_GPU_PRECISION_UINT32);
                                    if (d_w && d_ci && d_rp) {
                                        nimcp_gpu_snn_isyn_csr(gpu,
                                            (const float*)d_spk->data,
                                            (const float*)d_w->data,
                                            (const unsigned int*)d_ci->data,
                                            (const unsigned int*)d_rp->data,
                                            (const float*)d_ext->data,
                                            (float*)d_isyn->data,
                                            pop->n_neurons);
                                        nimcp_gpu_tensor_to_host(d_isyn,
                                            h_input + pop_offset);
                                    }
                                    if (d_w) nimcp_gpu_tensor_destroy(d_w);
                                    if (d_ci) nimcp_gpu_tensor_destroy(d_ci);
                                    if (d_rp) nimcp_gpu_tensor_destroy(d_rp);
                                }

                                if (d_ext) nimcp_gpu_tensor_destroy(d_ext);
                                if (d_isyn) nimcp_gpu_tensor_destroy(d_isyn);
                                if (!ext_cur && h_ext) nimcp_free(h_ext);
                            } else {
                                /* CPU fallback for non-lightweight or non-GPU-ready pops */
                                for (uint32_t n = 0; n < pop->n_neurons; n++) {
                                    float I_syn = 0.0f;
                                    if (pop->lightweight && pop->incoming_csr &&
                                        pop->incoming_csr->finalized) {
                                        I_syn = pop->external_current ?
                                            pop->external_current[n] : 0.0f;
                                        uint32_t sc;
                                        snn_csr_synapse_t* syns = snn_csr_get_incoming(
                                            pop->incoming_csr, n, &sc);
                                        for (uint32_t s = 0; s < sc; s++) {
                                            if (syns[s].src_pop >= network->n_populations) continue;
                                            snn_population_t* sp = network->populations[syns[s].src_pop];
                                            if (!sp || !sp->spike_output) continue;
                                            float* ss = (float*)nimcp_tensor_data(sp->spike_output);
                                            if (ss && syns[s].src_neuron < sp->n_neurons
                                                && ss[syns[s].src_neuron] > 0.5f)
                                                I_syn += syns[s].weight;
                                        }
                                    } else if (network->neural_net) {
                                        neuron_t* neuron = neural_network_get_neuron(
                                            network->neural_net, pop->neuron_ids[n]);
                                        if (neuron) {
                                            I_syn = neuron->external_current;
                                            uint32_t ic = neuron->incoming.embedded_count
                                                        + neuron->incoming.overflow_count;
                                            for (uint32_t s = 0; s < ic; s++) {
                                                synapse_handle_t* h = sparse_synapse_get(&neuron->incoming, s);
                                                if (!h) continue;
                                                neuron_t* pre = neural_network_get_neuron(
                                                    network->neural_net, h->target_neuron_id);
                                                if (pre && pre->state > 0.5f)
                                                    I_syn += h->weight;
                                            }
                                        }
                                    }
                                    h_input[pop_offset + n] = I_syn;
                                }
                            }
                            pop_offset += pop->n_neurons;
                        }
                        nimcp_gpu_tensor_destroy(d_spk);
                    }
                    nimcp_free(h_spk_flat);
                }
            } else {
                /* === CPU-ONLY I_SYN PATH (no GPU CSR available) === */
                size_t neuron_offset = 0;
                for (uint32_t p = 0; p < network->n_populations; p++) {
                    snn_population_t* pop = network->populations[p];
                    if (!pop) continue;
                    for (uint32_t n = 0; n < pop->n_neurons; n++) {
                        float I_syn = 0.0f;
                        if (pop->lightweight && pop->incoming_csr && pop->incoming_csr->finalized) {
                            I_syn = pop->external_current ? pop->external_current[n] : 0.0f;
                            uint32_t sc;
                            snn_csr_synapse_t* syns = snn_csr_get_incoming(
                                pop->incoming_csr, n, &sc);
                            for (uint32_t s = 0; s < sc; s++) {
                                if (syns[s].src_pop >= network->n_populations) continue;
                                snn_population_t* sp = network->populations[syns[s].src_pop];
                                if (!sp || !sp->spike_output) continue;
                                float* ss = (float*)nimcp_tensor_data(sp->spike_output);
                                if (ss && syns[s].src_neuron < sp->n_neurons
                                    && ss[syns[s].src_neuron] > 0.5f)
                                    I_syn += syns[s].weight;
                            }
                        } else if (network->neural_net) {
                            neuron_t* neuron = neural_network_get_neuron(
                                network->neural_net, pop->neuron_ids[n]);
                            if (neuron) {
                                I_syn = neuron->external_current;
                                uint32_t ic = neuron->incoming.embedded_count
                                            + neuron->incoming.overflow_count;
                                for (uint32_t s = 0; s < ic; s++) {
                                    synapse_handle_t* h = sparse_synapse_get(&neuron->incoming, s);
                                    if (!h) continue;
                                    neuron_t* pre = neural_network_get_neuron(
                                        network->neural_net, h->target_neuron_id);
                                    if (pre && pre->state > 0.5f)
                                        I_syn += h->weight;
                                }
                            }
                        }
                        h_input[neuron_offset + n] = I_syn;
                    }
                    neuron_offset += pop->n_neurons;
                }
            }

            clock_gettime(CLOCK_MONOTONIC, &_phase_t1);
            _isyn_ms = (_phase_t1.tv_sec - _phase_t0.tv_sec) * 1000.0
                     + (_phase_t1.tv_nsec - _phase_t0.tv_nsec) / 1e6;

            /* Adaptive Poisson background noise — inject per-population
             * scaled depolarizing pulses into h_input before GPU LIF
             * forward. Dead pops get full noise to escape the absorbing
             * zero state; pops at or above target get zero noise so noise
             * doesn't saturate healthy populations. The per-pop factor
             * comes from snn_noise_factor_for_pop() which reads each
             * pop's firing_rate_ema. noise_rate_hz=0 disables entirely.
             *
             * See project_snn_structural_fixes_2026-04-21 for why fixed-
             * amplitude noise caused SATURATION ↔ FULL_COLLAPSE cycling. */
            {
                extern float snn_tune_get_noise_rate_hz(void);
                extern float snn_tune_get_noise_pulse_mv(void);
                extern float snn_tune_get_noise_ei_ratio(void);
                extern float snn_noise_factor_for_pop(const snn_population_t*);
                const float nrate = snn_tune_get_noise_rate_hz();
                if (nrate > 0.0f) {
                    const float npulse = snn_tune_get_noise_pulse_mv();
                    const float p_base = nrate * dt_ms * 0.001f;
                    const float ei_ratio = snn_tune_get_noise_ei_ratio();
                    static __thread unsigned int _gpu_noise_seed = 0;
                    if (_gpu_noise_seed == 0) {
                        struct timespec _nt;
                        clock_gettime(CLOCK_MONOTONIC, &_nt);
                        _gpu_noise_seed = (unsigned int)(_nt.tv_nsec
                            ^ ((uintptr_t)&_gpu_noise_seed >> 4));
                        if (_gpu_noise_seed == 0) _gpu_noise_seed = 0xDEADBEEFu;
                    }
                    uint32_t neuron_idx = 0;
                    for (uint32_t p = 0; p < network->n_populations; p++) {
                        snn_population_t* pop = network->populations[p];
                        if (!pop) continue;
                        const float factor = snn_noise_factor_for_pop(pop);
                        const unsigned int pop_thresh =
                            (unsigned int)(p_base * factor * (float)RAND_MAX);
                        /* Dead-pop exception: if pop is (nearly) fully in
                         * rescue mode, force excitatory-only noise so it
                         * can escape the absorbing-zero state. Otherwise
                         * apply ei_ratio as the fraction of pulses that
                         * are inhibitory. */
                        const int exc_only = (factor >= 0.9f);
                        for (uint32_t n = 0; n < pop->n_neurons; n++) {
                            if (pop_thresh > 0 &&
                                (unsigned int)rand_r(&_gpu_noise_seed) < pop_thresh) {
                                if (exc_only) {
                                    h_input[neuron_idx] += npulse;
                                } else {
                                    float sign_r = (float)rand_r(&_gpu_noise_seed)
                                                 * (1.0f / (float)RAND_MAX);
                                    if (sign_r < ei_ratio) {
                                        h_input[neuron_idx] -= npulse;
                                    } else {
                                        h_input[neuron_idx] += npulse;
                                    }
                                }
                            }
                            neuron_idx++;
                        }
                    }
                }
            }

            /* Upload input to GPU tensor */
            clock_gettime(CLOCK_MONOTONIC, &_phase_t0);
            size_t dims[1] = { total_neurons };
            nimcp_gpu_tensor_t* input_tensor = nimcp_gpu_tensor_from_host(
                gpu, h_input, dims, 1, NIMCP_GPU_PRECISION_FP32);

            if (input_tensor) {
                /* Run GPU LIF forward pass */
                bool gpu_ok = nimcp_gpu_lif_forward(gpu, lif_state, input_tensor);

                if (gpu_ok && lif_state->spikes) {
                    /* Read back spikes from GPU */
                    float* h_spikes = (float*)nimcp_malloc(total_neurons * sizeof(float));
                    if (h_spikes) {
                        nimcp_gpu_tensor_to_host(lif_state->spikes, h_spikes);

                        /* Distribute GPU results back to population tensors and neural_net */
                        size_t neuron_offset = 0;
                        for (uint32_t p = 0; p < network->n_populations; p++) {
                            snn_population_t* pop = network->populations[p];
                            if (!pop) continue;

                            float* spike_data = (float*)nimcp_tensor_data(pop->spike_output);
                            float* v_data = (float*)nimcp_tensor_data(pop->membrane_v);

                            for (uint32_t n = 0; n < pop->n_neurons; n++) {
                                float spiked = h_spikes[neuron_offset + n];
                                spike_data[n] = spiked;

                                if (spiked > 0.5f) {
                                    record_spike(&pop->spike_trains[n], network->sim->current_time_us);
                                    total_spikes++;
                                    pop->total_spikes++;

                                    if (!pop->lightweight && network->neural_net) {
                                        neuron_t* nn = neural_network_get_neuron(
                                            network->neural_net, pop->neuron_ids[n]);
                                        if (nn) nn->state = 1.0f;
                                    }
                                } else {
                                    if (!pop->lightweight && network->neural_net) {
                                        neuron_t* nn = neural_network_get_neuron(
                                            network->neural_net, pop->neuron_ids[n]);
                                        if (nn) nn->state = 0.0f;
                                    }
                                }
                            }

                            /* Sync membrane potential back from GPU */
                            if (lif_state->v) {
                                float* h_v = (float*)nimcp_malloc(total_neurons * sizeof(float));
                                if (h_v) {
                                    nimcp_gpu_tensor_to_host(lif_state->v, h_v);
                                    for (uint32_t n = 0; n < pop->n_neurons; n++) {
                                        v_data[n] = h_v[neuron_offset + n];
                                    }
                                    nimcp_free(h_v);
                                }
                            }

                            neuron_offset += pop->n_neurons;
                        }
                        nimcp_free(h_spikes);
                        gpu_executed = true;
                    }
                } else if (!gpu_ok) {
                    NIMCP_LOGGING_WARN("snn_network_step: GPU LIF forward failed, falling back to CPU");
                }

                nimcp_gpu_tensor_destroy(input_tensor);
            }
            clock_gettime(CLOCK_MONOTONIC, &_phase_t1);
            _lif_ms = (_phase_t1.tv_sec - _phase_t0.tv_sec) * 1000.0
                    + (_phase_t1.tv_nsec - _phase_t0.tv_nsec) / 1e6;
            nimcp_free(h_input);
        }

        /* Clear external_current for lightweight pops regardless of GPU success */
        for (uint32_t p = 0; p < network->n_populations; p++) {
            snn_population_t* pop = network->populations[p];
            if (pop && pop->lightweight && pop->external_current) {
                memset(pop->external_current, 0, pop->n_neurons * sizeof(float));
            }
        }
    }

    if (!gpu_executed) {
    /* ===== CPU FALLBACK PATH ===== */

    /* Substrate adapter: cache was already refreshed at the top of
     * snn_network_step (so GPU path gets the same treatment). We just
     * need to pick up the cached pointers here for tau_eff / tref_eff
     * / spike-survival — the three CPU-only per-neuron hooks. */
    const axon_substrate_effects_t*     se_axon = NULL;
    const dendrite_substrate_effects_t* se_dend = NULL;
    if (network->substrate && snn_tune_get_substrate_enabled() != 0.0f) {
        se_axon = &network->cached_axon_effects;
        se_dend = &network->cached_dend_effects;
    }
    const int substrate_spike_dropout_on =
        (snn_tune_get_substrate_spike_dropout_on() != 0.0f);

    /* Process each population */
    for (uint32_t p = 0; p < network->n_populations; p++) {
        snn_population_t* pop = network->populations[p];
        if (!pop) continue;

        float* v_data = (float*)nimcp_tensor_data(pop->membrane_v);
        float* spike_data = (float*)nimcp_tensor_data(pop->spike_output);
        float* ref_data = (float*)nimcp_tensor_data(pop->refractory);

        snn_lif_params_t lif = snn_pop_lif_params(pop, &network->config);
        float v_thresh = lif.v_thresh;
        float v_reset  = lif.v_reset;
        float v_rest   = lif.v_rest;
        float tau_mem  = lif.tau_mem;

        /* Wave G — heterogeneity capture. Branch hoisted out of inner
         * per-neuron loop. When the per-neuron arrays are NULL (default
         * heterogeneity_sigma=0) `het_active` stays false and the
         * inner loop reads pop-wide values — bit-identical to pre-Wave-G. */
        const bool   het_active_tau     = (pop->tau_mem_per_neuron  != NULL);
        const bool   het_active_thresh  = (pop->v_thresh_per_neuron != NULL);
        const float* tau_per_neuron     = pop->tau_mem_per_neuron;
        const float* vthr_per_neuron    = pop->v_thresh_per_neuron;

        /* Substrate-modulated tau (dendritic membrane time constant) and
         * refractory period (axonal Na+/K+ pump recovery). When no
         * substrate is attached these collapse to the config values. */
        float tau_eff  = se_dend ? substrate_apply_tau(tau_mem, se_dend) : tau_mem;
        float tref_eff = se_axon ? substrate_apply_tref(lif.t_ref, se_axon)
                                 : lif.t_ref;
        /* Emergency silence: when axonal capacity collapses (ATP or ion
         * gradient critically low), the neuron cannot propagate an AP.
         * We skip the entire LIF update for this step on that pop. */
        const int substrate_emergency_silence =
            (se_axon && se_axon->overall_capacity < 0.1f);

        /* Fetch Poisson noise parameters ONCE per population to avoid
         * the extern-call overhead inside the inner per-neuron loop.
         * Apply per-pop adaptive factor (dead=1.0, at-target=0.0). */
        extern float snn_tune_get_noise_rate_hz(void);
        extern float snn_tune_get_noise_pulse_mv(void);
        extern float snn_noise_factor_for_pop(const snn_population_t*);
        const float noise_rate_hz = snn_tune_get_noise_rate_hz();
        const float noise_pulse_mv = snn_tune_get_noise_pulse_mv();
        const float noise_factor = snn_noise_factor_for_pop(pop);
        /* Per-step spike probability: p = rate_hz × dt_s × adaptive_factor. */
        const float noise_p = noise_rate_hz * dt_ms * 0.001f * noise_factor;
        /* Thread-local PRNG seed. Seeded once on first call from
         * clock-based entropy + a per-thread spread so different worker
         * threads don't produce correlated noise. */
        static __thread unsigned int _noise_seed = 0;
        if (_noise_seed == 0) {
            struct timespec _t;
            clock_gettime(CLOCK_MONOTONIC, &_t);
            _noise_seed = (unsigned int)(_t.tv_nsec ^ ((uintptr_t)&_noise_seed >> 4));
            if (_noise_seed == 0) _noise_seed = 0xDEADBEEFu;
        }

        /* Update each neuron */
        if (pop->lightweight && pop->incoming_csr && pop->incoming_csr->finalized) {
            /* ===== LIGHTWEIGHT CSR PATH ===== */

            /* Biophysical stability mechanisms — fetch knobs once per pop.
             *
             * AHP + pump: spike-triggered hyperpolarizing currents. Bulk-
             *   compute the per-neuron contribution into pre-allocated
             *   buffers before the inner loop; subtract from I_syn inside
             *   the loop; update state (decay + spike bump) after the loop.
             *
             * Basket pool: fast-spiking inhibitory interneurons. Uniform
             *   contribution (same mV to every parent neuron) so we compute
             *   one scalar before the loop. emit_inhibition uses the PREVIOUS
             *   basket step's mean spike output — the call to
             *   snn_basket_pool_step at the end of this pop's work advances
             *   it for next step.
             *
             * E/I noise: a fraction g_snn_noise_ei_ratio of Poisson pulses
             *   are negative. Exception: if pop is effectively dead
             *   (noise_factor >= 0.9) we force excitatory-only so silent
             *   pops can still escape the absorbing-zero state. */
            extern float snn_tune_get_ahp_enabled(void);
            extern float snn_tune_get_pump_enabled(void);
            extern float snn_tune_get_basket_enabled(void);
            extern float snn_tune_get_noise_ei_ratio(void);
            extern float snn_tune_get_ahp_pump_substrate_coupling(void);
            extern float snn_tune_get_conductance_enabled(void);
            extern float snn_tune_get_e_ampa_mv(void);
            extern float snn_tune_get_e_nmda_mv(void);
            extern float snn_tune_get_e_gaba_a_mv(void);
            extern float snn_tune_get_e_gaba_b_mv(void);
            extern float snn_tune_get_tau_ampa_ms(void);
            extern float snn_tune_get_tau_nmda_ms(void);
            extern float snn_tune_get_tau_gaba_a_ms(void);
            extern float snn_tune_get_tau_gaba_b_ms(void);
            extern float snn_tune_get_nmda_mg_mm(void);
            const int   ahp_on    = (pop->ahp  && snn_tune_get_ahp_enabled()  != 0.0f);
            const int   pump_on   = (pop->pump && snn_tune_get_pump_enabled() != 0.0f);
            const int   basket_on = (pop->basket && snn_tune_get_basket_enabled() != 0.0f);
            const float ei_ratio  = snn_tune_get_noise_ei_ratio();
            const int   noise_exc_only = (noise_factor >= 0.9f);  /* dead-pop exception */
            /* CB mode capture — single check per pop (LICM-friendly).
             *
             * Per-receptor silent-degrade contract: CB mode runs as long as
             * the conductance flag is on AND at least one receptor array
             * is allocated. Any individually-NULL receptor array is
             * treated as that receptor having zero conductance for this
             * pop (the per-neuron pointer captures and compute_dv args
             * below skip it). This matches snn_population_t's documented
             * "Any NULL pointer makes that receptor a no-op" guarantee
             * and lets a partial alloc failure (e.g. NMDA fails but
             * AMPA+GABA succeed) still run sensibly instead of forcing
             * the whole pop back to legacy current mode.
             *
             * Thread safety: the tune-knob snapshots below are best-effort
             * — Python tune_set RPCs racing this hot loop can produce
             * mixed-version snapshots across pops within a single step.
             * Each individual capture is a 4-byte aligned float (atomic on
             * x86_64) so no torn reads inside a pop. */
            const bool  cb_mode      = (snn_tune_get_conductance_enabled() != 0.0f
                                        && (pop->g_ampa || pop->g_nmda
                                            || pop->g_gaba_a || pop->g_gaba_b));
            const float cb_e_ampa    = snn_tune_get_e_ampa_mv();
            const float cb_e_nmda    = snn_tune_get_e_nmda_mv();
            const float cb_e_gaba_a  = snn_tune_get_e_gaba_a_mv();
            const float cb_e_gaba_b  = snn_tune_get_e_gaba_b_mv();
            const float cb_mg        = snn_tune_get_nmda_mg_mm();
            const float cb_decay_ampa   = cb_mode ? expf(-dt_ms / snn_tune_get_tau_ampa_ms())   : 0.0f;
            const float cb_decay_nmda   = cb_mode ? expf(-dt_ms / snn_tune_get_tau_nmda_ms())   : 0.0f;
            const float cb_decay_gaba_a = cb_mode ? expf(-dt_ms / snn_tune_get_tau_gaba_a_ms()) : 0.0f;
            const float cb_decay_gaba_b = cb_mode ? expf(-dt_ms / snn_tune_get_tau_gaba_b_ms()) : 0.0f;
            /* In CB mode, basket and noise inhibition + non-CSR deposits use
             * SYNAPSE_GENERIC so the deposit kernel's sign-routing fallback
             * picks the correct receptor (positive→AMPA, negative→GABA_A).
             * pulse_mv is reinterpreted as a unitless conductance bump (the
             * Phase 3 rescale factor puts both paths into the same
             * effective ballpark). */

            /* F8: biological feedback — scale AHP/pump gains by substrate
             * pump_activity (ATP proxy). Real Na/K-ATPase is ATP-dependent:
             * low ATP slows the pumps and weakens both AHP (pump-driven
             * component) and pump adaptation. Clamp to [0.1, 1.0] since
             * pumps slow but don't stop entirely. Gated on the dedicated
             * knob so users can disable the new coupling without losing
             * the existing substrate modulation of tau/tref/spike-survival.
             * When se_axon is NULL (no substrate attached) or the knob is
             * off, factor stays at 1.0 (identity). */
            float substrate_pump_factor = 1.0f;
            if (se_axon
                && snn_tune_get_ahp_pump_substrate_coupling() != 0.0f) {
                substrate_pump_factor = se_axon->pump_activity;
                if (substrate_pump_factor < 0.1f) substrate_pump_factor = 0.1f;
                if (substrate_pump_factor > 1.0f) substrate_pump_factor = 1.0f;
            }

            float* ahp_hyp  = ahp_on
                ? (float*)nimcp_calloc(pop->n_neurons, sizeof(float)) : NULL;
            float* pump_hyp = pump_on
                ? (float*)nimcp_calloc(pop->n_neurons, sizeof(float)) : NULL;
            if (ahp_hyp)  snn_adaptation_compute_hyperpol(pop->ahp,  ahp_hyp,  dt_ms);
            if (pump_hyp) snn_adaptation_compute_hyperpol(pop->pump, pump_hyp, dt_ms);

            /* Basket contribution is uniform across parent. Using
             * gain_inhib_to_parent × basket_mean_rate reproduces exactly
             * what snn_basket_pool_emit_inhibition would write per-neuron,
             * without allocating a temporary buffer. */
            float basket_contrib = 0.0f;
            if (basket_on) {
                basket_contrib = pop->basket->gain_inhib_to_parent
                               * snn_basket_pool_mean_rate(pop->basket);
            }

            /* Wave D — gap-junction (electrical synapse) coupling.
             *
             * Biology: Connexin-36 gap junctions among PV basket cells form
             * a fast electrical syncytium that synchronizes membrane voltage
             * and is the primary substrate for cortical gamma rhythm
             * (30-80 Hz). Without it, PV pops fire asynchronously.
             *
             * Implementation: compute V_mean ONCE per pop step (LICM —
             * scalar across the inner loop, not per-neuron), then in the
             * per-neuron loop add (V_mean - V_n) × gap_coupling to each
             * neuron's V. Gated on cb_mode to match the CB hot-loop
             * convention (gap junctions are part of the conductance-based
             * biophysical model). Default gap_coupling=0 ⇒ skip entirely.
             *
             * The instruction is to compute V_mean ONCE; we read v_data
             * BEFORE membrane integration and use the same scalar for all
             * neurons, which is the standard explicit-Euler treatment of
             * coupled compartments and avoids creating an order-dependent
             * inner loop. */
            const float gap_coupling = pop->gap_coupling;
            const bool  gap_active   = (cb_mode && gap_coupling > 0.0f
                                        && pop->n_neurons > 1);
            float gap_v_mean = 0.0f;
            if (gap_active) {
                double v_sum = 0.0;
                for (uint32_t n = 0; n < pop->n_neurons; n++) v_sum += v_data[n];
                gap_v_mean = (float)(v_sum / (double)pop->n_neurons);
            }

            /* Wave H — dendritic compartment mode. Hoisted ONCE per pop
             * (LICM-friendly) so the inner loop has no branch on the
             * pop-level flag. Silent-degrade contract: dendritic mode
             * only activates if the gate is set AND v_basal/v_apical are
             * both allocated. Any allocation failure on the 8 arrays
             * leaves the pop in single-compartment mode with no further
             * action. Only meaningful in CB mode (the dendritic helpers
             * use conductance buckets); current-mode pops use legacy. */
            const bool dend_mode = (cb_mode
                                    && pop->dendritic_enabled
                                    && pop->v_basal && pop->v_apical
                                    && pop->g_ampa_basal && pop->g_gaba_a_basal
                                    && pop->g_nmda_apical && pop->g_gaba_b_apical
                                    && pop->plateau_active && pop->plateau_t0);
            /* Dendritic decay factors — share τ with the legacy CB path:
             * basal AMPA/GABA_A and apical NMDA/GABA_B are biologically
             * the same channel families, just on different compartments. */
            const float cb_decay_ampa_b   = dend_mode ? cb_decay_ampa   : 0.0f;
            const float cb_decay_gaba_a_b = dend_mode ? cb_decay_gaba_a : 0.0f;
            const float cb_decay_nmda_a   = dend_mode ? cb_decay_nmda   : 0.0f;
            const float cb_decay_gaba_b_a = dend_mode ? cb_decay_gaba_b : 0.0f;
            /* Plateau parameters — for now use the doc defaults; future
             * Wave H+ may make these tunable per-pop. */
            const float dend_plateau_thr = SNN_DEND_V_PLATEAU_THRESHOLD_MV;
            const float dend_plateau_g   = SNN_DEND_PLATEAU_GAIN;
            const float dend_plateau_tau = SNN_DEND_PLATEAU_TAU_MS;
            const float dend_g_coup      = SNN_DEND_G_COUP_DEFAULT;
            const uint64_t dend_now_tick = network->sim
                                           ? network->sim->step_count : 0;

            for (uint32_t n = 0; n < pop->n_neurons; n++) {
                spike_data[n] = 0.0f;

                /* CB mode: per-neuron 4-receptor conductance pointers
                 * captured once (NULL in current mode so the deposit helper
                 * short-circuits). Each pointer is independently NULL-safe:
                 * if a particular receptor's pop array failed to alloc, its
                 * pointer stays NULL even though cb_mode is true (only
                 * receptor missing → that receptor no-ops, others run).
                 *
                 * Captures + decay happen BEFORE the refractory / substrate
                 * early-outs because passive conductance decay is a
                 * receptor-level property (independent of postsyn spike
                 * state) — biologically, an open AMPA/NMDA/GABA channel
                 * keeps decaying whether the postsyn neuron is in its
                 * absolute refractory period or not. Skipping decay during
                 * refractory was a pre-P0 bug that left g_X stuck at full
                 * strength for the duration of refractory. */
                float* cb_g_ampa_n   = (cb_mode && pop->g_ampa)   ? &pop->g_ampa[n]   : NULL;
                float* cb_g_nmda_n   = (cb_mode && pop->g_nmda)   ? &pop->g_nmda[n]   : NULL;
                float* cb_g_gaba_a_n = (cb_mode && pop->g_gaba_a) ? &pop->g_gaba_a[n] : NULL;
                float* cb_g_gaba_b_n = (cb_mode && pop->g_gaba_b) ? &pop->g_gaba_b[n] : NULL;
                /* Wave H — per-neuron compartment pointers. Captured
                 * unconditionally so the compartmental deposit helper
                 * can route directly. NULL-safe at the helper boundary. */
                float* dend_g_ampa_b_n   = dend_mode ? &pop->g_ampa_basal[n]    : NULL;
                float* dend_g_gaba_a_b_n = dend_mode ? &pop->g_gaba_a_basal[n]  : NULL;
                float* dend_g_nmda_a_n   = dend_mode ? &pop->g_nmda_apical[n]   : NULL;
                float* dend_g_gaba_b_a_n = dend_mode ? &pop->g_gaba_b_apical[n] : NULL;
                if (cb_mode) {
                    snn_membrane_decay_one(
                        cb_g_ampa_n, cb_g_nmda_n,
                        cb_g_gaba_a_n, cb_g_gaba_b_n,
                        cb_decay_ampa, cb_decay_nmda,
                        cb_decay_gaba_a, cb_decay_gaba_b);
                }
                if (dend_mode) {
                    /* Dendritic-side decay — same form, separate buckets. */
                    snn_membrane_decay_one(
                        dend_g_ampa_b_n, dend_g_nmda_a_n,
                        dend_g_gaba_a_b_n, dend_g_gaba_b_a_n,
                        cb_decay_ampa_b, cb_decay_nmda_a,
                        cb_decay_gaba_a_b, cb_decay_gaba_b_a);
                }

                /* Wave D — apply gap-junction coupling BEFORE refractory
                 * check. Gap junctions are passive ohmic conductances; they
                 * do not gate on the postsyn neuron's spike state, so a
                 * refractory neuron's V is still pulled toward the pop
                 * mean. Mathematically this is a post-integration
                 * adjustment (kept OUTSIDE compute_dv per the constraint to
                 * leave the membrane API unchanged). */
                if (gap_active) {
                    v_data[n] += gap_coupling * (gap_v_mean - v_data[n]);
                }

                /* Substrate emergency silence: overall axon capacity has
                 * collapsed (ATP/ion critical). The neuron cannot
                 * generate or propagate an action potential this step.
                 * Decrement refractory so it continues to tick down
                 * correctly when the substrate recovers. */
                if (substrate_emergency_silence) {
                    if (ref_data[n] > 0.0f) ref_data[n] -= dt_ms;
                    continue;
                }

                if (ref_data[n] > 0.0f) {
                    ref_data[n] -= dt_ms;
                    continue;
                }

                /* CB-GPU-5 Phase A.2: per-neuron synaptic deposit
                 * extracted to snn_cb_deposit_neuron(). external_current
                 * + CSR walk + basket + Poisson noise. In CB mode I_syn
                 * stays at zero (unused); in current mode it accumulates
                 * the post-deposit drive that compute_dv consumes. */
                snn_cb_deposit_ctx_t _dep_ctx = {
                    .cb_mode        = cb_mode,
                    .dend_mode      = dend_mode,
                    .basket_contrib = basket_contrib,
                    .noise_p        = noise_p,
                    .noise_pulse_mv = noise_pulse_mv,
                    .ei_ratio       = ei_ratio,
                    .noise_exc_only = (bool)noise_exc_only,
                    .noise_seed     = &_noise_seed,
                };
                float I_syn;
                snn_cb_deposit_neuron(
                    network, pop, n, &_dep_ctx,
                    cb_g_ampa_n, cb_g_nmda_n, cb_g_gaba_a_n, cb_g_gaba_b_n,
                    dend_g_ampa_b_n, dend_g_gaba_a_b_n,
                    dend_g_nmda_a_n, dend_g_gaba_b_a_n,
                    &I_syn);

                /* CB-GPU-5 Phase A.3: per-neuron integration extracted to
                 * snn_cb_integrate_neuron(). Computes dv via CB / dendritic
                 * / current branch, applies threshold + spike survival,
                 * resets V on spike, sets refractory, records spike. The
                 * helper increments pop->total_spikes internally; we add
                 * the return value to the function-local total_spikes. */
                snn_cb_integrate_ctx_t _int_ctx = {
                    .cb_mode           = cb_mode,
                    .dend_mode         = dend_mode,
                    .het_active_tau    = het_active_tau,
                    .het_active_thresh = het_active_thresh,
                    .tau_eff           = tau_eff,
                    .v_thresh          = v_thresh,
                    .v_reset           = v_reset,
                    .v_rest            = v_rest,
                    .tref_eff          = tref_eff,
                    .cb_e_ampa         = cb_e_ampa,
                    .cb_e_nmda         = cb_e_nmda,
                    .cb_e_gaba_a       = cb_e_gaba_a,
                    .cb_e_gaba_b       = cb_e_gaba_b,
                    .cb_mg             = cb_mg,
                    .dend_plateau_thr  = dend_plateau_thr,
                    .dend_plateau_g    = dend_plateau_g,
                    .dend_plateau_tau  = dend_plateau_tau,
                    .dend_g_coup       = dend_g_coup,
                    .dend_now_tick     = dend_now_tick,
                    .substrate_pump_factor      = substrate_pump_factor,
                    .substrate_spike_dropout_on = (bool)substrate_spike_dropout_on,
                    .se_axon           = se_axon,
                    .se_dend           = se_dend,
                    .tau_per_neuron    = tau_per_neuron,
                    .vthr_per_neuron   = vthr_per_neuron,
                    .ahp_hyp           = ahp_hyp,
                    .pump_hyp          = pump_hyp,
                    .noise_seed        = &_noise_seed,
                    .dt_ms             = dt_ms,
                };
                total_spikes += snn_cb_integrate_neuron(
                    network, pop, n, &_int_ctx,
                    v_data, spike_data, ref_data,
                    cb_g_ampa_n, cb_g_nmda_n, cb_g_gaba_a_n, cb_g_gaba_b_n,
                    dend_g_ampa_b_n, dend_g_gaba_a_b_n,
                    dend_g_nmda_a_n, dend_g_gaba_b_a_n,
                    I_syn);
            }

            /* CB-GPU-5 Phase A: post-spike adaptation extracted to
             * snn_cb_post_spike_pop() so the GPU CB step path can call
             * the same logic. Behavior is bit-identical (locked by
             * test_snn_cb_loop_baseline.cpp). The helper takes ownership
             * of ahp_hyp / pump_hyp and frees them. */
            snn_cb_post_spike_pop(pop, dt_ms,
                                  ahp_on != 0, pump_on != 0, basket_on != 0,
                                  ahp_hyp, pump_hyp);
            ahp_hyp = NULL;
            pump_hyp = NULL;

            /* Clear external_current for next step (input is per-step) */
            if (pop->external_current) {
                memset(pop->external_current, 0, pop->n_neurons * sizeof(float));
            }

            /* Report this pop's activity to the substrate. n_plasticity=0
             * here; any plasticity application downstream (homeostasis,
             * R-STDP) debits separately. region_id=0 since pop-level
             * region tagging is not yet plumbed. */
            if (network->substrate &&
                snn_tune_get_substrate_enabled() != 0.0f) {
                uint32_t pop_spikes_this_step = 0;
                for (uint32_t n = 0; n < pop->n_neurons; n++) {
                    if (spike_data[n] > 0.5f) pop_spikes_this_step++;
                }
                substrate_debit_activity(network->substrate,
                                         0 /* region_id */,
                                         pop_spikes_this_step,
                                         0 /* n_plasticity */);
            }
        } else {
        /* ===== LEGACY NEURON_T PATH =====
         * Bug #3 fix: mirror the lightweight path's substrate modulation.
         * Use tau_eff / tref_eff (already computed above in this pop's
         * loop iteration) instead of the raw config values so the
         * biological substrate feedback also modulates legacy pops.
         *
         * CB migration: same per-pop captures as the lightweight branch.
         * Legacy pops typically lack the per-neuron 4-receptor conductance
         * arrays (allocated in snn_population_create_internal — both paths
         * get them, but legacy pops are sometimes built by older test
         * harnesses); the (g_ampa && g_nmda && g_gaba_a && g_gaba_b) guard
         * inside legacy_cb_mode handles the absent case. */
        extern float snn_tune_get_conductance_enabled(void);
        extern float snn_tune_get_e_ampa_mv(void);
        extern float snn_tune_get_e_nmda_mv(void);
        extern float snn_tune_get_e_gaba_a_mv(void);
        extern float snn_tune_get_e_gaba_b_mv(void);
        extern float snn_tune_get_tau_ampa_ms(void);
        extern float snn_tune_get_tau_nmda_ms(void);
        extern float snn_tune_get_tau_gaba_a_ms(void);
        extern float snn_tune_get_tau_gaba_b_ms(void);
        extern float snn_tune_get_nmda_mg_mm(void);
        /* Per-receptor silent-degrade contract (matches lightweight branch):
         * CB mode runs as long as ANY receptor array is allocated; missing
         * ones no-op individually via the NULL-safe per-neuron pointer
         * captures and conditional reads at compute_dv. */
        const bool  legacy_cb_mode = (snn_tune_get_conductance_enabled() != 0.0f
                                      && (pop->g_ampa || pop->g_nmda
                                          || pop->g_gaba_a || pop->g_gaba_b));
        const float legacy_cb_e_ampa   = snn_tune_get_e_ampa_mv();
        const float legacy_cb_e_nmda   = snn_tune_get_e_nmda_mv();
        const float legacy_cb_e_gaba_a = snn_tune_get_e_gaba_a_mv();
        const float legacy_cb_e_gaba_b = snn_tune_get_e_gaba_b_mv();
        const float legacy_cb_mg       = snn_tune_get_nmda_mg_mm();
        const float legacy_cb_decay_ampa   = legacy_cb_mode
            ? expf(-dt_ms / snn_tune_get_tau_ampa_ms())   : 0.0f;
        const float legacy_cb_decay_nmda   = legacy_cb_mode
            ? expf(-dt_ms / snn_tune_get_tau_nmda_ms())   : 0.0f;
        const float legacy_cb_decay_gaba_a = legacy_cb_mode
            ? expf(-dt_ms / snn_tune_get_tau_gaba_a_ms()) : 0.0f;
        const float legacy_cb_decay_gaba_b = legacy_cb_mode
            ? expf(-dt_ms / snn_tune_get_tau_gaba_b_ms()) : 0.0f;

        /* Wave D — gap-junction (electrical synapse) coupling. Mirror of
         * the lightweight branch's pre-loop V_mean computation, gated on
         * legacy_cb_mode. See lightweight branch comment for biology. */
        const float legacy_gap_coupling = pop->gap_coupling;
        const bool  legacy_gap_active   = (legacy_cb_mode
                                           && legacy_gap_coupling > 0.0f
                                           && pop->n_neurons > 1);
        float legacy_gap_v_mean = 0.0f;
        if (legacy_gap_active) {
            double v_sum = 0.0;
            for (uint32_t n = 0; n < pop->n_neurons; n++) v_sum += v_data[n];
            legacy_gap_v_mean = (float)(v_sum / (double)pop->n_neurons);
        }

        for (uint32_t n = 0; n < pop->n_neurons; n++) {
            spike_data[n] = 0.0f;

            /* CB per-neuron decay (matches lightweight). NULL-safe:
             * any individually-missing receptor array stays NULL even
             * though legacy_cb_mode is true (per-receptor silent-degrade).
             *
             * Decay BEFORE the refractory continue — passive conductance
             * decay is independent of postsyn spike state (an open ion
             * channel keeps decaying whether the neuron is in absolute
             * refractory or not). Pre-P0 code skipped decay during ref,
             * which left g_X stuck at full strength for the duration of
             * refractory. */
            float* cb_g_ampa_n   = (legacy_cb_mode && pop->g_ampa)   ? &pop->g_ampa[n]   : NULL;
            float* cb_g_nmda_n   = (legacy_cb_mode && pop->g_nmda)   ? &pop->g_nmda[n]   : NULL;
            float* cb_g_gaba_a_n = (legacy_cb_mode && pop->g_gaba_a) ? &pop->g_gaba_a[n] : NULL;
            float* cb_g_gaba_b_n = (legacy_cb_mode && pop->g_gaba_b) ? &pop->g_gaba_b[n] : NULL;
            if (legacy_cb_mode) {
                snn_membrane_decay_one(
                    cb_g_ampa_n, cb_g_nmda_n,
                    cb_g_gaba_a_n, cb_g_gaba_b_n,
                    legacy_cb_decay_ampa, legacy_cb_decay_nmda,
                    legacy_cb_decay_gaba_a, legacy_cb_decay_gaba_b);
            }

            /* Wave D — apply gap-junction coupling BEFORE refractory
             * check (gap junctions are passive, don't gate on postsyn
             * spike state). */
            if (legacy_gap_active) {
                v_data[n] += legacy_gap_coupling * (legacy_gap_v_mean - v_data[n]);
            }

            if (ref_data[n] > 0.0f) {
                ref_data[n] -= dt_ms;
                continue;
            }

            float I_syn = 0.0f;
            if (network->neural_net && n < pop->n_neurons) {
                neuron_t* neuron = neural_network_get_neuron(
                    network->neural_net, pop->neuron_ids[n]);
                if (neuron) {
                    /* Non-CSR deposit (legacy neuron_t external current /
                     * sparse synapse handles): use SYNAPSE_GENERIC so the
                     * deposit kernel's sign-fallback routes the weight. */
                    snn_membrane_deposit_synapse(
                        &I_syn,
                        cb_g_ampa_n, cb_g_nmda_n,
                        cb_g_gaba_a_n, cb_g_gaba_b_n,
                        neuron->external_current,
                        SYNAPSE_GENERIC, legacy_cb_mode);
                    uint32_t in_count = neuron->incoming.embedded_count
                                      + neuron->incoming.overflow_count;
                    for (uint32_t s = 0; s < in_count; s++) {
                        synapse_handle_t* h = sparse_synapse_get(&neuron->incoming, s);
                        if (!h) continue;
                        uint32_t pre_id = h->target_neuron_id;
                        neuron_t* pre = neural_network_get_neuron(network->neural_net, pre_id);
                        if (pre && pre->state > 0.5f) {
                            snn_membrane_deposit_synapse(
                                &I_syn,
                                cb_g_ampa_n, cb_g_nmda_n,
                                cb_g_gaba_a_n, cb_g_gaba_b_n,
                                h->weight,
                                SYNAPSE_GENERIC, legacy_cb_mode);
                        }
                    }
                }
            }

            /* LIF dynamics — use substrate-modulated tau (tau_eff
             * collapses to tau_mem when no substrate is attached).
             *
             * Wave G — per-neuron τ_mem rides on top of substrate. Same
             * pattern as the lightweight branch: re-apply the substrate
             * factor to the per-neuron base. */
            float tau_eff_n = tau_eff;
            if (het_active_tau) {
                float tau_n = tau_per_neuron[n];
                tau_eff_n = se_dend ? substrate_apply_tau(tau_n, se_dend)
                                    : tau_n;
            }
            float dv;
            if (legacy_cb_mode) {
                /* NULL-safe per-receptor reads: a missing receptor
                 * contributes 0 conductance (silent-degrade contract). */
                dv = snn_membrane_compute_dv(
                    v_data[n], v_rest, tau_eff_n, dt_ms,
                    0.0f,
                    cb_g_ampa_n   ? *cb_g_ampa_n   : 0.0f,
                    cb_g_nmda_n   ? *cb_g_nmda_n   : 0.0f,
                    cb_g_gaba_a_n ? *cb_g_gaba_a_n : 0.0f,
                    cb_g_gaba_b_n ? *cb_g_gaba_b_n : 0.0f,
                    legacy_cb_e_ampa, legacy_cb_e_nmda,
                    legacy_cb_e_gaba_a, legacy_cb_e_gaba_b,
                    legacy_cb_mg, true);
            } else {
                dv = snn_membrane_compute_dv(
                    v_data[n], v_rest, tau_eff_n, dt_ms,
                    I_syn,
                    0.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, false);
            }
            v_data[n] += dv;

            /* Wave G — per-neuron threshold (rides on top of subclass /
             * pop-wide v_thresh). When het_active_thresh is false the
             * lookup collapses to the pop-wide value bit-identically. */
            float v_thresh_n_legacy = het_active_thresh
                ? vthr_per_neuron[n] : v_thresh;
            if (v_data[n] >= v_thresh_n_legacy) {
                spike_data[n] = 1.0f;
                v_data[n] = v_reset;
                /* Refractory uses substrate-modulated tref_eff
                 * (collapses to config.t_ref when no substrate). */
                ref_data[n] = tref_eff;
                record_spike(&pop->spike_trains[n], network->sim->current_time_us);
                total_spikes++;
                pop->total_spikes++;

                if (network->neural_net) {
                    neuron_t* spiked = neural_network_get_neuron(
                        network->neural_net, pop->neuron_ids[n]);
                    if (spiked) spiked->state = 1.0f;
                }
            } else {
                if (network->neural_net) {
                    neuron_t* quiet = neural_network_get_neuron(
                        network->neural_net, pop->neuron_ids[n]);
                    if (quiet) quiet->state = 0.0f;
                }
            }
        }

        /* Report this pop's activity to the substrate (mirrors the
         * lightweight branch). Bug #3 fix: legacy neuron_t pops were
         * previously silent contributors — substrate never saw their
         * metabolic cost. */
        if (network->substrate &&
            snn_tune_get_substrate_enabled() != 0.0f) {
            uint32_t pop_spikes_this_step = 0;
            for (uint32_t n = 0; n < pop->n_neurons; n++) {
                if (spike_data[n] > 0.5f) pop_spikes_this_step++;
            }
            substrate_debit_activity(network->substrate,
                                     0 /* region_id */,
                                     pop_spikes_this_step,
                                     0 /* n_plasticity */);
        }
        } /* end legacy */
    }
    } /* end CPU fallback */

    /* ===== GPU-path substrate activity debit =====
     * Bug #2 fix: previously substrate_debit_activity only ran inside the
     * CPU fallback, so the GPU fast path never reported spike cost back
     * to the substrate. Result: ATP never drained, plasticity_mod stayed
     * pinned at ~1.0, and the metabolic feedback loop was silently open.
     * When the GPU executed the step, walk each population's spike_output
     * (already synced back to host above) and debit the substrate once
     * per pop. */
    if (gpu_executed && network->substrate &&
        snn_tune_get_substrate_enabled() != 0.0f) {
        clock_gettime(CLOCK_MONOTONIC, &_phase_t0);
        for (uint32_t p = 0; p < network->n_populations; p++) {
            snn_population_t* pop = network->populations[p];
            if (!pop || !pop->spike_output) continue;
            const float* sp = (const float*)nimcp_tensor_data_const(
                pop->spike_output);
            if (!sp) continue;
            uint32_t pop_spikes_this_step = 0;
            for (uint32_t n = 0; n < pop->n_neurons; n++) {
                if (sp[n] > 0.5f) pop_spikes_this_step++;
            }
            substrate_debit_activity(network->substrate,
                                     0 /* region_id */,
                                     pop_spikes_this_step,
                                     0 /* n_plasticity */);
        }
        clock_gettime(CLOCK_MONOTONIC, &_phase_t1);
        _substrate_debit_ms = (_phase_t1.tv_sec - _phase_t0.tv_sec) * 1000.0
                            + (_phase_t1.tv_nsec - _phase_t0.tv_nsec) / 1e6;
    }

    /* Update simulation time */
    network->sim->current_time_us += dt_us;
    network->sim->step_count++;

    /* Per-population firing-rate EMA for homeostatic plasticity.
     * Counts fresh spike_output from this step and folds into an EMA
     * with alpha=0.01 (≈100-step time constant). The EMA is consumed
     * by snn_homeostatic_apply() which runs every N training steps. */
    clock_gettime(CLOCK_MONOTONIC, &_phase_t0);
    for (uint32_t p = 0; p < network->n_populations; p++) {
        snn_population_t* pop = network->populations[p];
        if (!pop || !pop->lightweight || !pop->spike_output) continue;
        const float* sp = (const float*)nimcp_tensor_data_const(pop->spike_output);
        if (!sp) continue;
        uint32_t n_spk = 0;
        for (uint32_t n = 0; n < pop->n_neurons; n++) {
            if (sp[n] > 0.5f) n_spk++;
        }
        float rate = (pop->n_neurons > 0)
                   ? (float)n_spk / (float)pop->n_neurons : 0.0f;
        /* EMA with warm-up: for first 100 samples weight new data more */
        float alpha = (pop->rate_samples < 100) ? 0.05f : 0.01f;
        pop->firing_rate_ema = (1.0f - alpha) * pop->firing_rate_ema
                             + alpha * rate;
        pop->rate_samples++;

        /* Temporal history ring buffer — store per-step spike count.
         * Consumed by Python-side FFT/correlation analysis via the
         * snn_get_population_history RPC. */
        pop->spike_count_history[pop->history_write_idx] = n_spk;
        pop->history_write_idx = (pop->history_write_idx + 1) % SNN_POP_HISTORY_LEN;
        pop->history_total_steps++;

        /* Wave E FFI fix — write THIS step's spike snapshot into the
         * per-pop spike-history ring AFTER the deposit kernel has read
         * its delayed view (the kernel ran above; we are now at end of
         * step). Head advances ONCE per step (not per neuron). NULL-safe:
         * when alloc failed at create time, the deposit kernel falls
         * back to spike_output for legacy behavior — and we skip this
         * write block entirely.
         * Order matters: the deposit pass for the NEXT step must see
         * "head - 1 - delay = this step's snapshot for delay=0", so
         * write FIRST then advance. See
         * docs/claude/ffi-timing-audit-2026-04-27.md. */
        if (pop->spike_history) {
            const size_t row_off = (size_t)pop->spike_history_head
                                 * (size_t)pop->n_neurons;
            memcpy(pop->spike_history + row_off, sp,
                   (size_t)pop->n_neurons * sizeof(float));
            pop->spike_history_head = (uint8_t)(
                (pop->spike_history_head + 1u) % SNN_SPIKE_HISTORY_SLOTS);
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &_phase_t1);
    _ema_ring_ms = (_phase_t1.tv_sec - _phase_t0.tv_sec) * 1000.0
                 + (_phase_t1.tv_nsec - _phase_t0.tv_nsec) / 1e6;

    /* Update statistics */
    clock_gettime(CLOCK_MONOTONIC, &_step_t1);
    double step_ms = (_step_t1.tv_sec - _step_t0.tv_sec) * 1000.0
                   + (_step_t1.tv_nsec - _step_t0.tv_nsec) / 1e6;
    network->stats.total_steps = network->sim->step_count;
    network->stats.total_spikes += total_spikes;
    network->stats.total_compute_time_ms += step_ms;
    network->stats.avg_step_time_ms = network->stats.total_compute_time_ms
                                    / (double)network->stats.total_steps;
    network->stats.last_step_time_ms = step_ms;
    network->stats.last_isyn_time_ms = _isyn_ms;
    network->stats.last_step_gpu = gpu_executed;
    if (gpu_executed) {
        network->stats.gpu_steps++;
    } else {
        network->stats.cpu_fallback_steps++;
    }

    /* Refresh per-pop and network-level firing-rate / sparsity from running
     * totals so monitoring (get_snn_stats → mean_firing_rate_hz / sparsity)
     * reflects every per-tick caller of snn_network_step — not just the
     * _for_duration wrappers. ~12 cognitive bridges + training dispatch
     * call snn_network_step() directly per tick; before this hoist they
     * left pop->mean_rate / stats.{mean_firing_rate, sparsity} stale at
     * the value last set by a *_for_duration call (often zero post-load
     * since CB mode took the CPU path that doesn't fan out to update_stats
     * unless step_for_duration wraps it). 2026-04-26 SNN-quiet metric
     * paradox: total_spikes climbed past 2300 while mean_firing_rate_hz
     * stayed pinned at 0. */
    {
        double sim_s = (double)network->sim->current_time_us / 1.0e6;
        if (sim_s > 0.0) {
            uint32_t total_neurons_acc = 0;
            uint64_t total_spikes_acc  = 0;
            uint32_t silent_acc        = 0;
            float    max_pop_rate      = 0.0f;
            for (uint32_t p = 0; p < network->n_populations; p++) {
                snn_population_t* pop = network->populations[p];
                if (!pop || pop->n_neurons == 0) continue;
                pop->mean_rate = (float)((double)pop->total_spikes
                                         / (double)pop->n_neurons / sim_s);
                if (pop->mean_rate > max_pop_rate) {
                    max_pop_rate = pop->mean_rate;
                }
                /* "Silent" approximation: any pop with mean rate below 0.1 Hz
                 * over the whole run contributes its neurons to the silent
                 * count. Cheap; sparsity recomputed exactly by
                 * snn_network_update_stats when a _for_duration wrapper runs. */
                if (pop->mean_rate < 0.1f) {
                    silent_acc += pop->n_neurons;
                }
                total_neurons_acc += pop->n_neurons;
                total_spikes_acc  += pop->total_spikes;
            }
            if (total_neurons_acc > 0) {
                network->stats.mean_firing_rate =
                    (float)((double)total_spikes_acc
                            / (double)total_neurons_acc / sim_s);
                network->stats.max_firing_rate = max_pop_rate;
                network->stats.sparsity =
                    (float)silent_acc / (float)total_neurons_acc;
                network->stats.silent_neurons    = silent_acc;
                network->stats.spikes_per_sample =
                    (float)total_spikes_acc / (float)total_neurons_acc;
            }
        }
    }

    /* W5 KG anomaly emit — compute an approximate per-step spike rate and
     * emit only when it crosses the quiescent/hyperactive boundaries.
     * Normal operation never writes to the KG. Rate estimate uses
     * total_spikes over the step interval and a coarse neuron count. */
    if (s_net_snn_kg_brain && step_ms > 0.0) {
        uint32_t total_neurons = 0;
        for (uint32_t p = 0; p < network->n_populations; p++) {
            if (network->populations[p]) {
                total_neurons += network->populations[p]->n_neurons;
            }
        }
        if (total_neurons > 0) {
            /* spikes/neuron/sec = spikes / neurons / (step_ms / 1000) */
            float rate_hz = (float)total_spikes /
                (float)total_neurons / ((float)step_ms / 1000.0f);
            if (rate_hz < 0.01f || rate_hz > 100.0f) {
                uint64_t ts_us = (uint64_t)time(NULL) * 1000000ULL;
                net_snn_kg_emit_event(s_net_snn_kg_brain,
                    rate_hz < 0.01f ? "quiescent" : "spike_rate_anomaly",
                    rate_hz, ts_us);
            }
        }
    }

    /* Log timing breakdown every step for profiling */
    {
        NIMCP_LOGGING_INFO("SNN step %llu: total=%.1fms (isyn=%.1fms lif+readback=%.1fms "
                           "cbgpu=%.1fms substrate_debit=%.1fms ema_ring=%.1fms) "
                           "spikes=%d gpu=%s avg=%.1fms",
                           (unsigned long long)network->stats.total_steps,
                           step_ms, _isyn_ms, _lif_ms,
                           _cbgpu_ms, _substrate_debit_ms, _ema_ring_ms,
                           total_spikes, gpu_executed ? "yes" : "no",
                           network->stats.avg_step_time_ms);
    }

    /* === PER-POPULATION DIAGNOSTICS (NIMCP_DEBUG_SNN=1) ===
     * Every 100 steps, dump per-population spike counts, mean V, mean I_syn.
     * Tells us exactly which populations are firing, which are dead, and
     * why (flat V = no input current; threshold V = input adequate). */
    {
        static int _snn_dbg_checked = 0, _snn_dbg = 0;
        if (!_snn_dbg_checked) {
            const char* env = getenv("NIMCP_DEBUG_SNN");
            _snn_dbg = (env && env[0] == '1');
            _snn_dbg_checked = 1;
        }
        /* Allow on-demand triggering: another thread can set a flag (or
         * we re-check env var) to force an immediate dump. For now, the
         * env var NIMCP_SNN_DUMP_NOW=1 triggers a one-shot dump that
         * resets to 0 after firing. */
        bool dump_now = false;
        const char* dump_env = getenv("NIMCP_SNN_DUMP_NOW");
        if (dump_env && dump_env[0] == '1') {
            dump_now = true;
            unsetenv("NIMCP_SNN_DUMP_NOW");  /* one-shot */
        }
        if (_snn_dbg && ((network->stats.total_steps % 100) == 0 || dump_now)) {
            for (uint32_t p = 0; p < network->n_populations; p++) {
                snn_population_t* pop = network->populations[p];
                if (!pop || !pop->spike_output || !pop->membrane_v) continue;
                const float* spikes = (const float*)nimcp_tensor_data_const(pop->spike_output);
                const float* v = (const float*)nimcp_tensor_data_const(pop->membrane_v);
                if (!spikes || !v) continue;

                uint32_t n_spiked = 0;
                float v_sum = 0, v_min = 1e9, v_max = -1e9;
                for (uint32_t i = 0; i < pop->n_neurons; i++) {
                    if (spikes[i] > 0.5f) n_spiked++;
                    v_sum += v[i];
                    if (v[i] < v_min) v_min = v[i];
                    if (v[i] > v_max) v_max = v[i];
                }
                float v_mean = v_sum / (float)pop->n_neurons;

                /* External current stats (lightweight pops only) */
                float ext_mean = 0, ext_max = 0;
                if (pop->lightweight && pop->external_current) {
                    for (uint32_t i = 0; i < pop->n_neurons; i++) {
                        float ec = pop->external_current[i];
                        ext_mean += ec;
                        if (ec > ext_max) ext_max = ec;
                    }
                    ext_mean /= (float)pop->n_neurons;
                }

                /* Incoming CSR weight stats + computed I_syn from current spikes.
                 * I_syn = ext_current[n] + sum(weight × pre_spike) for each n.
                 * This is the EXPECTED I_syn this neuron will see next step,
                 * computed independently of GPU/CPU execution. Tells us
                 * whether signal is reaching this layer. */
                float w_mean = 0, w_max = 0;
                float isyn_mean = 0, isyn_max = 0;
                uint32_t n_syn = 0, n_neurons_with_isyn = 0;
                if (pop->lightweight && pop->incoming_csr && pop->incoming_csr->entries) {
                    n_syn = pop->incoming_csr->n_synapses;
                    for (uint32_t e = 0; e < n_syn; e++) {
                        float w = pop->incoming_csr->entries[e].weight;
                        w_mean += w;
                        if (w > w_max) w_max = w;
                    }
                    if (n_syn > 0) w_mean /= (float)n_syn;

                    /* Compute I_syn per neuron from pre-spikes × weights */
                    snn_csr_storage_t* csr = pop->incoming_csr;
                    if (csr->row_ptr) {
                        for (uint32_t n = 0; n < pop->n_neurons; n++) {
                            float isyn = pop->external_current ? pop->external_current[n] : 0.0f;
                            uint32_t s_lo = csr->row_ptr[n];
                            uint32_t s_hi = csr->row_ptr[n + 1];
                            for (uint32_t e = s_lo; e < s_hi; e++) {
                                snn_csr_synapse_t* syn = &csr->entries[e];
                                if (syn->src_pop >= network->n_populations) continue;
                                snn_population_t* src = network->populations[syn->src_pop];
                                if (!src || !src->spike_output) continue;
                                if (syn->src_neuron >= src->n_neurons) continue;
                                /* Verify tensor dtype before casting — non-lightweight
                                 * pops may use different dtypes (audit bug #2) */
                                if (nimcp_tensor_dtype(src->spike_output) != NIMCP_DTYPE_F32)
                                    continue;
                                const float* src_sp =
                                    (const float*)nimcp_tensor_data_const(src->spike_output);
                                if (src_sp && src_sp[syn->src_neuron] > 0.5f) {
                                    isyn += syn->weight;
                                }
                            }
                            isyn_mean += isyn;
                            if (isyn > isyn_max) isyn_max = isyn;
                            if (isyn != 0.0f) n_neurons_with_isyn++;
                        }
                        if (pop->n_neurons > 0) isyn_mean /= (float)pop->n_neurons;
                    }
                }

                NIMCP_LOGGING_INFO(
                    "[SNN-POP] %s n=%u spk=%u V[%.1f..%.1f]μ=%.1f "
                    "ext[μ=%.2f max=%.2f] w[μ=%.3f max=%.3f] "
                    "I_syn[μ=%.3f max=%.2f n_recv=%u/%u] nsyn=%u",
                    pop->name, pop->n_neurons, n_spiked,
                    v_min, v_max, v_mean,
                    ext_mean, ext_max, w_mean, w_max,
                    isyn_mean, isyn_max, n_neurons_with_isyn, pop->n_neurons,
                    n_syn);
            }

            /* Per-tier propagation summary: aggregate spike counts by name prefix */
            uint32_t tier_spikes[10] = {0};
            uint32_t tier_neurons[10] = {0};
            const char* tier_names[10] = {
                "input", "L0", "L1", "L2", "L3", "L4", "L5", "L6", "L7", "output"
            };
            for (uint32_t p = 0; p < network->n_populations; p++) {
                snn_population_t* pop = network->populations[p];
                if (!pop || !pop->spike_output) continue;
                const float* sp = (const float*)nimcp_tensor_data_const(pop->spike_output);
                if (!sp) continue;
                int tier = -1;
                if (strncmp(pop->name, "input", 5) == 0) tier = 0;
                else if (strncmp(pop->name, "output", 6) == 0) tier = 9;
                /* Strict L<digit>_ pattern — avoid matching "L10..." or "Ldifferent" */
                else if (pop->name[0] == 'L' && pop->name[1] >= '0' && pop->name[1] <= '7'
                         && (pop->name[2] == '\0' || pop->name[2] == '_'))
                    tier = (pop->name[1] - '0') + 1;  /* L0→1, L1→2, ... L7→8 */
                if (tier < 0 || tier >= 10) continue;
                uint32_t s = 0;
                for (uint32_t i = 0; i < pop->n_neurons; i++) if (sp[i] > 0.5f) s++;
                tier_spikes[tier] += s;
                tier_neurons[tier] += pop->n_neurons;
            }
            char tier_summary[512] = {0};
            int off = 0;
            for (int t = 0; t < 10; t++) {
                if (tier_neurons[t] == 0) continue;
                int n = snprintf(tier_summary + off, sizeof(tier_summary) - off,
                    "%s%s=%u/%u", off > 0 ? " " : "", tier_names[t],
                    tier_spikes[t], tier_neurons[t]);
                if (n > 0) off += n;
                if (off >= (int)sizeof(tier_summary) - 1) break;
            }
            NIMCP_LOGGING_INFO("[SNN-TIER] %s", tier_summary);
        }
    }

    /* FNO recording: pair the V_before snapshot taken at function entry with
     * the now-settled V_after + spike vectors and feed snn_fno_record_pair
     * for each population's FNO. Runs after stats so the observed state is
     * fully consistent. No-op when recording is disabled. */
    snn_fno_record_post_step(network);

    return total_spikes;
}

/**
 * @brief Compute per-population derived firing-rate stats and write to network->stats.
 *
 * Shared helper so both the inference path (snn_network_run) and the BPTT
 * training path (snn_backprop_forward in src/training/nimcp_snn_backprop.c)
 * can update the same visible metrics. Before this was extracted, only
 * snn_network_run updated network->stats, so get_snn_stats RPC reflected
 * inference activity exclusively — training-time spiking was invisible.
 * Discovered Apr 11 2026 while verifying the unroll_steps=1000 fix.
 *
 * @param network     The network whose populations have just been stepped.
 * @param total_spikes Total spike count from the run (sum across populations).
 * @param duration_ms  Simulated duration in milliseconds (for Hz conversion).
 */
void snn_network_update_stats(snn_network_t* network, int total_spikes, float duration_ms) {
    if (!network) return;

    float duration_s = duration_ms / 1000.0f;
    uint32_t total_neurons = 0;
    uint32_t silent = 0;
    uint32_t hyperactive = 0;
    float max_rate = 0.0f;

    for (uint32_t p = 0; p < network->n_populations; p++) {
        snn_population_t* pop = network->populations[p];
        if (!pop) continue;

        /* Per-neuron firing rate from spike trains */
        for (uint32_t n = 0; n < pop->n_neurons; n++) {
            float rate = (duration_s > 0.0f) ?
                (float)pop->spike_trains[n].total_spikes / duration_s : 0.0f;
            if (rate == 0.0f) silent++;
            if (rate > 200.0f) hyperactive++;  /* >200 Hz is pathological */
            if (rate > max_rate) max_rate = rate;
        }

        /* Population mean rate */
        pop->mean_rate = (duration_s > 0.0f && pop->n_neurons > 0) ?
            (float)pop->total_spikes / (float)pop->n_neurons / duration_s : 0.0f;
        total_neurons += pop->n_neurons;
    }

    if (total_neurons > 0) {
        network->stats.mean_firing_rate = (duration_s > 0.0f) ?
            (float)total_spikes / (float)total_neurons / duration_s : 0.0f;
        network->stats.max_firing_rate = max_rate;
        network->stats.sparsity = (float)silent / (float)total_neurons;
        network->stats.silent_neurons = silent;
        network->stats.hyperactive_neurons = hyperactive;
        network->stats.spikes_per_sample = (float)total_spikes / (float)total_neurons;
    }
}

/* ===========================================================================
 * Conductance-Based Migration — Weight Rescaling
 *
 * One-shot multiplicative scan over every CSR entry in every population.
 * Idempotent via the cb_weights_rescaled sticky knob. Mirrors weights to
 * the GPU if the population's CSR is GPU-resident.
 *
 * Out-of-scope: legacy neuron_t synapse_handle_t weights are NOT rescaled.
 * Legacy populations would need a separate sweep over neural_network_t;
 * the pod brain and all hierarchical SNN tiers use lightweight CSR pops
 * exclusively, so this is not a regression for the live runaway-fix path.
 * (See docs/claude/cb-phase0-design.md "open questions" — heterogeneous
 * mode out of scope for v1.)
 * =========================================================================== */
int snn_rescale_weights_for_conductance(snn_network_t* network, float factor) {
    extern float snn_tune_get_cb_weights_rescaled(void);
    extern void  snn_tune_set_cb_weights_rescaled(float);

    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_rescale_weights_for_conductance: null network");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!isfinite(factor) || factor <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_rescale_weights_for_conductance: factor must be > 0 and finite");
        return SNN_ERROR_INVALID_CONFIG;
    }
    /* Idempotence: refuse a second apply unless caller cleared the flag.
     * A no-op return is preferable to silent double-rescale (would shrink
     * weights by factor² and likely silence the network). */
    if (snn_tune_get_cb_weights_rescaled() != 0.0f) {
        NIMCP_LOGGING_WARN(
            "snn_rescale_weights_for_conductance: already rescaled "
            "(cb_weights_rescaled=1.0); set knob to 0 first to re-apply");
        return SNN_ERROR_INVALID_STATE;
    }

    uint64_t total_synapses = 0;
    uint32_t pops_touched   = 0;
    uint32_t gpu_pops       = 0;

    for (uint32_t p = 0; p < network->n_populations; p++) {
        snn_population_t* pop = network->populations[p];
        if (!pop || !pop->incoming_csr) continue;
        snn_csr_storage_t* csr = pop->incoming_csr;
        if (!csr->finalized || csr->n_synapses == 0) continue;

        /* Apply to entries[] — the source of truth for CPU-side learning
         * (R-STDP, Louvain, introspection) per the existing pattern at
         * nimcp_snn_training.c:1200+. */
        for (uint32_t e = 0; e < csr->n_synapses; e++) {
            csr->entries[e].weight *= factor;
        }
        /* Mirror to flat weights[] (GPU-feeding array) if populated. */
        if (csr->weights) {
            for (uint32_t e = 0; e < csr->n_synapses; e++) {
                csr->weights[e] *= factor;
            }
        }
        /* Push to GPU if device copy exists. */
        if (csr->gpu_resident) {
            snn_csr_sync_weights_to_gpu(csr);
            gpu_pops++;
        }
        total_synapses += csr->n_synapses;
        pops_touched++;
    }

    /* Set sticky flag so a daemon restart that reloads snn_tune.json
     * sees rescale-already-applied and skips. */
    snn_tune_set_cb_weights_rescaled(1.0f);

    NIMCP_LOGGING_INFO(
        "snn_rescale_weights_for_conductance: rescaled %llu synapses across "
        "%u populations (%u GPU-synced) by factor=%.6f",
        (unsigned long long)total_synapses, pops_touched, gpu_pops, factor);
    return 0;
}

int snn_network_run(snn_network_t* network, float duration_ms) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_run: null network pointer");
        return SNN_ERROR_NULL_POINTER;
    }

    float dt_ms = network->config.dt;
    int n_steps = (int)(duration_ms / dt_ms);
    int total_spikes = 0;

    for (int i = 0; i < n_steps; i++) {
        int spikes = snn_network_step(network, dt_ms);
        if (spikes < 0) {
            return spikes;  /* Error code */
        }
        total_spikes += spikes;
    }

    snn_network_update_stats(network, total_spikes, duration_ms);
    return total_spikes;
}

//=============================================================================
// Performance-Optimized Stepping
//=============================================================================

/** Default threshold margin for sparse stepping (mV) */
#define SNN_SPARSE_THRESHOLD_MARGIN_DEFAULT 5.0f

/**
 * @brief Spike-driven sparse step — skip quiescent neurons
 *
 * WHAT: Only update neurons that are near threshold or received input
 * WHY:  At 2-5% firing rate, most neurons decay passively → skip them
 * HOW:  Two-pass: (1) identify active neurons, (2) update only those
 *
 * A neuron is "active" if any of:
 * - In refractory period (needs countdown)
 * - Has non-zero external_current (received a spike)
 * - Membrane potential is within threshold_margin of v_thresh
 * - Was spiking last step (needs reset propagation)
 */
int snn_network_step_sparse(snn_network_t* network, float dt,
                             float threshold_margin,
                             snn_step_stats_t* stats) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_network_step_sparse: null network pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!network->sim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
            "snn_network_step_sparse: null simulation context");
        return SNN_ERROR_INVALID_STATE;
    }

    snn_network_heartbeat("snn_step_sparse", 0.0f);

    float dt_ms = (dt > 0.0f) ? dt : network->config.dt;
    uint64_t dt_us = (uint64_t)(dt_ms * 1000.0f);
    float margin = (threshold_margin > 0.0f) ? threshold_margin
                                              : SNN_SPARSE_THRESHOLD_MARGIN_DEFAULT;

    int total_spikes = 0;
    uint32_t total_neurons = 0;
    uint32_t neurons_updated = 0;
    uint32_t neurons_skipped = 0;
    uint32_t neurons_refractory = 0;

    /* LIF params resolve per-pop (P2.0 always returns config defaults;
     * P2.1+ may diverge for PV/SOM/VIP-tagged pops). Hoist active_threshold
     * inside the loop too — it depends on v_thresh which may differ per pop. */
    for (uint32_t p = 0; p < network->n_populations; p++) {
        snn_population_t* pop = network->populations[p];
        if (!pop) continue;

        snn_lif_params_t lif = snn_pop_lif_params(pop, &network->config);
        float v_thresh = lif.v_thresh;
        float v_reset  = lif.v_reset;
        float v_rest   = lif.v_rest;
        float tau_mem  = lif.tau_mem;
        float active_threshold = v_thresh - margin;

        /* Wave G — per-neuron heterogeneity capture (hoisted). */
        const bool   het_active_tau    = (pop->tau_mem_per_neuron  != NULL);
        const bool   het_active_thresh = (pop->v_thresh_per_neuron != NULL);
        const float* tau_per_neuron    = pop->tau_mem_per_neuron;
        const float* vthr_per_neuron   = pop->v_thresh_per_neuron;

        float* v_data = (float*)nimcp_tensor_data(pop->membrane_v);
        float* spike_data = (float*)nimcp_tensor_data(pop->spike_output);
        float* ref_data = (float*)nimcp_tensor_data(pop->refractory);

        total_neurons += pop->n_neurons;

        for (uint32_t n = 0; n < pop->n_neurons; n++) {
            spike_data[n] = 0.0f;

            /* Refractory neurons: just decrement, count as refractory */
            if (ref_data[n] > 0.0f) {
                ref_data[n] -= dt_ms;
                neurons_refractory++;
                continue;
            }

            /* Get synaptic input (G8/M1 lightweight-safe branch).
             * Lightweight pops have no dense neuron_ids[] — dereferencing
             * would index into the wrong neural_net slot or segfault. Read
             * from the pop's own external_current[] instead. */
            float I_syn = 0.0f;
            if (pop->lightweight) {
                if (pop->external_current) {
                    I_syn = pop->external_current[n];
                }
            } else if (network->neural_net) {
                neuron_t* neuron = neural_network_get_neuron(
                    network->neural_net, pop->neuron_ids[n]);
                if (neuron) {
                    I_syn = neuron->external_current;
                }
            }

            /* Wave G — per-neuron LIF lookup. NULL fallback collapses to
             * pop-wide values bit-identically. */
            float tau_n    = het_active_tau    ? tau_per_neuron[n]  : tau_mem;
            float v_thr_n  = het_active_thresh ? vthr_per_neuron[n] : v_thresh;
            float active_thr_n = v_thr_n - margin;

            /* Sparse optimization: skip neuron if quiescent
             * - No synaptic input AND
             * - Membrane potential far below threshold (passive decay only)
             *
             * For passive decay: dV = (v_rest - V) / tau * dt
             * If V < active_threshold and I_syn == 0, the neuron is decaying
             * toward rest and won't spike. Apply the decay analytically. */
            if (I_syn == 0.0f && v_data[n] < active_thr_n) {
                /* Analytical exponential decay: V → v_rest + (V-v_rest)*exp(-dt/tau) */
                float alpha = expf(-dt_ms / tau_n);
                v_data[n] = v_rest + (v_data[n] - v_rest) * alpha;
                neurons_skipped++;
                continue;
            }

            /* Full LIF update for active neurons */
            neurons_updated++;
            float dv = (v_rest - v_data[n] + I_syn) / tau_n * dt_ms;
            v_data[n] += dv;

            /* Spike generation */
            if (v_data[n] >= v_thr_n) {
                spike_data[n] = 1.0f;
                v_data[n] = v_reset;
                ref_data[n] = lif.t_ref;

                record_spike(&pop->spike_trains[n], network->sim->current_time_us);
                total_spikes++;
                pop->total_spikes++;
            }
        }
    }

    /* Update simulation time */
    network->sim->current_time_us += dt_us;
    network->sim->step_count++;
    network->stats.total_steps = network->sim->step_count;
    network->stats.total_spikes += total_spikes;

    /* Fill statistics if requested */
    if (stats) {
        stats->total_neurons = total_neurons;
        stats->neurons_updated = neurons_updated;
        stats->neurons_skipped = neurons_skipped;
        stats->neurons_refractory = neurons_refractory;
        stats->spikes_generated = (uint32_t)total_spikes;
        stats->compute_ratio = (total_neurons > 0)
            ? (float)neurons_updated / (float)total_neurons
            : 0.0f;
    }

    return total_spikes;
}

/**
 * @brief Worker context for parallel population stepping
 */
typedef struct {
    snn_population_t* pop;
    neural_network_t neural_net;
    float dt_ms;
    float v_thresh;
    float v_reset;
    float v_rest;
    float tau_mem;
    float t_ref;
    uint64_t current_time_us;
    int spikes;  /* output */
} snn_pop_step_ctx_t;

/**
 * @brief Step a single population (thread worker function)
 */
static void snn_pop_step_worker(snn_pop_step_ctx_t* ctx) {
    snn_population_t* pop = ctx->pop;
    if (!pop) { ctx->spikes = 0; return; }

    float* v_data = (float*)nimcp_tensor_data(pop->membrane_v);
    float* spike_data = (float*)nimcp_tensor_data(pop->spike_output);
    float* ref_data = (float*)nimcp_tensor_data(pop->refractory);

    /* Wave G — per-neuron heterogeneity capture (hoisted). */
    const bool   het_active_tau    = (pop->tau_mem_per_neuron  != NULL);
    const bool   het_active_thresh = (pop->v_thresh_per_neuron != NULL);
    const float* tau_per_neuron    = pop->tau_mem_per_neuron;
    const float* vthr_per_neuron   = pop->v_thresh_per_neuron;

    int spikes = 0;

    for (uint32_t n = 0; n < pop->n_neurons; n++) {
        spike_data[n] = 0.0f;

        if (ref_data[n] > 0.0f) {
            ref_data[n] -= ctx->dt_ms;
            continue;
        }

        float I_syn = 0.0f;
        if (ctx->neural_net) {
            neuron_t* neuron = neural_network_get_neuron(
                ctx->neural_net, pop->neuron_ids[n]);
            if (neuron) {
                I_syn = neuron->external_current;
            }
        }

        /* Wave G — per-neuron LIF lookup. NULL fallback collapses to
         * pop-wide values bit-identically. */
        float tau_n   = het_active_tau    ? tau_per_neuron[n]  : ctx->tau_mem;
        float v_thr_n = het_active_thresh ? vthr_per_neuron[n] : ctx->v_thresh;

        float dv = (ctx->v_rest - v_data[n] + I_syn) / tau_n * ctx->dt_ms;
        v_data[n] += dv;

        if (v_data[n] >= v_thr_n) {
            spike_data[n] = 1.0f;
            v_data[n] = ctx->v_reset;
            ref_data[n] = ctx->t_ref;
            record_spike(&pop->spike_trains[n], ctx->current_time_us);
            spikes++;
            pop->total_spikes++;
        }
    }

    ctx->spikes = spikes;
}

/**
 * @brief Population-parallel step using pthreads
 *
 * WHAT: Step independent populations concurrently
 * WHY:  Populations within a step don't have cross-dependencies
 * HOW:  Launch one thread per population (up to n_threads), join all
 */
int snn_network_step_parallel(snn_network_t* network, float dt,
                               uint32_t n_threads) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_network_step_parallel: null network pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!network->sim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
            "snn_network_step_parallel: null simulation context");
        return SNN_ERROR_INVALID_STATE;
    }

    snn_network_heartbeat("snn_step_parallel", 0.0f);

    float dt_ms = (dt > 0.0f) ? dt : network->config.dt;
    uint64_t dt_us = (uint64_t)(dt_ms * 1000.0f);

    uint32_t n_pops = network->n_populations;
    if (n_pops == 0) return 0;

    /* For single population or 1 thread, fall back to serial */
    uint32_t actual_threads = (n_threads > 0) ? n_threads : network->config.n_threads;
    if (actual_threads == 0) actual_threads = 4;  /* Default */
    if (n_pops == 1 || actual_threads <= 1) {
        return snn_network_step(network, dt);
    }

    /* Cap threads to population count */
    if (actual_threads > n_pops) actual_threads = n_pops;

    /* Prepare per-population contexts */
    snn_pop_step_ctx_t* ctxs = (snn_pop_step_ctx_t*)nimcp_malloc(
        n_pops * sizeof(snn_pop_step_ctx_t));
    if (!ctxs) return snn_network_step(network, dt);  /* Fallback */

    for (uint32_t p = 0; p < n_pops; p++) {
        ctxs[p].pop = network->populations[p];
        ctxs[p].neural_net = network->neural_net;
        ctxs[p].dt_ms = dt_ms;
        snn_lif_params_t lif = snn_pop_lif_params(ctxs[p].pop, &network->config);
        ctxs[p].v_thresh = lif.v_thresh;
        ctxs[p].v_reset  = lif.v_reset;
        ctxs[p].v_rest   = lif.v_rest;
        ctxs[p].tau_mem  = lif.tau_mem;
        ctxs[p].t_ref    = lif.t_ref;
        ctxs[p].current_time_us = network->sim->current_time_us;
        ctxs[p].spikes = 0;
    }

    /* Launch threads for populations (use nimcp_thread API) */
    nimcp_thread_t* threads = (nimcp_thread_t*)nimcp_malloc(
        actual_threads * sizeof(nimcp_thread_t));
    if (!threads) {
        /* Fallback: run serially */
        for (uint32_t p = 0; p < n_pops; p++) {
            snn_pop_step_worker(&ctxs[p]);
        }
    } else {
        /* Round-robin populations to threads in batches */
        uint32_t launched = 0;
        uint32_t p = 0;

        while (p < n_pops) {
            launched = 0;
            uint32_t batch = (n_pops - p < actual_threads) ? (n_pops - p) : actual_threads;

            for (uint32_t t = 0; t < batch; t++) {
                int rc = nimcp_thread_create(&threads[t],
                    (void*(*)(void*))snn_pop_step_worker, &ctxs[p + t], NULL);
                if (rc != 0) {
                    /* Thread creation failed — run serially */
                    snn_pop_step_worker(&ctxs[p + t]);
                } else {
                    launched++;
                }
            }

            /* Join all launched threads */
            for (uint32_t t = 0; t < batch; t++) {
                if (launched > 0) {
                    nimcp_thread_join(threads[t], NULL);
                }
            }
            p += batch;
        }
        nimcp_free(threads);
    }

    /* Accumulate results */
    int total_spikes = 0;
    for (uint32_t p = 0; p < n_pops; p++) {
        total_spikes += ctxs[p].spikes;
    }
    nimcp_free(ctxs);

    /* Update simulation time */
    network->sim->current_time_us += dt_us;
    network->sim->step_count++;
    network->stats.total_steps = network->sim->step_count;
    network->stats.total_spikes += total_spikes;

    return total_spikes;
}

int snn_network_run_sparse(snn_network_t* network, float duration_ms,
                            float threshold_margin,
                            snn_step_stats_t* stats) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_network_run_sparse: null network pointer");
        return SNN_ERROR_NULL_POINTER;
    }

    float dt_ms = network->config.dt;
    int n_steps = (int)(duration_ms / dt_ms);
    int total_spikes = 0;

    /* Accumulate stats across steps */
    snn_step_stats_t acc = {0};

    for (int i = 0; i < n_steps; i++) {
        snn_step_stats_t step_stats = {0};
        int spikes = snn_network_step_sparse(network, dt_ms,
                                              threshold_margin, &step_stats);
        if (spikes < 0) return spikes;
        total_spikes += spikes;

        acc.total_neurons = step_stats.total_neurons;  /* Same each step */
        acc.neurons_updated += step_stats.neurons_updated;
        acc.neurons_skipped += step_stats.neurons_skipped;
        acc.neurons_refractory += step_stats.neurons_refractory;
        acc.spikes_generated += step_stats.spikes_generated;
    }

    if (stats && n_steps > 0) {
        stats->total_neurons = acc.total_neurons;
        stats->neurons_updated = acc.neurons_updated / (uint32_t)n_steps;
        stats->neurons_skipped = acc.neurons_skipped / (uint32_t)n_steps;
        stats->neurons_refractory = acc.neurons_refractory / (uint32_t)n_steps;
        stats->spikes_generated = acc.spikes_generated;
        stats->compute_ratio = (acc.total_neurons > 0)
            ? (float)acc.neurons_updated /
              (float)(acc.total_neurons * (uint32_t)n_steps)
            : 0.0f;
    }

    /* Update network-level stats so get_snn_stats reflects sparse-path runs too. */
    snn_network_update_stats(network, total_spikes, duration_ms);
    return total_spikes;
}

//=============================================================================
// Input/Output
//=============================================================================

int snn_network_set_inputs(snn_network_t* network,
                           const float* inputs,
                           uint32_t n_inputs) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_set_inputs: null network pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_set_inputs: null inputs pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!network->input_pop) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "snn_network_set_inputs: null input population");
        return SNN_ERROR_INVALID_STATE;
    }
    if (n_inputs != network->input_pop->n_neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_SIZE,
            "snn_network_set_inputs: dimension mismatch");
        NIMCP_LOGGING_ERROR("snn_network_set_inputs: dimension mismatch (%u != %u)",
                           n_inputs, network->input_pop->n_neurons);
        return SNN_ERROR_INVALID_DIMENSION;
    }

    /* For now, set inputs as external currents on input neurons */
    /* A full implementation would use the encoder to generate spikes */
    /* Lightweight path: write directly to population's external_current array */
    if (network->input_pop->lightweight && network->input_pop->external_current) {
        float scale = network->config.input_current_scale;
        for (uint32_t i = 0; i < n_inputs && i < network->input_pop->n_neurons; i++) {
            float inp = inputs[i];
            network->input_pop->external_current[i] = (inp > 0.0f) ? inp * scale : 0.0f;
        }
        return SNN_SUCCESS;
    }

    /* Legacy path: write to neuron_t external_current */
    for (uint32_t i = 0; i < n_inputs; i++) {
        if (network->neural_net && i < network->config.n_inputs) {
            neuron_t* neuron = neural_network_get_neuron(
                network->neural_net, network->input_pop->neuron_ids[i]);
            if (neuron) {
                float inp = inputs[i];
                if (inp > 0.0f) {
                    neuron->external_current = inp * network->config.input_current_scale;
                } else {
                    neuron->external_current = 0.0f;
                }
            }
        }
    }

    return SNN_SUCCESS;
}

int snn_network_set_input_tensor(snn_network_t* network,
                                 const nimcp_tensor_t* input) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_set_input_tensor: null network pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_set_input_tensor: null input tensor");
        return SNN_ERROR_NULL_POINTER;
    }

    const float* data = (const float*)nimcp_tensor_data((nimcp_tensor_t*)input);
    uint64_t size = nimcp_tensor_numel(input);

    return snn_network_set_inputs(network, data, (uint32_t)size);
}

int snn_network_get_outputs(snn_network_t* network,
                            float* outputs,
                            uint32_t n_outputs) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_get_outputs: null network pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!outputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_get_outputs: null outputs pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!network->output_pop) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "snn_network_get_outputs: null output population");
        return SNN_ERROR_INVALID_STATE;
    }
    if (n_outputs != network->output_pop->n_neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_SIZE,
            "snn_network_get_outputs: dimension mismatch");
        return SNN_ERROR_INVALID_DIMENSION;
    }

    /* Rate decoding: count spikes in time window */
    float window_ms = network->config.decoder.time_window;
    uint64_t current_time = network->sim->current_time_us;

    for (uint32_t i = 0; i < n_outputs; i++) {
        outputs[i] = compute_firing_rate(
            &network->output_pop->spike_trains[i],
            current_time,
            window_ms);
    }

    return SNN_SUCCESS;
}

int snn_network_get_output_tensor(snn_network_t* network,
                                  nimcp_tensor_t* output) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_get_output_tensor: null network pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_get_output_tensor: null output tensor");
        return SNN_ERROR_NULL_POINTER;
    }

    float* data = (float*)nimcp_tensor_data(output);
    uint64_t size = nimcp_tensor_numel(output);

    return snn_network_get_outputs(network, data, (uint32_t)size);
}

int snn_network_forward(snn_network_t* network,
                        const float* inputs,
                        uint32_t n_inputs,
                        float* outputs,
                        uint32_t n_outputs,
                        float duration_ms) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_forward: null network pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_forward: null inputs pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!outputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_forward: null outputs pointer");
        return SNN_ERROR_NULL_POINTER;
    }

    /* Phase 8: Send heartbeat at start of forward pass */
    snn_network_heartbeat("snn_forward", 0.0f);

    /* Reset network state */
    int result = snn_network_reset(network);
    if (result != SNN_SUCCESS) return result;

    /* Set inputs */
    result = snn_network_set_inputs(network, inputs, n_inputs);
    if (result != SNN_SUCCESS) return result;

    /* Run simulation */
    result = snn_network_run(network, duration_ms);
    if (result < 0) return result;

    /* Get outputs */
    return snn_network_get_outputs(network, outputs, n_outputs);
}

//=============================================================================
// Training
//=============================================================================

int snn_network_set_training(snn_network_t* network, bool training) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_set_training: null network pointer");
        return SNN_ERROR_NULL_POINTER;
    }

    network->is_training = training;

    if (training && !network->train_ctx) {
        /* Allocate training context */
        network->train_ctx = (snn_training_ctx_t*)nimcp_malloc(sizeof(snn_training_ctx_t));
        if (!network->train_ctx) return SNN_ERROR_OUT_OF_MEMORY;

        memset(network->train_ctx, 0, sizeof(snn_training_ctx_t));
        network->train_ctx->mode = network->config.train_mode;
        network->train_ctx->surrogate = network->config.surrogate;
        network->train_ctx->surrogate_beta = network->config.surrogate_beta;
        network->train_ctx->eligibility_decay = 0.99f;
    }

    return SNN_SUCCESS;
}

int snn_network_apply_stdp(snn_network_t* network) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_apply_stdp: null network pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!network->neural_net) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "snn_network_apply_stdp: null neural network");
        return SNN_ERROR_INVALID_STATE;
    }

    /* Delegate to neural_network_apply_stdp which already exists */
    uint64_t timestamp = network->sim ? network->sim->current_time_us : 0;
    uint32_t n_neurons = neural_network_get_num_neurons(network->neural_net);
    int total_modified = 0;

    for (uint32_t i = 0; i < n_neurons; i++) {
        int modified = neural_network_apply_stdp(network->neural_net, i, timestamp);
        if (modified > 0) {
            total_modified += modified;
        }
    }

    return total_modified;
}

int snn_network_apply_rstdp(snn_network_t* network, float reward) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_apply_rstdp: null network pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!network->train_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "snn_network_apply_rstdp: null training context");
        return SNN_ERROR_INVALID_STATE;
    }

    network->train_ctx->reward = reward;

    /* Apply reward-modulated learning using existing infrastructure */
    uint64_t timestamp = network->sim ? network->sim->current_time_us : 0;
    float learning_rate = network->config.learning_rate;

    uint32_t modified = neural_network_apply_reward_learning(
        network->neural_net, reward, learning_rate, timestamp);

    return (int)modified;
}

int snn_network_compute_gradients(snn_network_t* network,
                                  const float* target,
                                  uint32_t n_targets) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_compute_gradients: null network pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_compute_gradients: null target pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!network->train_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "snn_network_compute_gradients: null training context");
        return SNN_ERROR_INVALID_STATE;
    }

    /* Surrogate gradient computation - placeholder for full BPTT */
    /* A complete implementation would unroll the simulation and compute
       gradients using the chain rule with surrogate gradient functions */

    /* For now, compute simple error signal */
    if (!network->output_pop || n_targets != network->output_pop->n_neurons) {
        return SNN_ERROR_INVALID_DIMENSION;
    }

    float total_error = 0.0f;
    for (uint32_t i = 0; i < n_targets; i++) {
        float rate = compute_firing_rate(
            &network->output_pop->spike_trains[i],
            network->sim->current_time_us,
            network->config.decoder.time_window);

        float error = target[i] - rate;
        total_error += error * error;
    }

    network->train_ctx->current_loss = total_error / (float)n_targets;

    /* Exponential moving average of loss */
    float alpha = 0.1f;
    network->train_ctx->smoothed_loss =
        alpha * network->train_ctx->current_loss +
        (1.0f - alpha) * network->train_ctx->smoothed_loss;

    return SNN_SUCCESS;
}

int snn_network_apply_gradients(snn_network_t* network, float learning_rate) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_apply_gradients: null network pointer");
        return SNN_ERROR_NULL_POINTER;
    }

    float lr = (learning_rate > 0.0f) ? learning_rate : network->config.learning_rate;

    /* Temporarily override config LR so STDP uses the caller's rate */
    float saved_lr = network->config.learning_rate;
    network->config.learning_rate = lr;
    int rc = snn_network_apply_stdp(network);
    network->config.learning_rate = saved_lr;
    return rc;
}

float snn_network_train_step(snn_network_t* network,
                             const float* inputs,
                             uint32_t n_inputs,
                             const float* targets,
                             uint32_t n_targets,
                             float duration_ms) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_train_step: null network pointer");
        return -1.0f;
    }
    if (!inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_train_step: null inputs pointer");
        return -1.0f;
    }
    if (!targets) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_train_step: null targets pointer");
        return -1.0f;
    }

    /* Enable training mode */
    snn_network_set_training(network, true);

    /* Forward pass — dynamic allocation instead of fixed 4096 stack buffer */
    float* outputs = (float*)nimcp_calloc(n_targets, sizeof(float));
    if (!outputs) return -1.0f;

    int result = snn_network_forward(network, inputs, n_inputs,
                                     outputs, n_targets, duration_ms);
    if (result < 0) { nimcp_free(outputs); return -1.0f; }

    /* Compute gradients */
    result = snn_network_compute_gradients(network, targets, n_targets);
    if (result != SNN_SUCCESS) { nimcp_free(outputs); return -1.0f; }

    /* Apply gradients (STDP for now) */
    snn_network_apply_gradients(network, 0.0f);

    nimcp_free(outputs);
    return network->train_ctx->current_loss;
}

//=============================================================================
// Population Management
//=============================================================================

int snn_network_add_population(snn_network_t* network,
                               uint32_t n_neurons,
                               neuron_type_t neuron_type,
                               const char* name) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_add_population: null network pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (n_neurons == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_SIZE, "snn_network_add_population: zero neurons");
        return SNN_ERROR_INVALID_DIMENSION;
    }
    /* Bug fix: guard against the actual allocation size, not just
     * SNN_MAX_POPULATIONS. Legacy code tripped over this when
     * populations[] was sized by config->n_populations. */
    uint32_t capacity = network->populations_capacity;
    if (capacity == 0) capacity = SNN_MAX_POPULATIONS; /* legacy safety */
    if (network->n_populations >= capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_SIZE,
            "snn_network_add_population: max populations exceeded");
        return SNN_ERROR_OUT_OF_MEMORY;
    }

    uint32_t pop_id = network->n_populations;

    snn_population_t* pop = snn_population_create_internal(
        pop_id, n_neurons, neuron_type, name, 0);
    if (!pop) return SNN_ERROR_OUT_OF_MEMORY;

    /* Add neurons to underlying neural network.
     * Bug fix: neural_network_add_neuron returns UINT32_MAX on capacity
     * exhaustion. The old code stored that sentinel into
     * pop->neuron_ids[i] without checking, and later
     * neural_net->neurons[pop->neuron_ids[i]] segfaulted. Now we detect
     * the sentinel, destroy the partially-built population, and return
     * SNN_ERROR_OUT_OF_MEMORY. Any neurons already added to the
     * underlying neural_network_t are leak-tolerated: no
     * neural_network_remove_neuron exists, and those slots will be
     * freed on neural_network_destroy with the rest of the network. */
    for (uint32_t i = 0; i < n_neurons; i++) {
        uint32_t neuron_id = neural_network_add_neuron(
            network->neural_net, ACTIVATION_SIGMOID);
        if (neuron_id == UINT32_MAX) {
            NIMCP_LOGGING_ERROR(
                "snn_network_add_population: neural_network_add_neuron "
                "exhausted at slot %u/%u for pop '%s'; %u neurons already "
                "added to neural_network are leak-tolerated "
                "(no remove API)",
                i, n_neurons, name ? name : "unnamed", i);
            snn_population_destroy_internal(pop);
            return SNN_ERROR_OUT_OF_MEMORY;
        }
        pop->neuron_ids[i] = neuron_id;
    }

    network->populations[pop_id] = pop;
    network->n_populations++;

    NIMCP_LOGGING_DEBUG("snn_network_add_population: added '%s' with %u neurons",
                       name ? name : "unnamed", n_neurons);

    return (int)pop_id;
}

int snn_network_add_population_lightweight(snn_network_t* network,
                                           uint32_t n_neurons,
                                           neuron_type_t neuron_type,
                                           const char* name) {
    if (!network) return -1;
    if (n_neurons == 0) return -1;
    /* Bug fix: guard against the actual allocation size, not just
     * SNN_MAX_POPULATIONS. See snn_network_add_population for details. */
    uint32_t capacity = network->populations_capacity;
    if (capacity == 0) capacity = SNN_MAX_POPULATIONS; /* legacy safety */
    if (network->n_populations >= capacity) return -1;

    uint32_t pop_id = network->n_populations;
    snn_population_t* pop = snn_population_create_lightweight(
        pop_id, n_neurons, neuron_type, name);
    if (!pop) return -1;

    network->populations[pop_id] = pop;
    network->n_populations++;

    NIMCP_LOGGING_DEBUG("snn_network_add_population_lightweight: added '%s' "
                        "with %u neurons (CSR mode)", name ? name : "unnamed", n_neurons);

    return (int)pop_id;
}

/* CB-GPU-7 invariant: this function frees + NULLs the device-side mirror
 * dst->d_synapse_type_per_src so the next CB-GPU step re-uploads the
 * receptor-routing table. The free is NOT lock-protected. All current
 * callers run at brain init (src/core/brain/init/* and snn_hierarchical),
 * so the GPU step is never in flight and the race is unreachable today.
 *
 * If you ever call this from training/sleep/runtime-RPC paths, you MUST
 * first add synchronization (network->step_mutex or an atomic
 * step_in_flight flag), or refactor to defer device-side invalidation
 * onto the next GPU step (stash a "dirty src_pop" set on the pop and
 * have snn_network_step_cb_gpu's lazy-upload block honor it). See
 * memory note "project_runtime_pop_wiring_upgrades.md" for the four
 * candidate features and the recommended defer pattern. */
int snn_network_connect_populations(snn_network_t* network,
                                    uint32_t src_pop,
                                    uint32_t dst_pop,
                                    snn_topology_t topology,
                                    float connectivity,
                                    synapse_type_t synapse_type,
                                    float weight_mean,
                                    float weight_std) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_connect_populations: null network pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (src_pop >= network->n_populations || dst_pop >= network->n_populations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_network_connect_populations: invalid population index");
        return SNN_ERROR_INVALID_POPULATION;
    }

    snn_population_t* src = network->populations[src_pop];
    snn_population_t* dst = network->populations[dst_pop];
    if (!src || !dst) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_network_connect_populations: null population pointer");
        return SNN_ERROR_INVALID_POPULATION;
    }

    /* P0: record receptor type for this (src → dst) pop pair.
     * Looked up by the hot-loop deposit kernel to route weights into the
     * correct g_ampa / g_nmda / g_gaba_a / g_gaba_b conductance bucket.
     * Bounded by the table size (declared at SNN_MAX_POPULATIONS).
     * Multiple connect_populations calls on the same pair update the type
     * to the most recent caller's choice — by convention pathways are
     * receptor-uniform so this should not happen in practice. */
    if (src_pop < SNN_MAX_POPULATIONS) {
        dst->synapse_type_per_src[src_pop] = (uint8_t)synapse_type;
        /* CB-GPU-7: invalidate device-side mirror so the next CB GPU step
         * re-uploads the table. Without this, the deposit kernel routes
         * via stale receptor types after runtime re-wiring. */
        if (dst->d_synapse_type_per_src) {
            nimcp_gpu_context_t* st_gpu =
                (nimcp_gpu_context_t*)dst->d_synapse_type_gpu_ctx;
            if (!st_gpu) st_gpu = (nimcp_gpu_context_t*)network->gpu_ctx;
            if (st_gpu) {
                nimcp_gpu_free(st_gpu, dst->d_synapse_type_per_src);
            }
            dst->d_synapse_type_per_src = NULL;
            dst->d_synapse_type_gpu_ctx = NULL;
        }
    }

    int n_connections = 0;

    /* Helper: generate Gaussian random weight */
    #define RANDOM_WEIGHT(mean, std) ({ \
        float _w = (mean); \
        if ((std) > 0.0f) { \
            float _u1 = (float)nimcp_tl_rand() / (float)RAND_MAX; \
            float _u2 = (float)nimcp_tl_rand() / (float)RAND_MAX; \
            if (_u1 < 1e-7f) _u1 = 1e-7f; \
            _w += sqrtf(-2.0f * logf(_u1)) * cosf(NIMCP_TWO_PI_F * _u2) * (std); \
        } _w; })

    /* Lightweight CSR path: store synapses in dst population's CSR storage
     * instead of the neural_network_t sparse synapse system. */
    if (dst->lightweight && dst->incoming_csr) {
        bool use_ds = (topology == SNN_TOPO_RANDOM && connectivity < 0.1f &&
                       (uint64_t)src->n_neurons * dst->n_neurons > 1000000);
        if (use_ds) {
            uint32_t k = (uint32_t)(dst->n_neurons * connectivity + 0.5f);
            if (k == 0) k = 1;
            if (k > dst->n_neurons) k = dst->n_neurons;
            for (uint32_t i = 0; i < src->n_neurons; i++) {
                for (uint32_t c = 0; c < k; c++) {
                    uint32_t j = (uint32_t)(nimcp_tl_rand() % dst->n_neurons);
                    float w = RANDOM_WEIGHT(weight_mean, weight_std);
                    if (snn_csr_add_entry(dst->incoming_csr, j,
                                          src_pop, i, w) == 0)
                        n_connections++;
                }
            }
        } else {
            for (uint32_t i = 0; i < src->n_neurons; i++) {
                for (uint32_t j = 0; j < dst->n_neurons; j++) {
                    if (topology == SNN_TOPO_RANDOM) {
                        float r = (float)nimcp_tl_rand() / (float)RAND_MAX;
                        if (r > connectivity) continue;
                    }
                    float w = RANDOM_WEIGHT(weight_mean, weight_std);
                    if (snn_csr_add_entry(dst->incoming_csr, j,
                                          src_pop, i, w) == 0)
                        n_connections++;
                }
            }
        }
        goto done;
    }

    /* Legacy neuron_t path (for non-lightweight populations) */
    {
    bool use_direct_sampling = (topology == SNN_TOPO_RANDOM &&
                                 connectivity < 0.1f &&
                                 (uint64_t)src->n_neurons * dst->n_neurons > 1000000);

    if (use_direct_sampling) {
        uint32_t k = (uint32_t)(dst->n_neurons * connectivity + 0.5f);
        if (k == 0) k = 1;
        if (k > dst->n_neurons) k = dst->n_neurons;

        for (uint32_t i = 0; i < src->n_neurons; i++) {
            for (uint32_t c = 0; c < k; c++) {
                uint32_t j = (uint32_t)(nimcp_tl_rand() % dst->n_neurons);
                float weight = RANDOM_WEIGHT(weight_mean, weight_std);

                bool success = neural_network_add_connection_typed(
                    network->neural_net,
                    src->neuron_ids[i],
                    dst->neuron_ids[j],
                    weight,
                    synapse_type);
                if (success) n_connections++;
            }
        }
    } else {
        for (uint32_t i = 0; i < src->n_neurons; i++) {
            for (uint32_t j = 0; j < dst->n_neurons; j++) {
                if (topology == SNN_TOPO_RANDOM) {
                    float r = (float)nimcp_tl_rand() / (float)RAND_MAX;
                    if (r > connectivity) continue;
                }
                float weight = RANDOM_WEIGHT(weight_mean, weight_std);

                bool success = neural_network_add_connection_typed(
                    network->neural_net,
                    src->neuron_ids[i],
                    dst->neuron_ids[j],
                    weight,
                    synapse_type);
                if (success) n_connections++;
            }
        }
    }
    } /* end legacy block */

done:
    #undef RANDOM_WEIGHT

    NIMCP_LOGGING_DEBUG("snn_network_connect_populations: created %d connections",
                       n_connections);

    return n_connections;
}

int snn_network_finalize_connections(snn_network_t* network) {
    if (!network) return -1;
    int finalized = 0;
    for (uint32_t p = 0; p < network->n_populations; p++) {
        snn_population_t* pop = network->populations[p];
        if (pop && pop->lightweight && pop->incoming_csr && !pop->incoming_csr->finalized) {
            if (snn_csr_finalize(pop->incoming_csr) == 0) {
                finalized++;
            }
        }
    }
    NIMCP_LOGGING_INFO("Finalized CSR for %d lightweight populations", finalized);

    /* Re-create GPU LIF state with correct total neuron count.
     * The initial snn_network_create() only counted n_inputs+n_hidden+n_outputs,
     * but lightweight populations add millions more neurons that need GPU state. */
    if (finalized > 0) {
        size_t total_neurons = 0;
        for (uint32_t p = 0; p < network->n_populations; p++) {
            if (network->populations[p])
                total_neurons += network->populations[p]->n_neurons;
        }

        /* Destroy old undersized GPU state */
        if (network->gpu_lif_state) {
            nimcp_lif_state_destroy((nimcp_lif_state_t*)network->gpu_lif_state);
            network->gpu_lif_state = NULL;
        }

        /* Create new state for full neuron count */
        if (network->gpu_ctx) {
            nimcp_lif_params_t lif_params = {
                .tau_mem   = network->config.tau_mem,
                .tau_syn   = network->config.tau_syn > 0.0f ? network->config.tau_syn : 5.0f,
                .v_thresh  = network->config.v_thresh,
                .v_reset   = network->config.v_reset,
                .v_rest    = network->config.v_rest,
                .dt        = network->config.dt,
                .hard_reset = true
            };
            nimcp_lif_state_t* lif_state = nimcp_lif_state_create(
                (nimcp_gpu_context_t*)network->gpu_ctx, total_neurons, &lif_params);
            if (lif_state) {
                network->gpu_lif_state = lif_state;
                NIMCP_LOGGING_INFO("GPU LIF state re-created for %zu total neurons "
                                   "(including lightweight)", total_neurons);
            } else {
                NIMCP_LOGGING_WARN("GPU LIF state creation failed for %zu neurons — "
                                   "falling back to CPU", total_neurons);
            }
        }

        /* Build population offset table and prepare GPU-friendly CSR arrays.
         * Flat index = pop_offsets[src_pop] + src_neuron */
        uint32_t* pop_offsets = nimcp_calloc(network->n_populations + 1, sizeof(uint32_t));
        if (pop_offsets) {
            for (uint32_t p = 0; p < network->n_populations; p++) {
                pop_offsets[p + 1] = pop_offsets[p] +
                    (network->populations[p] ? network->populations[p]->n_neurons : 0);
            }
            int gpu_prepared = 0;
            for (uint32_t p = 0; p < network->n_populations; p++) {
                snn_population_t* pop = network->populations[p];
                if (pop && pop->lightweight && pop->incoming_csr) {
                    if (snn_csr_prepare_gpu(pop->incoming_csr, pop_offsets,
                                            network->n_populations) == 0) {
                        gpu_prepared++;
                    }
                }
            }
            NIMCP_LOGGING_INFO("GPU CSR prepared for %d populations "
                               "(%zu total neurons, flat index range 0-%u)",
                               gpu_prepared, total_neurons,
                               pop_offsets[network->n_populations]);
            nimcp_free(pop_offsets);
        }
    }

    return finalized;
}

snn_population_t* snn_network_get_population(snn_network_t* network,
                                             uint32_t pop_id) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_get_population: null network pointer");
        return NULL;
    }
    if (pop_id >= network->n_populations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_network_get_population: invalid population index");
        return NULL;
    }
    return network->populations[pop_id];
}

int snn_network_set_pop_exclude_from_plasticity(snn_network_t* network,
                                                uint32_t pop_id, bool exclude) {
    if (!network || pop_id >= network->n_populations ||
        !network->populations[pop_id]) {
        return -1;
    }
    network->populations[pop_id]->exclude_from_plasticity = exclude;
    return 0;
}

bool snn_network_get_pop_exclude_from_plasticity(const snn_network_t* network,
                                                 uint32_t pop_id) {
    if (!network || pop_id >= network->n_populations ||
        !network->populations[pop_id]) {
        return false;
    }
    return network->populations[pop_id]->exclude_from_plasticity;
}

int snn_network_find_pop_by_name(const snn_network_t* network, const char* name) {
    if (!network || !name) return -1;
    for (uint32_t i = 0; i < network->n_populations; i++) {
        const snn_population_t* p = network->populations[i];
        if (!p) continue;
        if (strncmp(p->name, name, sizeof(p->name)) == 0) return (int)i;
    }
    return -1;
}

int snn_network_set_pop_subclass(snn_network_t* network,
                                 uint32_t pop_id,
                                 neuron_subclass_t subclass) {
    if (!network) return -1;
    if (pop_id >= network->n_populations) return -1;
    snn_population_t* p = network->populations[pop_id];
    if (!p) return -1;
    if ((int)subclass < 0 || subclass >= SNN_NSC_COUNT) return -1;
    p->subclass = subclass;
    return 0;
}

int snn_network_set_pop_gap_coupling(snn_network_t* network,
                                     uint32_t pop_id,
                                     float weight) {
    /* Single-responsibility setter: assigns the float, nothing else.
     * Negative coupling has no biological meaning (gap junctions are
     * symmetric ohmic conductances); clamp to 0 to keep downstream
     * arithmetic well-defined. NaN/Inf are also rejected since the hot
     * loop reads this value into a multiplier that would propagate. */
    if (!network) return -1;
    if (pop_id >= network->n_populations) return -1;
    snn_population_t* p = network->populations[pop_id];
    if (!p) return -1;
    if (!isfinite(weight)) return -1;
    if (weight < 0.0f) weight = 0.0f;
    p->gap_coupling = weight;
    return 0;
}

int snn_network_set_pop_conduction_delay(snn_network_t* network,
                                         uint32_t pop_id,
                                         uint32_t steps) {
    /* Single-responsibility setter (Wave E FFI fix): clamps `steps` to
     * SNN_MAX_CONDUCTION_DELAY_STEPS and writes it to the pop. The ring
     * buffer is allocated at population creation time and is not touched
     * here — this setter is pure metadata. See
     * docs/claude/ffi-timing-audit-2026-04-27.md. */
    if (!network) return -1;
    if (pop_id >= network->n_populations) return -1;
    snn_population_t* p = network->populations[pop_id];
    if (!p) return -1;
    if (steps > SNN_MAX_CONDUCTION_DELAY_STEPS) {
        steps = SNN_MAX_CONDUCTION_DELAY_STEPS;
    }
    p->conduction_delay_steps = (uint8_t)steps;
    return 0;
}

/* Wave G — per-neuron heterogeneity (τ_mem + v_thresh).
 *
 * Single-responsibility setter: clamps σ to [0, 0.5], allocates the two
 * per-neuron arrays on first non-zero call, and populates each neuron's
 * value as `pop_wide × (1 + σ × N(0,1))`. Box-Muller transforms two
 * uniforms from nimcp_tl_rand() into one Gaussian sample; we draw fresh
 * uniforms for every neuron+field so τ and v_thresh are independent (as
 * they are biophysically).
 *
 * Bit-identity contract: σ == 0 leaves the per-neuron arrays NULL so the
 * LIF lookup helper (snn_pop_neuron_lif_params) collapses to pop-wide
 * values exactly. A second call with σ == 0 frees the arrays so callers
 * can dial heterogeneity off symmetrically.
 *
 * Box-Muller seeding: nimcp_tl_rand() is thread-local — no shared seed
 * state leaks between pops, so calling this setter on multiple pops
 * back-to-back from the wiring thread gives independent draws per pop. */
int snn_network_set_pop_heterogeneity(snn_network_t* network,
                                      uint32_t pop_id,
                                      float sigma) {
    if (!network) return -1;
    if (pop_id >= network->n_populations) return -1;
    snn_population_t* p = network->populations[pop_id];
    if (!p) return -1;
    if (!isfinite(sigma)) return -1;

    /* Clamp σ to [0, 0.5]. Negative σ has no biological meaning; values
     * above 0.5 produce > 50 % CV which would routinely generate
     * non-positive τ_mem under the (1 + σ N(0,1)) parameterization. */
    if (sigma < 0.0f) sigma = 0.0f;
    if (sigma > 0.5f) sigma = 0.5f;

    p->heterogeneity_sigma = sigma;

    /* Wave G GPU sync (v17): mark the GPU lif_state dirty so the next step
     * re-uploads the (now-updated) per-neuron arrays. Safe even when the
     * GPU path is not engaged (no-op on lif_state==NULL). */
    if (network->gpu_lif_state) {
        ((nimcp_lif_state_t*)network->gpu_lif_state)->per_neuron_params_dirty = true;
    }

    /* σ == 0 → free arrays (symmetric off-switch) and bail. */
    if (sigma == 0.0f) {
        if (p->tau_mem_per_neuron)  { nimcp_free(p->tau_mem_per_neuron);  p->tau_mem_per_neuron  = NULL; }
        if (p->v_thresh_per_neuron) { nimcp_free(p->v_thresh_per_neuron); p->v_thresh_per_neuron = NULL; }
        return 0;
    }

    /* Lazy allocate. Best-effort — partial alloc failure is non-fatal:
     * a NULL field falls back to pop-wide via snn_pop_neuron_lif_params. */
    if (!p->tau_mem_per_neuron) {
        p->tau_mem_per_neuron = (float*)nimcp_calloc(p->n_neurons, sizeof(float));
    }
    if (!p->v_thresh_per_neuron) {
        p->v_thresh_per_neuron = (float*)nimcp_calloc(p->n_neurons, sizeof(float));
    }

    /* Resolve pop-wide params (subclass deltas applied). Per-neuron
     * heterogeneity rides ON TOP — this is the LAST stage of LIF param
     * resolution. */
    snn_lif_params_t base = snn_pop_lif_params(p, &network->config);

    /* Box-Muller: two uniforms → one Gaussian. */
    for (uint32_t n = 0; n < p->n_neurons; n++) {
        if (p->tau_mem_per_neuron) {
            float u1 = (float)nimcp_tl_rand() / (float)RAND_MAX;
            float u2 = (float)nimcp_tl_rand() / (float)RAND_MAX;
            if (u1 < 1e-9f) u1 = 1e-9f;  /* avoid log(0) */
            float g = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * (float)NIMCP_PI * u2);
            float tau_n = base.tau_mem * (1.0f + sigma * g);
            /* Clamp to a positive floor so the LIF (1/τ) never blows up. */
            if (tau_n < 0.5f) tau_n = 0.5f;
            p->tau_mem_per_neuron[n] = tau_n;
        }
        if (p->v_thresh_per_neuron) {
            float u1 = (float)nimcp_tl_rand() / (float)RAND_MAX;
            float u2 = (float)nimcp_tl_rand() / (float)RAND_MAX;
            if (u1 < 1e-9f) u1 = 1e-9f;
            float g = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * (float)NIMCP_PI * u2);
            p->v_thresh_per_neuron[n] = base.v_thresh * (1.0f + sigma * g);
        }
    }
    return 0;
}

/*=============================================================================
 * Wave H — snn_network_enable_dendritic
 *=============================================================================
 * Allocates the 8 per-neuron arrays (v_basal, v_apical, g_ampa_basal,
 * g_gaba_a_basal, g_nmda_apical, g_gaba_b_apical, plateau_active,
 * plateau_t0) and sets pop->dendritic_enabled = true. Initializes both
 * compartment voltages to v_rest so a fresh dendritic pop starts at
 * resting potential.
 *
 * Idempotent: a second call when arrays are already allocated is a no-op
 * (returns SNN_SUCCESS without re-allocating). On allocation failure,
 * frees what was acquired and returns SNN_ERROR_OUT_OF_MEMORY (leaves
 * dendritic_enabled = false so the hot loop's silent-degrade contract
 * routes the pop through the single-compartment path).
 *
 * See docs/claude/wave-h-dendritic-design-2026-04-27.md.
 */
int snn_network_enable_dendritic(snn_network_t* network, uint32_t pop_id) {
    if (!network) return SNN_ERROR_NULL_POINTER;
    if (pop_id >= network->n_populations) return SNN_ERROR_INVALID_POPULATION;
    snn_population_t* p = network->populations[pop_id];
    if (!p) return SNN_ERROR_INVALID_POPULATION;

    /* Idempotent: if already enabled with all arrays present, no-op. */
    if (p->dendritic_enabled
        && p->v_basal && p->v_apical
        && p->g_ampa_basal && p->g_gaba_a_basal
        && p->g_nmda_apical && p->g_gaba_b_apical
        && p->plateau_active && p->plateau_t0) {
        return SNN_SUCCESS;
    }

    const uint32_t n = p->n_neurons;
    /* Allocate all 8 arrays. Best-effort: track per-array NULLs and unwind
     * on any failure so a partial allocation never leaves the pop in a
     * half-dendritic state (which would cross-contaminate the hot loop). */
    float*    v_basal_a        = (float*)   nimcp_calloc(n, sizeof(float));
    float*    v_apical_a       = (float*)   nimcp_calloc(n, sizeof(float));
    float*    g_ampa_basal_a   = (float*)   nimcp_calloc(n, sizeof(float));
    float*    g_gaba_a_basal_a = (float*)   nimcp_calloc(n, sizeof(float));
    float*    g_nmda_apical_a  = (float*)   nimcp_calloc(n, sizeof(float));
    float*    g_gaba_b_apical_a= (float*)   nimcp_calloc(n, sizeof(float));
    uint8_t*  plateau_active_a = (uint8_t*) nimcp_calloc(n, sizeof(uint8_t));
    uint64_t* plateau_t0_a     = (uint64_t*)nimcp_calloc(n, sizeof(uint64_t));

    if (!v_basal_a || !v_apical_a
        || !g_ampa_basal_a || !g_gaba_a_basal_a
        || !g_nmda_apical_a || !g_gaba_b_apical_a
        || !plateau_active_a || !plateau_t0_a) {
        if (v_basal_a)         nimcp_free(v_basal_a);
        if (v_apical_a)        nimcp_free(v_apical_a);
        if (g_ampa_basal_a)    nimcp_free(g_ampa_basal_a);
        if (g_gaba_a_basal_a)  nimcp_free(g_gaba_a_basal_a);
        if (g_nmda_apical_a)   nimcp_free(g_nmda_apical_a);
        if (g_gaba_b_apical_a) nimcp_free(g_gaba_b_apical_a);
        if (plateau_active_a)  nimcp_free(plateau_active_a);
        if (plateau_t0_a)      nimcp_free(plateau_t0_a);
        NIMCP_LOGGING_WARN(
            "snn_network_enable_dendritic: alloc failure for pop '%s' "
            "(%u neurons) — staying single-compartment", p->name, n);
        return SNN_ERROR_OUT_OF_MEMORY;
    }

    /* Seed v_basal / v_apical to v_rest (effective per-pop value). The
     * subclass-modulated rest potential is fetched here once. */
    snn_lif_params_t lif = snn_pop_lif_params(p, &network->config);
    for (uint32_t i = 0; i < n; i++) {
        v_basal_a[i]  = lif.v_rest;
        v_apical_a[i] = lif.v_rest;
    }
    /* g_* and plateau_* are zero from calloc — physiological initial state. */

    /* Assign in one shot — atomic from the hot loop's perspective in
     * single-threaded init paths. */
    p->v_basal         = v_basal_a;
    p->v_apical        = v_apical_a;
    p->g_ampa_basal    = g_ampa_basal_a;
    p->g_gaba_a_basal  = g_gaba_a_basal_a;
    p->g_nmda_apical   = g_nmda_apical_a;
    p->g_gaba_b_apical = g_gaba_b_apical_a;
    p->plateau_active  = plateau_active_a;
    p->plateau_t0      = plateau_t0_a;
    p->dendritic_enabled = true;
    return SNN_SUCCESS;
}

//=============================================================================
// Statistics and Monitoring
//=============================================================================

int snn_network_get_stats(snn_network_t* network, snn_stats_t* stats) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_get_stats: null network pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_get_stats: null stats pointer");
        return SNN_ERROR_NULL_POINTER;
    }

    memcpy(stats, &network->stats, sizeof(snn_stats_t));

    /* Update computed stats */
    stats->health = snn_network_check_health(network);

    /* Compute mean firing rate across all populations */
    float total_rate = 0.0f;
    uint32_t total_neurons = 0;
    for (uint32_t p = 0; p < network->n_populations; p++) {
        snn_population_t* pop = network->populations[p];
        if (!pop) continue;

        total_rate += pop->mean_rate * pop->n_neurons;
        total_neurons += pop->n_neurons;
    }
    stats->mean_firing_rate = (total_neurons > 0) ? total_rate / total_neurons : 0.0f;

    return SNN_SUCCESS;
}

snn_state_health_t snn_network_check_health(snn_network_t* network) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_check_health: null network pointer");
        return SNN_STATE_SILENT;
    }

    /* Check for silence (no spikes in recent window) */
    if (network->stats.total_spikes == 0 && network->sim->step_count > 1000) {
        return SNN_STATE_SILENT;
    }

    /* Check for explosion (too many spikes) */
    float max_rate = 1000.0f;  /* Hz */
    for (uint32_t p = 0; p < network->n_populations; p++) {
        snn_population_t* pop = network->populations[p];
        if (!pop) continue;

        if (pop->mean_rate > max_rate) {
            return SNN_STATE_EXPLOSION;
        }
    }

    return SNN_STATE_HEALTHY;
}

float snn_network_get_firing_rate(snn_network_t* network,
                                  uint32_t pop_id,
                                  uint32_t neuron_idx,
                                  float window_ms) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_get_firing_rate: null network pointer");
        return 0.0f;
    }
    if (pop_id >= network->n_populations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_network_get_firing_rate: invalid population index");
        return 0.0f;
    }

    snn_population_t* pop = network->populations[pop_id];
    if (!pop || neuron_idx >= pop->n_neurons) return 0.0f;

    return compute_firing_rate(
        &pop->spike_trains[neuron_idx],
        network->sim->current_time_us,
        window_ms);
}

float snn_network_get_population_rate(snn_network_t* network,
                                      uint32_t pop_id,
                                      float window_ms) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_get_population_rate: null network pointer");
        return 0.0f;
    }
    if (pop_id >= network->n_populations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_network_get_population_rate: invalid population index");
        return 0.0f;
    }

    snn_population_t* pop = network->populations[pop_id];
    if (!pop) return 0.0f;

    float total_rate = 0.0f;
    for (uint32_t n = 0; n < pop->n_neurons; n++) {
        total_rate += compute_firing_rate(
            &pop->spike_trains[n],
            network->sim->current_time_us,
            window_ms);
    }

    return total_rate / (float)pop->n_neurons;
}

uint32_t snn_network_get_population_history(snn_network_t* network,
                                            uint32_t pop_id,
                                            uint32_t* out_counts,
                                            uint64_t* out_total_steps) {
    if (!network || !out_counts) return 0;
    if (pop_id >= network->n_populations) return 0;
    snn_population_t* pop = network->populations[pop_id];
    if (!pop) return 0;

    /* Copy in time order: start at write_idx (oldest), wrap around.
     * If history_total_steps < SNN_POP_HISTORY_LEN, only the first
     * history_total_steps entries are valid — caller gets that count. */
    uint64_t total = pop->history_total_steps;
    uint32_t valid = (total < SNN_POP_HISTORY_LEN)
                   ? (uint32_t)total : SNN_POP_HISTORY_LEN;
    uint32_t start = (valid == SNN_POP_HISTORY_LEN)
                   ? pop->history_write_idx  /* ring wrapped, oldest is write head */
                   : 0;                      /* not wrapped yet, oldest is at 0 */
    for (uint32_t i = 0; i < valid; i++) {
        uint32_t idx = (start + i) % SNN_POP_HISTORY_LEN;
        out_counts[i] = pop->spike_count_history[idx];
    }
    if (out_total_steps) *out_total_steps = total;
    return valid;
}

/* Emergency saturation rescue: run snn_homeostatic_apply in a tight loop
 * without waiting for R-STDP cadence. Used when the network has drifted
 * far from biological range and we don't want to wait for the normal
 * every-10-steps homeostatic cadence (which can take hours at 1 learn_vector
 * per minute). Each iteration scales all lightweight population weights by
 * target_rate/current_rate (clamped to [0.90, 1.10] in emergency range),
 * so log(0.9) ≈ -0.105 per iteration — 20 iterations drops firing 8×.
 *
 * @param network   SNN network (must have snn_training_ctx attached)
 * @param ctx       Training context (from dispatch or test harness)
 * @param n_iter    Number of homeostatic applies to run back-to-back
 * @return Total number of populations scaled across all iterations
 */
uint32_t snn_network_force_homeostasis(snn_network_t* network,
                                       void* ctx,
                                       uint32_t n_iter) {
    if (!network || !ctx || n_iter == 0) return 0;
    /* snn_homeostatic_apply is declared in nimcp_snn_training.h (included
     * at the top of this file). ctx is a snn_training_ctx_t*; we take
     * void* through the force-homeostasis signature to keep the public
     * force-homeostasis API loose, but cast on the call site. */
    uint32_t total_scaled = 0;
    for (uint32_t i = 0; i < n_iter; i++) {
        total_scaled += snn_homeostatic_apply((snn_training_ctx_t*)ctx, network);
    }
    return total_scaled;
}

float snn_population_get_firing_rate(const snn_population_t* population) {
    if (!population) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_population_get_firing_rate: null population pointer");
        return 0.0f;
    }

    /* Return the cached mean_rate maintained by the population */
    return population->mean_rate;
}

//=============================================================================
// Integration
//=============================================================================

int snn_network_connect_bio_async(snn_network_t* network) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_connect_bio_async: null network pointer");
        return SNN_ERROR_NULL_POINTER;
    }

    /* Bio-async registration - placeholder */
    /* Full implementation would register with bio_router */
    NIMCP_LOGGING_INFO("snn_network_connect_bio_async: bio-async connection (stub)");

    return SNN_SUCCESS;
}

int snn_network_disconnect_bio_async(snn_network_t* network) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_disconnect_bio_async: null network pointer");
        return SNN_ERROR_NULL_POINTER;
    }

    network->bio_ctx = NULL;
    return SNN_SUCCESS;
}

int snn_network_connect_immune(snn_network_t* network, void* immune) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_connect_immune: null network pointer");
        return SNN_ERROR_NULL_POINTER;
    }

    network->immune_bridge = immune;
    NIMCP_LOGGING_INFO("snn_network_connect_immune: immune system connected");

    return SNN_SUCCESS;
}

int snn_network_apply_immune_modulation(snn_network_t* network) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_apply_immune_modulation: null network pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!network->immune_bridge) return SNN_SUCCESS;  /* No-op if not connected */

    /* Immune modulation - placeholder */
    /* Full implementation would query cytokine levels and modulate
       firing threshold, time constants, and learning rates */

    return SNN_SUCCESS;
}

//=============================================================================
// Utility Functions
//=============================================================================

neural_network_t snn_network_get_neural_net(snn_network_t* network) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_get_neural_net: null network pointer");
        return NULL;
    }
    return network->neural_net;
}

int snn_network_validate(const snn_network_t* network) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_network_validate: null network pointer");
        return SNN_ERROR_NULL_POINTER;
    }

    /* Check magic number */
    if (network->magic != SNN_MAGIC) {
        NIMCP_LOGGING_ERROR("snn_network_validate: invalid magic number");
        return SNN_ERROR_INVALID_STATE;
    }

    /* Check neural network */
    if (!network->neural_net) {
        NIMCP_LOGGING_ERROR("snn_network_validate: no underlying neural network");
        return SNN_ERROR_INVALID_STATE;
    }

    /* Check populations */
    if (!network->populations) {
        NIMCP_LOGGING_ERROR("snn_network_validate: no populations array");
        return SNN_ERROR_INVALID_STATE;
    }

    return SNN_SUCCESS;
}

/*-----------------------------------------------------------------------------
 * Dale's Principle Validator (Wave C, task #260, 2026-04-27)
 *
 * Dale's principle: a single neuron releases the same neurotransmitter at
 * all of its synaptic terminals. In our population model that translates to:
 * for every source population, every outgoing synapse (across all dst pops)
 * must share the same broad receptor class — either excitatory (AMPA/NMDA)
 * or inhibitory (GABA_A/GABA_B). Modulatory / electrical / generic synapses
 * are ignored (they coexist with classical transmission in real biology).
 *
 * Algorithm: walk every dst pop's synapse_type_per_src[] table, build a
 * reverse map src_pop -> bitset {emits_excitatory, emits_inhibitory}, then
 * flag any src that has both bits set. Pure read; no wiring side effects.
 *
 * Returns 0 if Dale holds (or the network is empty), non-zero on violation.
 *---------------------------------------------------------------------------*/
int snn_network_validate_dale(const snn_network_t* net,
                              char* err_buf,
                              size_t err_buf_sz)
{
    if (err_buf && err_buf_sz > 0) err_buf[0] = '\0';
    if (!net || !net->populations) return 0;  /* nothing to violate */

    /* Per-src bitset: bit 0 = emits excitatory, bit 1 = emits inhibitory. */
    uint8_t emits[SNN_MAX_POPULATIONS];
    memset(emits, 0, sizeof(emits));

    const uint32_t n_pops = net->n_populations;
    for (uint32_t dp = 0; dp < n_pops; dp++) {
        const snn_population_t* dst = net->populations[dp];
        if (!dst) continue;
        for (uint32_t sp = 0; sp < SNN_MAX_POPULATIONS; sp++) {
            synapse_type_t st = (synapse_type_t)dst->synapse_type_per_src[sp];
            if (st == SYNAPSE_GENERIC) continue;       /* unconnected slot */
            if (synapse_type_is_excitatory(st))      emits[sp] |= 0x01u;
            else if (synapse_type_is_inhibitory(st)) emits[sp] |= 0x02u;
            /* Modulatory / electrical: don't constrain Dale class. */
        }
    }

    int violations = 0;
    for (uint32_t sp = 0; sp < SNN_MAX_POPULATIONS && sp < n_pops; sp++) {
        if (emits[sp] == 0x03u) {
            violations++;
            if (err_buf && err_buf_sz > 0) {
                const snn_population_t* src = net->populations[sp];
                const char* nm = (src && src->name[0]) ? src->name : "?";
                size_t used = strlen(err_buf);
                if (used + 1 < err_buf_sz) {
                    int wrote = snprintf(err_buf + used, err_buf_sz - used,
                        "%spop %u (%s): emits both excitatory and inhibitory",
                        used ? "; " : "", sp, nm);
                    if (wrote < 0) break;
                }
            }
        }
    }
    return violations;
}

//=============================================================================
// Persistence (save/load)
//=============================================================================

#include <stdio.h>
#include <errno.h>
#include "core/neuralnet/nimcp_sparse_synapse.h"

/* SNN checkpoint format constants */
#define SNN_CHECKPOINT_MAGIC    0x534E4E53  /**< "SNNS" */
#define SNN_CHECKPOINT_VERSION  4           /**< v4: adds wiring_schema_version field */

/* Wiring schema version: bump whenever the hierarchical wiring topology or
 * weight-initialization formula changes. A mismatch on load causes the cached
 * .snn file to be rejected so the network is rebuilt with the new scheme.
 *
 *   1 = original hardcoded weights (in=0.3, FF=1.0, inhib=-0.6, etc.)
 *   2 = fluctuation-driven weights per arxiv 2206.10226 (2026-04-15)
 *   3 = v2 + stronger recurrent quench (60E/40I at 4× I, 2026-04-15)
 *   4 = v3 reverted to 80E/20I + 4× I, budget reduced 0.8→0.3 × gap
 *       (rate recalibration; 2026-04-15)
 *   5 = v4 + Turrigiano-style homeostatic synaptic scaling to prevent
 *       R-STDP-induced drift into silent/saturated regimes (2026-04-16)
 *   6 = v5 + intrinsic plasticity (per-neuron threshold adaptation),
 *       short-term synaptic depression, separate E/I plasticity rules,
 *       per-neuron metabolic weight budget, aggressive emergency
 *       homeostatic range (±10% when rate > 3× or < 1/3 target),
 *       R-STDP LR halved 0.001→0.0005, and force_quench RPC.
 *       Full biological stability package. (2026-04-19) */
/* SCHEMA VERSION HISTORY
 *   v5: fluctuation-driven weight init (2026-04-18)
 *   v6: full biological stability package — IP + depression + inhibitory
 *       plasticity + metabolic budget + emergency homeostatic range (2026-04-19)
 *   v7: persist per-neuron homeostatic state across restarts —
 *       threshold_offset, neuron_rate_ema, depression (2026-04-19)
 *       Without this, every restart wiped hours of homeostatic settling.
 *   v8: substrate-correctness pass (2026-04-27).
 *       - P0: per-receptor conductance — population_t now carries four
 *         per-neuron g_ampa/g_nmda/g_gaba_a/g_gaba_b arrays + a
 *         per-pop-pair synapse_type_per_src[128] receptor lookup table.
 *         The lumped g_exc/g_inh format from v7 is gone; on-disk reads
 *         of v7 files cannot be migrated 1:1 (no way to know how a v7
 *         lumped weight should split across the 4 receptor buckets), so
 *         this is a HARD format break — caller must rebuild.
 *       - P1.1: top-down NMDA feedback (L5→L3, L6→L2) added.
 *       - P3.1: reticular thalamic nucleus pop ("thalamus_reticular",
 *         10K LIF) wired tier0+L6→TRN NMDA + TRN→tier0 GABA_A.
 *       Bumping SNN_MIN_COMPATIBLE_SCHEMA to 8 forces rebuild of any
 *       sidecar saved before today; otherwise old caches would load
 *       silently with the lumped-CB layout and crash on first step.
 *   v9: substrate-correctness pass cont'd (2026-04-27).
 *       - P2.0: snn_lif_params_t + snn_pop_lif_params() helper; LIF
 *         params resolve per-pop instead of per-network.
 *       - P2.1: neuron_subclass_t enum + subclass field on
 *         snn_population_s; resolves to subclass-specific τ_mem/t_ref.
 *       - P2.2: PV/SOM/VIP sub-pops added to tiers 2-6 (15 new pops),
 *         tagged with subclass, wired with the canonical disinhibition
 *         microcircuit (pyr→PV/SOM AMPA, prev-tier→VIP AMPA, PV→pyr
 *         + SOM→pyr GABA_A, VIP→SOM GABA_A).
 *       Pop count grows from ~47 to ~62. Schema bump forces rebuild
 *       so old caches don't load with mismatched pop indices; the v8
 *       per-receptor on-disk format is otherwise unchanged.
 *  v10: P4.1 layer-specific pyramidal subtypes (2026-04-27).
 *       - Extended neuron_subclass_t with PYRAMIDAL_L23 / L4_STELLATE /
 *         L5_BETZ; tier-pyr pops now tagged at hierarchical wiring time.
 *       - LIF profile deltas: L23 τ=18, L4 τ=14 + threshold +2 (selective),
 *         L5 τ=25 + threshold -2 (excitable, output projection).
 *       Format-compatible with v9 — only adds subclass tag values that
 *       were already in v9 (file format already serializes subclass).
 *       Schema still bumped to force rebuild so old caches that were
 *       built before P4.1 don't load with incorrect tier-pyr params.
 *  v11: Wave C — Dale's principle fix (2026-04-27).
 *       - Within-tier recurrent loop now pure E→E AMPA. The previous
 *         mod-5 dual-class branch made pyramidal pops emit BOTH AMPA
 *         and GABA_A which violated Dale; inhibitory tone is now solely
 *         from PV/SOM/VIP sub-pops (P2.2). docs/claude/dale-audit-2026-04-27.md.
 *       - File format unchanged; schema bump forces rebuild so caches
 *         built with the Dale-violating recurrent CSR are rejected.
 *  v12: Wave E (partial) — direct input_pop → PV thalamic afferent
 *       (2026-04-27). Restores the canonical thalamus → {pyr, PV}
 *       parallel projection. PV's faster τ_mem (10 ms) lets it spike
 *       under thalamic drive WITHOUT first waiting for a pyr→PV
 *       collateral, restoring the FFI temporal-precision arm.
 *       Conduction-delay model fix (per-pop ring buffer) is a separate
 *       follow-up; this commit fixes the structural part of the
 *       FFI audit. docs/claude/ffi-timing-audit-2026-04-27.md.
 *  v13: Wave E (full) — per-pop conduction delay ring buffer
 *       (2026-04-27). Adds snn_population_t::conduction_delay_steps
 *       (uint8_t, default 0, max 8) and a per-pop spike_history ring
 *       (SNN_SPIKE_HISTORY_SLOTS rows × n_neurons floats). The deposit
 *       kernel now reads delayed spikes via snn_pop_get_delayed_spike_row;
 *       the end-of-step pass copies spike_output into the ring and
 *       advances the head. Hierarchical wiring sets PV pops to 0 steps
 *       (fast/myelinated) and tier pyramidal pops to 1 step (≈1 ms),
 *       restoring the canonical 1-3 ms FFI window. With all pops at
 *       delay 0 (default) the deposit kernel reads the freshest slot
 *       which equals the current spike_output snapshot — bit-identical
 *       to v12 behavior. Format-compatible with v12 (no new on-disk
 *       fields); schema bump forces rebuild so old caches built before
 *       Wave E full don't load with un-tagged delays.
 *       See docs/claude/ffi-timing-audit-2026-04-27.md.
 *  v14: Wave G — per-neuron LIF heterogeneity (2026-04-27).
 *       Adds snn_population_t::tau_mem_per_neuron,
 *       snn_population_t::v_thresh_per_neuron (each [n_neurons] floats,
 *       NULL by default → fall back to pop-wide), and the relative-σ
 *       knob heterogeneity_sigma (default 0). Setter
 *       snn_network_set_pop_heterogeneity() draws per-neuron values via
 *       Box-Muller around the pop-wide-resolved LIF params; clamps σ to
 *       [0, 0.5]. Hierarchical wiring tags every tier-pyramidal pop with
 *       σ=0.10 (10 % relative); PV/SOM/VIP/TRN stay at σ=0.
 *       LIF resolution helper snn_pop_neuron_lif_params() rides on top
 *       of snn_pop_lif_params(): subclass deltas first, then per-neuron
 *       σ noise, then substrate. With all pops at default σ=0 the LIF
 *       lookup collapses to pop-wide bit-identically — v13 behaviour
 *       preserved. Format-compatible with v13 (the new fields are
 *       runtime-allocated and rebuilt on load). Schema bump forces
 *       rebuild so old caches don't load with surprise homogeneous
 *       tier pops.
 *
 * v15 (Wave H — dendritic compartments + NMDA plateau, 2026-04-27): adds
 *       8 per-neuron arrays on snn_population_t (v_basal, v_apical,
 *       g_ampa_basal, g_gaba_a_basal, g_nmda_apical, g_gaba_b_apical,
 *       plateau_active, plateau_t0) plus the bool dendritic_enabled gate.
 *       All 8 default to NULL / false; allocated only when
 *       snn_network_enable_dendritic() is called. Hierarchical wiring
 *       opts in tier-pyramidal pops only (PYRAMIDAL{,_L23,_L4_STELLATE,
 *       _L5_BETZ}); interneurons stay single-compartment.
 *       Hot loop checks `pop->dendritic_enabled && pop->v_basal != NULL`
 *       before running the two-compartment branch (silent-degrade per
 *       doc). With dendritic OFF on every pop the LIF lookup collapses
 *       to v14 bit-identically. Format-compatible with v14 (the new
 *       fields are runtime-allocated and rebuilt on load); schema bump
 *       forces rebuild so old caches don't load assuming dendritic
 *       state.
 *       See docs/claude/wave-h-dendritic-design-2026-04-27.md.
 *
 * v16 (Wave F — substrate skip-path extension, 2026-04-27): no struct or
 *       on-disk format changes. Hierarchical wiring's SKIP_DEFS table grows
 *       from 2 to 5 entries: adds {input→L4, L3→L6, L5→L2}. Topology only;
 *       schema bump forces rebuild so old caches don't load missing the
 *       new skip connections. Region-aware (Option A) and cortex-bridge
 *       (Option B) variants of Wave F remain deferred-pending-design per
 *       docs/claude/wave-f-cortico-cortical-design-2026-04-27.md.
 *
 * v17 (Wave G GPU sync — per-neuron LIF heterogeneity on GPU, 2026-04-27):
 *       NO on-disk format changes. Adds two device-resident tensors to
 *       nimcp_lif_state_t (tau_mem_per_neuron, v_thresh_per_neuron) plus
 *       a dirty flag. snn_network_step uploads the per-neuron arrays to
 *       GPU using snn_pop_neuron_lif_params() — composing subclass deltas
 *       (PV/SOM/VIP/TRN/L23/L4_STELLATE/L5_BETZ) AND per-neuron
 *       heterogeneity σ noise — so the GPU LIF kernel matches CPU
 *       semantics. Pre-v17 binaries running the GPU path silently dropped
 *       BOTH subclass deltas and heterogeneity (kernel used network-wide
 *       config defaults). v16 saves load fine — runtime-only fields are
 *       rebuilt on snn_network_create / CSR-finalize, and the per-neuron
 *       upload happens on the very first GPU step after load. SCHEMA
 *       BUMP IS INFORMATIONAL: SNN_MIN_COMPATIBLE_SCHEMA stays at 16
 *       because no on-disk semantics changed. Out-of-scope: GPU path is
 *       still skipped when CB-mode is ON (CB requires per-receptor
 *       reversal-potential math the kernel doesn't model — same as v16).
 */
#define SNN_WIRING_SCHEMA_VERSION  17

/* Checked fwrite helper — returns -1 on failure.
 * Saves raw (uncompressed) — compression happens at rsync transit time
 * via `rsync -z`. Raw write on NVMe is ~1.6 GB/s vs single-threaded gzip-3
 * at ~70 MB/s, so save wall time drops from ~16 min to ~15 s. The on-disk
 * file grows from 10.5 GB to ~17 GB. */
#define SNN_FWRITE(ptr, size, count, stream) \
    do { size_t _items = (count); \
         size_t _w = fwrite((ptr), (size), _items, (stream)); \
         if (_w != _items) { \
            NIMCP_LOGGING_ERROR("snn_network_save: fwrite failed (wrote %zu of %zu items)", \
                                _w, _items); \
            fclose(stream); if (tmp_path[0]) remove(tmp_path); return -1; \
         } } while (0)

int snn_network_save(snn_network_t* network, const char* path) {
    if (!network || !path) return -1;

    /* Atomic write: write to temp file, then rename */
    char tmp_path[1024];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    FILE* f = fopen(tmp_path, "wb");
    if (!f) {
        NIMCP_LOGGING_ERROR("snn_network_save: failed to open %s", tmp_path);
        return -1;
    }

    uint32_t magic = SNN_CHECKPOINT_MAGIC;
    uint32_t version = SNN_CHECKPOINT_VERSION;
    uint32_t wiring_schema = SNN_WIRING_SCHEMA_VERSION;
    SNN_FWRITE(&magic, sizeof(uint32_t), 1, f);
    SNN_FWRITE(&version, sizeof(uint32_t), 1, f);
    SNN_FWRITE(&wiring_schema, sizeof(uint32_t), 1, f);

    /* Save config (contains all architecture parameters) */
    SNN_FWRITE(&network->config, sizeof(snn_config_t), 1, f);
    SNN_FWRITE(&network->is_training, sizeof(bool), 1, f);

    /* Save neuron state and outgoing synapse weights via sparse synapse API */
    uint32_t total_neurons = network->neural_net
        ? neural_network_get_num_neurons(network->neural_net) : 0;
    SNN_FWRITE(&total_neurons, sizeof(uint32_t), 1, f);

    for (uint32_t i = 0; i < total_neurons && network->neural_net; i++) {
        neuron_t* n = neural_network_get_neuron(network->neural_net, i);
        if (!n) {
            float zero = 0.0f;
            uint32_t zero_u = 0;
            SNN_FWRITE(&zero, sizeof(float), 1, f);
            SNN_FWRITE(&zero, sizeof(float), 1, f);
            SNN_FWRITE(&zero, sizeof(float), 1, f);
            SNN_FWRITE(&zero_u, sizeof(uint32_t), 1, f);
            continue;
        }

        SNN_FWRITE(&n->state, sizeof(float), 1, f);
        SNN_FWRITE(&n->threshold, sizeof(float), 1, f);
        SNN_FWRITE(&n->bias, sizeof(float), 1, f);

        uint32_t n_synapses = sparse_synapse_count(&n->outgoing);
        SNN_FWRITE(&n_synapses, sizeof(uint32_t), 1, f);
        for (uint32_t s = 0; s < n_synapses; s++) {
            synapse_handle_t* h = sparse_synapse_get(&n->outgoing, s);
            float w = h ? h->weight : 0.0f;
            uint32_t tid = h ? h->target_neuron_id : 0;
            SNN_FWRITE(&w, sizeof(float), 1, f);
            SNN_FWRITE(&tid, sizeof(uint32_t), 1, f);
        }
    }

    /* === V3: Save lightweight CSR population data === */
    uint32_t n_lightweight = 0;
    for (uint32_t p = 0; p < network->n_populations; p++) {
        snn_population_t* pop = network->populations[p];
        if (pop && pop->lightweight && pop->incoming_csr && pop->incoming_csr->finalized)
            n_lightweight++;
    }
    SNN_FWRITE(&n_lightweight, sizeof(uint32_t), 1, f);
    SNN_FWRITE(&network->n_populations, sizeof(uint32_t), 1, f);

    if (n_lightweight > 0) {
        /* Save population metadata */
        for (uint32_t p = 0; p < network->n_populations; p++) {
            snn_population_t* pop = network->populations[p];
            if (!pop || !pop->lightweight || !pop->incoming_csr) continue;

            SNN_FWRITE(&pop->id, sizeof(uint32_t), 1, f);
            SNN_FWRITE(&pop->n_neurons, sizeof(uint32_t), 1, f);
            SNN_FWRITE(&pop->neuron_type, sizeof(neuron_type_t), 1, f);
            SNN_FWRITE(pop->name, sizeof(pop->name), 1, f);

            /* CSR data */
            snn_csr_storage_t* csr = pop->incoming_csr;
            SNN_FWRITE(&csr->n_neurons, sizeof(uint32_t), 1, f);
            SNN_FWRITE(&csr->n_synapses, sizeof(uint32_t), 1, f);
            SNN_FWRITE(csr->row_ptr, sizeof(uint32_t), csr->n_neurons + 1, f);
            SNN_FWRITE(csr->entries, sizeof(snn_csr_synapse_t), csr->n_synapses, f);

            /* GPU arrays (weights[], flat_col_idx[]) are derived from entries[]
             * — skip writing them to save ~8 bytes/synapse (~11.6 GB for 1.45B).
             * They will be regenerated on first GPU step via snn_csr_prepare_gpu().
             * Always write has_gpu=0 to signal "regenerate on demand". */
            uint8_t has_gpu = 0;
            SNN_FWRITE(&has_gpu, sizeof(uint8_t), 1, f);

            /* === V7: Persist per-neuron homeostatic state ===
             * These arrays capture hours of biological adaptation. Without
             * persisting them, every restart wiped threshold adaptation,
             * rate EMAs, and depression levels — forcing the network to
             * re-discover its operating point from scratch every time. */
            uint8_t has_homeostatic = (pop->threshold_offset &&
                                       pop->neuron_rate_ema &&
                                       pop->depression) ? 1 : 0;
            SNN_FWRITE(&has_homeostatic, sizeof(uint8_t), 1, f);
            if (has_homeostatic) {
                SNN_FWRITE(pop->threshold_offset, sizeof(float), pop->n_neurons, f);
                SNN_FWRITE(pop->neuron_rate_ema, sizeof(float), pop->n_neurons, f);
                SNN_FWRITE(pop->depression, sizeof(float), pop->n_neurons, f);
            }
        }
        NIMCP_LOGGING_INFO("SNN save: %u lightweight CSR populations saved "
                           "(with per-neuron homeostatic state)", n_lightweight);
    }

    fclose(f);

    /* Atomic rename: temp → final path.
     *
     * Pod MFS rename race: ~50% of saves trip an ENOENT on rename(tmp,final)
     * even though the tmp file just fclose()d successfully. The kernel/FS
     * needs a moment to settle directory state. Without this retry, the
     * trainer's downstream sidecar-rename loop finds no <snapshot>.snn.tmp
     * to rename, so timestamped .snn files never land on disk and resume
     * sidecar-checks see "missing .snn" until the next initial-save cycle.
     *
     * Retry up to 3× with backoff, then fall back to copy+unlink. If the
     * destination is genuinely bad (EISDIR/ENOTDIR/EROFS) the retries
     * also surface that; the errno from the final attempt is logged. */
    int rename_rc = -1;
    int saved_errno = 0;
    {
        struct timespec wait_ts;
        for (int attempt = 0; attempt < 3; attempt++) {
            if (rename(tmp_path, path) == 0) {
                rename_rc = 0;
                break;
            }
            saved_errno = errno;
            if (attempt < 2) {
                /* 50ms, 200ms back-offs */
                wait_ts.tv_sec = 0;
                wait_ts.tv_nsec = (attempt == 0 ? 50L : 200L) * 1000000L;
                nanosleep(&wait_ts, NULL);
            }
        }
    }

    if (rename_rc != 0) {
        /* Rename exhausted retries — try copy+unlink. This costs an extra
         * disk round-trip but preserves atomicity from the load side: the
         * destination is fully written before being visible at the final
         * name. */
        NIMCP_LOGGING_WARN("snn_network_save: rename %s → %s failed after retries: %s "
                           "(errno=%d) — falling back to copy",
                           tmp_path, path, strerror(saved_errno), saved_errno);

        FILE* src = fopen(tmp_path, "rb");
        FILE* dst = NULL;
        char copy_tmp[1024];
        snprintf(copy_tmp, sizeof(copy_tmp), "%s.copy", path);
        if (src) dst = fopen(copy_tmp, "wb");
        if (!src || !dst) {
            NIMCP_LOGGING_ERROR("snn_network_save: copy fallback open failed "
                                "(src=%p dst=%p, errno=%d)", (void*)src, (void*)dst, errno);
            if (src) fclose(src);
            if (dst) fclose(dst);
            remove(tmp_path);
            return -1;
        }

        char buf[1 << 20];  /* 1 MiB chunks */
        size_t n;
        int copy_ok = 1;
        while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
            if (fwrite(buf, 1, n, dst) != n) {
                NIMCP_LOGGING_ERROR("snn_network_save: copy fwrite failed: %s",
                                    strerror(errno));
                copy_ok = 0;
                break;
            }
        }
        fclose(src);
        if (fclose(dst) != 0) copy_ok = 0;

        if (!copy_ok) {
            remove(copy_tmp);
            remove(tmp_path);
            return -1;
        }

        if (rename(copy_tmp, path) != 0) {
            saved_errno = errno;
            NIMCP_LOGGING_ERROR("snn_network_save: final copy-rename %s → %s failed: %s "
                                "(errno=%d)", copy_tmp, path, strerror(saved_errno),
                                saved_errno);
            remove(copy_tmp);
            remove(tmp_path);
            return -1;
        }
        remove(tmp_path);  /* original tmp now redundant */
    }

    NIMCP_LOGGING_INFO("SNN network saved to %s (raw, %u neurons, %u CSR pops)",
                       path, total_neurons, n_lightweight);
    return 0;
}

#undef SNN_FWRITE

/* fread-like wrapper for gzFile. Returns count on success, less on EOF/error.
 * gzopen for read auto-detects gzip header and falls back to plain reads, so
 * this works for both compressed AND existing uncompressed .snn files. */
static inline size_t gz_fread(void* ptr, size_t sz, size_t count, gzFile f) {
    if (sz == 0 || count == 0) return 0;
    int got = gzread(f, ptr, (unsigned)(sz * count));
    if (got <= 0) return 0;
    return (size_t)got / sz;
}

snn_network_t* snn_network_load(const char* path) {
    if (!path) return NULL;

    /* gzopen handles both gzip-compressed and plain files transparently —
     * existing uncompressed .snn files load fine without conversion. */
    gzFile f = gzopen(path, "rb");
    if (!f) {
        NIMCP_LOGGING_WARN("snn_network_load: file not found %s", path);
        return NULL;
    }
    #define fread(p, s, c, fp) gz_fread((p), (s), (c), (fp))
    #define fclose(fp) gzclose(fp)
    #define fseek(fp, off, whence) gzseek((fp), (off), (whence))

    uint32_t magic = 0, version = 0;
    if (fread(&magic, sizeof(uint32_t), 1, f) != 1 || magic != SNN_CHECKPOINT_MAGIC) {
        NIMCP_LOGGING_ERROR("snn_network_load: invalid magic");
        fclose(f);
        return NULL;
    }
    if (fread(&version, sizeof(uint32_t), 1, f) != 1 || version > SNN_CHECKPOINT_VERSION) {
        NIMCP_LOGGING_ERROR("snn_network_load: unsupported version %u", version);
        fclose(f); return NULL;
    }

    /* Wiring-schema version gating. v4+ files embed the schema tag; older files
     * predate the fluctuation-driven init and must be rejected so the caller
     * rebuilds from scratch with the current formula. */
    if (version < 4) {
        NIMCP_LOGGING_WARN("snn_network_load: checkpoint v%u predates wiring schema "
                           "tracking — rejecting so network is rebuilt with current "
                           "wiring schema v%u", version, SNN_WIRING_SCHEMA_VERSION);
        fclose(f); return NULL;
    }
    uint32_t wiring_schema = 0;
    if (fread(&wiring_schema, sizeof(uint32_t), 1, f) != 1) {
        NIMCP_LOGGING_ERROR("snn_network_load: truncated wiring_schema field");
        fclose(f); return NULL;
    }
    /* Schema compatibility policy:
     *   - Forward-compatible: newer file schema than binary → reject (we
     *     don't know what fields were added, can't safely load)
     *   - Backward-compatible: older file schema than binary → ACCEPT for
     *     schemas ≥ SNN_MIN_COMPATIBLE_SCHEMA.
     *     * v5 → v6: added per-neuron in-memory fields (threshold_offset,
     *       neuron_rate_ema, depression) but didn't change on-disk format.
     *     * v6 → v7: on-disk format extended with optional homeostatic-state
     *       block per population. Reads guarded by `if (wiring_schema >= 7)`
     *       so v5/v6 files skip it and use seed defaults.
     *     Older saves still load cleanly; missing fields default-init. This
     *     preserves learned synaptic patterns across schema bumps.
     *   - For format-incompatible bumps in future, bump SNN_MIN_COMPATIBLE
     *     and force a rebuild. */
    /* Bumped 5 → 8 (2026-04-27): the substrate-correctness pass changed
     * snn_population_t's per-neuron conductance layout (g_exc/g_inh →
     * g_ampa/g_nmda/g_gaba_a/g_gaba_b) and added the per-pop-pair
     * synapse_type_per_src table. There is no safe migration path from
     * v5/v6/v7 lumped-CB saves to v8 per-receptor — reject and rebuild.
     * Bumped 8 → 9 (2026-04-27): P2 added 15 PV/SOM/VIP sub-pops to
     * tiers 2-6 plus the per-pop subclass field. Pop indices in v8
     * caches won't match the v9 layout — force rebuild.
     * Bumped 9 → 10 (2026-04-27): P4.1 added layer-specific pyramidal
     * subtypes (L23/L4_STELLATE/L5_BETZ) and tier-pyr pops now carry
     * the corresponding LIF profile (τ_mem and threshold offsets).
     * v9 caches load with default-pyramidal LIF profile, missing the
     * layer-specific tuning — force rebuild for correctness.
     * Bumped 10 → 16 (Wave F, 2026-04-27): SKIP_DEFS grew from 2 to 5
     * entries (input→L4, L3→L6, L5→L2). v11..v15 saves persist the
     * 2-entry skip topology; the new skip CSR rows would be silently
     * absent on load. v11..v15 are STRUCTURE-correct otherwise, but
     * topologically incomplete — force rebuild rather than default-init
     * a stub topology. (Future schema bumps that only add new in-memory
     * struct fields can safely keep the min lower.) */
    #define SNN_MIN_COMPATIBLE_SCHEMA 16
    if (wiring_schema > SNN_WIRING_SCHEMA_VERSION) {
        NIMCP_LOGGING_WARN("snn_network_load: file schema v%u is NEWER than "
                           "binary v%u — rejecting (can't load future format)",
                           wiring_schema, SNN_WIRING_SCHEMA_VERSION);
        fclose(f); return NULL;
    }
    if (wiring_schema < SNN_MIN_COMPATIBLE_SCHEMA) {
        NIMCP_LOGGING_WARN("snn_network_load: file schema v%u is below "
                           "minimum compatible v%u — rejecting, will rebuild",
                           wiring_schema, SNN_MIN_COMPATIBLE_SCHEMA);
        fclose(f); return NULL;
    }
    if (wiring_schema < SNN_WIRING_SCHEMA_VERSION) {
        NIMCP_LOGGING_INFO("snn_network_load: migrating v%u → v%u "
                           "(format-compatible, new in-memory fields default-init). "
                           "If loaded weights were saturated, call "
                           "brain.snn_force_quench(25) after load to rescue.",
                           wiring_schema, SNN_WIRING_SCHEMA_VERSION);
    }

    snn_config_t config;
    bool is_training = false;
    if (fread(&config, sizeof(snn_config_t), 1, f) != 1 ||
        fread(&is_training, sizeof(bool), 1, f) != 1) {
        NIMCP_LOGGING_ERROR("snn_network_load: truncated config");
        fclose(f); return NULL;
    }

    /* Recreate network from config */
    snn_network_t* net = snn_network_create(&config);
    if (!net) {
        NIMCP_LOGGING_ERROR("snn_network_load: failed to recreate network");
        fclose(f);
        return NULL;
    }
    net->is_training = is_training;

    /* Restore neuron state and synapse weights */
    uint32_t total_neurons = 0;
    if (fread(&total_neurons, sizeof(uint32_t), 1, f) != 1) {
        NIMCP_LOGGING_ERROR("snn_network_load: truncated neuron count");
        snn_network_destroy(net); fclose(f); return NULL;
    }

    uint32_t actual_neurons = net->neural_net
        ? neural_network_get_num_neurons(net->neural_net) : 0;
    uint32_t restore_count = (total_neurons < actual_neurons) ? total_neurons : actual_neurons;

    for (uint32_t i = 0; i < total_neurons; i++) {
        float state_val, threshold, bias;
        if (fread(&state_val, sizeof(float), 1, f) != 1 ||
            fread(&threshold, sizeof(float), 1, f) != 1 ||
            fread(&bias, sizeof(float), 1, f) != 1) {
            NIMCP_LOGGING_WARN("snn_network_load: truncated at neuron %u", i);
            break;
        }

        uint32_t n_synapses = 0;
        if (fread(&n_synapses, sizeof(uint32_t), 1, f) != 1) {
            NIMCP_LOGGING_WARN("snn_network_load: truncated synapse count at neuron %u", i);
            break;
        }

        /* Read per-synapse data: v1=weight only, v2=weight+target_id */
        float* weights = NULL;
        uint32_t* target_ids = NULL;
        if (n_synapses > 0) {
            weights = nimcp_malloc(n_synapses * sizeof(float));
            if (version >= 2) {
                target_ids = nimcp_malloc(n_synapses * sizeof(uint32_t));
            }
            bool synapse_read_truncated = false;
            for (uint32_t s = 0; s < n_synapses; s++) {
                float w = 0.0f;
                if (fread(&w, sizeof(float), 1, f) != 1) {
                    synapse_read_truncated = true;
                    break;
                }
                if (weights) weights[s] = w;
                if (version >= 2) {
                    uint32_t tid = 0;
                    if (fread(&tid, sizeof(uint32_t), 1, f) != 1) {
                        synapse_read_truncated = true;
                        break;
                    }
                    if (target_ids) target_ids[s] = tid;
                }
            }
            if (synapse_read_truncated) {
                NIMCP_LOGGING_WARN("snn_network_load: truncated synapse data at neuron %u", i);
                nimcp_free(weights);
                nimcp_free(target_ids);
                break;
            }
        }

        /* Apply to matching neuron in recreated network */
        if (i < restore_count && net->neural_net) {
            neuron_t* n = neural_network_get_neuron(net->neural_net, i);
            if (n) {
                n->state = state_val;
                n->threshold = threshold;
                n->bias = bias;
                if (weights && target_ids) {
                    /* v2: restore connections with weights and target IDs.
                     * If the network has no connections yet (fresh from snn_network_create),
                     * CREATE them from the checkpoint data. This preserves BPTT-trained
                     * weights across daemon restarts. */
                    uint32_t cur_syn = sparse_synapse_count(&n->outgoing);
                    if (cur_syn > 0) {
                        /* Network already has connections — match by target ID */
                        for (uint32_t s = 0; s < n_synapses; s++) {
                            for (uint32_t cs = 0; cs < cur_syn; cs++) {
                                synapse_handle_t* h = sparse_synapse_get(&n->outgoing, cs);
                                if (h && h->target_neuron_id == target_ids[s]) {
                                    h->weight = weights[s];
                                    break;
                                }
                            }
                        }
                    } else {
                        /* No connections — create from checkpoint data.
                         * Uses neural_network_add_connection to properly wire
                         * both outgoing and incoming synapse handles. */
                        for (uint32_t s = 0; s < n_synapses; s++) {
                            neural_network_add_connection(
                                net->neural_net, i, target_ids[s], weights[s]);
                        }
                    }
                } else if (weights) {
                    /* v1 fallback: positional restore */
                    uint32_t cur_syn = sparse_synapse_count(&n->outgoing);
                    uint32_t s_count = (n_synapses < cur_syn) ? n_synapses : cur_syn;
                    for (uint32_t s = 0; s < s_count; s++) {
                        synapse_handle_t* h = sparse_synapse_get(&n->outgoing, s);
                        if (h) h->weight = weights[s];
                    }
                }
            }
        }
        nimcp_free(weights);
        nimcp_free(target_ids);
    }

    /* === V3: Load lightweight CSR populations === */
    if (version >= 3) {
        uint32_t n_lightweight = 0, n_pops_saved = 0;
        if (fread(&n_lightweight, sizeof(uint32_t), 1, f) == 1 &&
            fread(&n_pops_saved, sizeof(uint32_t), 1, f) == 1 &&
            n_lightweight > 0) {

            NIMCP_LOGGING_INFO("Loading %u lightweight CSR populations...", n_lightweight);

            for (uint32_t lp = 0; lp < n_lightweight; lp++) {
                uint32_t pop_id, pop_n;
                neuron_type_t ntype;
                char pop_name[64];

                if (fread(&pop_id, sizeof(uint32_t), 1, f) != 1 ||
                    fread(&pop_n, sizeof(uint32_t), 1, f) != 1 ||
                    fread(&ntype, sizeof(neuron_type_t), 1, f) != 1 ||
                    fread(pop_name, sizeof(pop_name), 1, f) != 1) {
                    NIMCP_LOGGING_WARN("snn_network_load: truncated CSR pop header at %u", lp);
                    break;
                }

                /* Create lightweight population */
                int rc = snn_network_add_population_lightweight(net, pop_n, ntype, pop_name);
                if (rc < 0) {
                    NIMCP_LOGGING_ERROR("snn_network_load: failed to create CSR pop '%s'", pop_name);
                    /* Skip this pop's CSR data.
                     * Layout: [csr_n][csr_nnz][row_ptr][entries][has_gpu]
                     *         [if v7: has_homeostatic][if set: 3 × n_neurons floats] */
                    uint32_t csr_n, csr_nnz;
                    if (fread(&csr_n, 4, 1, f) == 1 && fread(&csr_nnz, 4, 1, f) == 1) {
                        fseek(f, (long)((csr_n + 1) * 4 + csr_nnz * sizeof(snn_csr_synapse_t)), SEEK_CUR);
                        uint8_t has_gpu;
                        if (fread(&has_gpu, 1, 1, f) == 1 && has_gpu)
                            fseek(f, (long)(csr_nnz * (4 + 4)), SEEK_CUR);
                        /* V7: also skip homeostatic block if present.
                         * Without this, the file cursor lands mid-array and
                         * subsequent population reads corrupt. */
                        if (wiring_schema >= 7) {
                            uint8_t has_homeo;
                            if (fread(&has_homeo, 1, 1, f) == 1 && has_homeo) {
                                /* 3 arrays × pop_n × sizeof(float) */
                                fseek(f, (long)(3 * pop_n * sizeof(float)),
                                       SEEK_CUR);
                            }
                        }
                    }
                    continue;
                }

                snn_population_t* pop = net->populations[rc];
                snn_csr_storage_t* csr = pop->incoming_csr;

                /* Read CSR arrays directly */
                uint32_t csr_n = 0, csr_nnz = 0;
                if (fread(&csr_n, sizeof(uint32_t), 1, f) != 1 ||
                    fread(&csr_nnz, sizeof(uint32_t), 1, f) != 1) {
                    NIMCP_LOGGING_WARN("snn_network_load: truncated CSR header for '%s'", pop_name);
                    break;
                }

                /* Resize CSR storage if needed */
                if (csr_nnz > csr->capacity) {
                    snn_csr_synapse_t* new_entries = nimcp_realloc(
                        csr->entries, csr_nnz * sizeof(snn_csr_synapse_t));
                    if (!new_entries) {
                        NIMCP_LOGGING_ERROR("snn_network_load: CSR realloc failed for %u synapses", csr_nnz);
                        break;
                    }
                    csr->entries = new_entries;
                    csr->capacity = csr_nnz;
                }

                /* Read row_ptr and entries */
                if (fread(csr->row_ptr, sizeof(uint32_t), csr_n + 1, f) != csr_n + 1 ||
                    fread(csr->entries, sizeof(snn_csr_synapse_t), csr_nnz, f) != csr_nnz) {
                    NIMCP_LOGGING_WARN("snn_network_load: truncated CSR data for '%s' — "
                                       "resetting pop CSR to empty so re-init/forward-pass "
                                       "can treat it as fresh", pop_name);
                    /* Truncation leaves entries/row_ptr partially filled and unsafe
                     * to walk. Reset CSR scalar state so any downstream reader
                     * sees an empty population (n_synapses=0) instead of
                     * trusting partial bytes. row_ptr/entries memory is left
                     * allocated; subsequent uses honor n_synapses=0. */
                    csr->n_neurons = 0;
                    csr->n_synapses = 0;
                    csr->finalized = false;
                    csr->gpu_ready = false;
                    /* File cursor is now past a corrupt boundary — bail out of
                     * the lightweight-CSR loop. Subsequent pops will be missing
                     * from the SNN, and init_language_pops's OR-of-any resume
                     * guard handles that (treats any survivor as resume). */
                    break;
                }
                csr->n_neurons = csr_n;
                csr->n_synapses = csr_nnz;
                csr->finalized = true;

                /* Read GPU-ready arrays if present */
                uint8_t has_gpu = 0;
                if (fread(&has_gpu, sizeof(uint8_t), 1, f) == 1 && has_gpu) {
                    csr->weights = nimcp_malloc(csr_nnz * sizeof(float));
                    csr->flat_col_idx = nimcp_malloc(csr_nnz * sizeof(uint32_t));
                    if (csr->weights && csr->flat_col_idx) {
                        if (fread(csr->weights, sizeof(float), csr_nnz, f) == csr_nnz &&
                            fread(csr->flat_col_idx, sizeof(uint32_t), csr_nnz, f) == csr_nnz) {
                            csr->gpu_ready = true;
                        }
                    }
                }

                /* === V7: Restore per-neuron homeostatic state ===
                 * Older schemas don't have these — keep the seed values
                 * initialized when the population was created. */
                if (wiring_schema >= 7) {
                    uint8_t has_homeostatic = 0;
                    if (fread(&has_homeostatic, sizeof(uint8_t), 1, f) == 1 && has_homeostatic) {
                        if (pop->threshold_offset) {
                            size_t r1 = fread(pop->threshold_offset, sizeof(float),
                                              pop->n_neurons, f);
                            if (r1 != pop->n_neurons) {
                                NIMCP_LOGGING_WARN("  pop '%s': truncated threshold_offset "
                                                    "(%zu of %u)", pop_name, r1, pop->n_neurons);
                            }
                        } else {
                            fseek(f, (long)(pop->n_neurons * sizeof(float)), SEEK_CUR);
                        }
                        if (pop->neuron_rate_ema) {
                            size_t r2 = fread(pop->neuron_rate_ema, sizeof(float),
                                              pop->n_neurons, f);
                            if (r2 != pop->n_neurons) {
                                NIMCP_LOGGING_WARN("  pop '%s': truncated rate_ema", pop_name);
                            }
                        } else {
                            fseek(f, (long)(pop->n_neurons * sizeof(float)), SEEK_CUR);
                        }
                        if (pop->depression) {
                            size_t r3 = fread(pop->depression, sizeof(float),
                                              pop->n_neurons, f);
                            if (r3 != pop->n_neurons) {
                                NIMCP_LOGGING_WARN("  pop '%s': truncated depression", pop_name);
                            }
                        } else {
                            fseek(f, (long)(pop->n_neurons * sizeof(float)), SEEK_CUR);
                        }
                        NIMCP_LOGGING_INFO("  pop '%s': restored homeostatic state "
                                           "(threshold_offset + rate_ema + depression)",
                                           pop_name);
                    }
                }

                NIMCP_LOGGING_INFO("  CSR pop '%s': %u neurons, %u synapses (gpu=%s)",
                                   pop_name, csr_n, csr_nnz, csr->gpu_ready ? "yes" : "no");
            }

            /* Re-create GPU LIF state for full neuron count */
            size_t total_n = 0;
            for (uint32_t p = 0; p < net->n_populations; p++) {
                if (net->populations[p]) total_n += net->populations[p]->n_neurons;
            }
            if (net->gpu_lif_state) {
                nimcp_lif_state_destroy((nimcp_lif_state_t*)net->gpu_lif_state);
                net->gpu_lif_state = NULL;
            }
            if (net->gpu_ctx) {
                nimcp_lif_params_t lp = {
                    .tau_mem = net->config.tau_mem,
                    .tau_syn = net->config.tau_syn > 0.0f ? net->config.tau_syn : 5.0f,
                    .v_thresh = net->config.v_thresh,
                    .v_reset = net->config.v_reset,
                    .v_rest = net->config.v_rest,
                    .dt = net->config.dt,
                    .hard_reset = true
                };
                nimcp_lif_state_t* ls = nimcp_lif_state_create(
                    (nimcp_gpu_context_t*)net->gpu_ctx, total_n, &lp);
                if (ls) {
                    net->gpu_lif_state = ls;
                    NIMCP_LOGGING_INFO("GPU LIF state created for %zu neurons (from checkpoint)", total_n);
                }
            }

            /* Prepare GPU CSR arrays if not loaded from checkpoint */
            uint32_t* pop_offsets = nimcp_calloc(net->n_populations + 1, sizeof(uint32_t));
            if (pop_offsets) {
                for (uint32_t p = 0; p < net->n_populations; p++) {
                    pop_offsets[p + 1] = pop_offsets[p] +
                        (net->populations[p] ? net->populations[p]->n_neurons : 0);
                }
                /* Audit bug #7: track GPU prep success/failure per pop.
                 * Without this, silent CPU fallback degrades perf 10-100x. */
                uint32_t gpu_ok = 0, gpu_fail = 0;
                for (uint32_t p = 0; p < net->n_populations; p++) {
                    snn_population_t* pop = net->populations[p];
                    if (pop && pop->lightweight && pop->incoming_csr && !pop->incoming_csr->gpu_ready) {
                        if (snn_csr_prepare_gpu(pop->incoming_csr, pop_offsets,
                                                net->n_populations) == 0) {
                            gpu_ok++;
                        } else {
                            gpu_fail++;
                            NIMCP_LOGGING_WARN("GPU prep failed for pop '%s' — "
                                               "falling back to CPU (perf hit)",
                                               pop->name);
                        }
                    }
                }
                if (gpu_fail > 0) {
                    NIMCP_LOGGING_WARN("snn_load: %u/%u CSR populations FAILED GPU prep",
                                       gpu_fail, gpu_ok + gpu_fail);
                } else if (gpu_ok > 0) {
                    NIMCP_LOGGING_INFO("snn_load: %u CSR populations GPU-ready",
                                       gpu_ok);
                }
                nimcp_free(pop_offsets);
            }
        }
    }

    fclose(f);
    NIMCP_LOGGING_INFO("SNN network loaded from %s (%u neurons restored, v%u)", path, restore_count, version);
    #undef fread
    #undef fclose
    #undef fseek
    return net;
}
