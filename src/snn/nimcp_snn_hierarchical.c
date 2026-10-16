/**
 * @file nimcp_snn_hierarchical.c
 * @brief Hierarchical SNN creation for 1.8M-neuron primary brain
 *
 * WHAT: Creates a cortical-inspired population hierarchy with 46 populations
 *       across 8 tiers, from sensory input through conceptual processing to
 *       motor output.
 * WHY:  The SNN is the primary brain — 1.8M spiking neurons with local
 *       learning rules (R-STDP + dopamine modulation). This replaces the
 *       previous 768-neuron toy SNN.
 * HOW:  Builds populations tier-by-tier, then wires feedforward, recurrent,
 *       and skip connections between them using efficient direct-sampling
 *       connectivity (not O(n×m) all-pairs).
 *
 * Population Hierarchy:
 *   Input (4 pops, 80K)  → L1 (6 pops, 132K) → L2 (8 pops, 240K)
 *   → L3 (8 pops, 360K)  → L4 (6 pops, 348K) → L5 (6 pops, 264K)
 *   → L6 (4 pops, 120K)  → Output (4 pops, 256K)
 *   Total: 46 populations, 1,800,000 neurons
 *
 * @version 1.0
 * @date 2026-04-12
 */

#include "snn/nimcp_snn_network.h"
#include <sys/statvfs.h>  /* for disk-space guard before .snn cache write */
#include "snn/nimcp_snn_types.h"
#include "core/neuron_types/nimcp_neuron_types.h"
#include "core/synapse_types/nimcp_synapse_types.h"
#include "utils/logging/nimcp_logging.h"
#include <string.h>

#define LOG_MODULE "SNN_HIERARCHICAL"

/* Tier definitions: { n_populations, neurons_per_pop, name } */
typedef struct {
    uint32_t n_pops;
    uint32_t neurons_per_pop;
    const char* name;
    float ff_connectivity;    /* feedforward to next tier */
    float recurrent_connectivity; /* within-tier recurrent */
} snn_tier_def_t;

static const snn_tier_def_t TIER_DEFS[] = {
    /* Tier 0: Input — sensory encoding
     * 1.8M total — keeps 5+ GiB headroom under new pod 86 GiB cgroup.
     * Each inter-tier connection creates n_src × n_dst × connectivity synapses
     * PER population pair, summed across all src×dst pop combinations. */
    { 4, 20000, "input",      0.005f, 0.0f    },
    /* Tier 1: L1 — feature extraction */
    { 6, 22000, "L1_feature",  0.004f, 0.0f    },
    /* Tier 2: L2 — pattern binding */
    { 8, 30000, "L2_pattern",  0.003f, 0.0005f },
    /* Tier 3: L3 — conceptual (recurrent for working memory) */
    { 8, 45000, "L3_concept",  0.002f, 0.001f  },
    /* Tier 4: L4 — integration peak (recurrent for attractor dynamics) */
    { 6, 58000, "L4_integr",   0.002f, 0.001f  },
    /* Tier 5: L5 — executive/planning (recurrent) */
    { 6, 44000, "L5_exec",     0.003f, 0.001f  },
    /* Tier 6: L6 — output projection */
    { 4, 30000, "L6_project",  0.004f, 0.0f    },
    /* Tier 7: Output — motor/response */
    { 4, 64000, "output",      0.0f,  0.0f     },
};
#define NUM_TIERS (sizeof(TIER_DEFS) / sizeof(TIER_DEFS[0]))

/* Skip connection definitions: { src_tier, dst_tier, connectivity } */
typedef struct {
    uint32_t src_tier;
    uint32_t dst_tier;
    float connectivity;
} snn_skip_def_t;

/* Skip ("cortico-cortical-like") topology.
 *
 * Real cortex has long-range bypass projections that connect non-adjacent
 * areas with task-specific topography (V1→MT, V1→V2, A1→Broca-region,
 * frontal→occipital top-down). The 8-tier substrate has no concept of
 * "region," so we approximate by hand-picking tier pairs whose role
 * mismatch best resembles those projections.
 *
 * Wave F (deferred-by-design partial): true region-to-region routing
 * needs either Option A (per-pop region tags + region-aware SKIP_DEFS)
 * or Option B (cortex-module bridges). Both are scoped in
 * docs/claude/wave-f-cortico-cortical-design-2026-04-27.md but require
 * an architectural decision and ~4-8 hrs of work each.
 *
 * The substrate-level finish for Wave F: extend this table from 2 to
 * 5 entries. The added entries provide the topological diversity that
 * lets fast bottom-up sensory hits, slow concept→output projection,
 * and executive-bias top-down all coexist without going through the
 * full tier chain. Connectivity values are set lower than the original
 * 0.0005 because the ADDITIONAL paths fan a constant V_GAP/8 budget
 * across more synapses — keeping per-synapse drive comparable. */
static const snn_skip_def_t SKIP_DEFS[] = {
    { 1, 5, 0.0005f },  /* L1 → L5: fast sensory-to-executive shortcut */
    { 2, 6, 0.0005f },  /* L2 → L6: pattern-to-output projection */
    /* --- Wave F substrate extension (2026-04-27) --- */
    { 0, 4, 0.0003f },  /* input → L4_integr: thalamic-bypass to integration tier */
    { 3, 6, 0.0008f },  /* L3 → L6: concept → output direct projection */
    { 5, 2, 0.0002f },  /* L5 → L2: long-range top-down (executive bias) */
};
#define NUM_SKIPS (sizeof(SKIP_DEFS) / sizeof(SKIP_DEFS[0]))

/* Fluctuation-driven weight initialization for LIF SNNs.
 *
 * Step equation: dv = (v_rest - V + I_syn) / tau_mem * dt  →  V_ss = v_rest + I_syn.
 * To spike, I_syn must approach (v_thresh - v_rest) = gap = 15 mV.
 *
 * Given K converging source populations, each contributing one connection to
 * a destination neuron, we split a target I_syn budget of 0.8 × gap evenly:
 *     per_conn_budget = 0.8 * gap / K
 *     w_mean = per_conn_budget / (src_n * connectivity * presyn_rate)
 *
 * The 0.8 factor keeps E[V] below threshold so spikes come from fluctuations,
 * not saturation. presyn_rate=0.1 assumes steady-state ~10% firing per step
 * (input_pop is driven harder, ~0.5, handled separately).
 *
 * Reference: Rossbroich et al., arxiv 2206.10226 (fluctuation-driven init). */
#define SNN_V_GAP                 15.0f  /**< v_thresh - v_rest */
/* I_syn budget reduced from 0.8 → 0.3 → 0.15 × gap (quiet-start protocol).
 * Prior 0.3 was half-right: it halved init weights but the actual cascade
 * presyn_rate is 4× the formula's 0.1 assumption, so weights were still
 * ~1.5× too strong. That put the system on a knife-edge — small rate
 * deviations produced either silent collapse or hyperactive saturation,
 * and homeostasis had to scale DOWN against R-STDP's LTP pressure.
 *
 * With 0.15 × gap (2.25 mV), initial firing lands BELOW target 3%.
 * Homeostasis scales UP (easy direction; R-STDP and homeostasis agree
 * when the network is too quiet, since quiet → low LTP → no compounding).
 *
 * UPDATE: 0.15 was tuned for a shallower/better-driven network. On the
 * 9-layer diamond with current cortex→SNN input drive, 0.15 produced a
 * DEAD CASCADE: input fired at ~5% but L1 through output stayed pinned
 * at rest (-65 mV, 0 spikes across 1.6M neurons) — input→L1 weighted sum
 * was below L1's threshold. Homeostatic scale-up at 1.02/apply needed
 * ~170 applies to escape, and R-STDP couldn't help because (a) nothing
 * downstream was firing, (b) the reward formula was simultaneously
 * broken (see brain_learning.c commit note). Raised 0.15 → 0.25 to
 * restore initial cascade propagation. Combined with the asymmetric
 * homeostatic escape band (1.05 cap when rate < 10% of target) and
 * the fixed reward formula, this should produce healthy firing within
 * the first 100-200 training steps instead of staying collapsed. */
#define SNN_I_SYN_BUDGET          (0.25f * SNN_V_GAP)  /**< 3.75 mV target (propagates cascade on deep nets) */
/* Separate budget for input_pop → tier 0 fanout. Input pops receive
 * external current from cortex CNN output ON TOP OF this synaptic drive,
 * so they fire at cortex-rate. If we use the full 0.25 budget here,
 * input pops fire at ~37% (observed in prod) — way above the 3% target.
 * Homeostatic then has to compress 37%→3%, which hits the LIF threshold
 * cliff and collapses firing to ~0% in one apply interval. Using a
 * smaller budget for this edge keeps input pops near target at init,
 * while the larger downstream budget still propagates cascade. */
#define SNN_I_SYN_BUDGET_INPUT    (0.10f * SNN_V_GAP)  /**< 1.5 mV — input pops get external drive too */
#define SNN_PRESYN_RATE_DEFAULT   0.1f                 /**< 10% firing per step assumption */
/* Input_pop is externally driven. With Poisson rate encoding at ~100 Hz and
 * dt=1 ms, per-step firing prob ≈ 0.1. We keep this equal to default; if the
 * encoding drives harder in practice, reduce it. Too-high here → under-strong
 * input weights → dead cascade (the failure mode we just debugged). */
#define SNN_PRESYN_RATE_INPUT_POP 0.1f
/* Upper bound on any wiring weight. Prevents runaway if configured fan_in
 * becomes pathologically small (e.g. tiny connectivity). 10 mV = 2/3 of gap. */
#define SNN_W_CAP                 10.0f

static inline float snn_fluct_weight(float budget_share,
                                     uint32_t src_n,
                                     float connectivity,
                                     float presyn_rate)
{
    float fan_in = (float)src_n * connectivity * presyn_rate;
    if (fan_in < 1.0f) fan_in = 1.0f;  /* avoid divide-by-near-zero */
    float w = budget_share / fan_in;
    if (w > SNN_W_CAP) w = SNN_W_CAP;
    if (w < -SNN_W_CAP) w = -SNN_W_CAP;
    return w;
}

#define SNN_INPUT_FANOUT_CONNECTIVITY    0.01f
#define SNN_OUTPUT_CONVERGE_CONNECTIVITY 0.005f


/* Well-known SNN sidecar path from the checkpoint system.
 * brain.save(path) writes the SNN to path.snn — this is the canonical
 * location for the trained hierarchical SNN with CSR weights.
 * Also check the legacy separate cache path for backward compat. */
#define SNN_SIDECAR_PATH "checkpoints/athena/athena_immersive.bin.snn"
#define SNN_LEGACY_CACHE_PATH "checkpoints/athena/snn_hierarchical.bin"

snn_network_t* snn_create_hierarchical_network(
    uint32_t n_inputs,
    uint32_t n_outputs,
    uint32_t target_total_neurons)
{
    /* Try to load cached SNN from the checkpoint sidecar (saves ~8 min of wiring).
     * The sidecar is updated every checkpoint cycle with trained weights. */
    {
        extern snn_network_t* snn_network_load(const char* path);
        const char* cache_paths[] = { SNN_SIDECAR_PATH, SNN_LEGACY_CACHE_PATH };
        for (int ci = 0; ci < 2; ci++) {
            snn_network_t* cached = snn_network_load(cache_paths[ci]);
            if (!cached) continue;
            if (cached->config.n_inputs == n_inputs &&
                cached->config.n_outputs == n_outputs) {
                LOG_INFO("Loaded cached hierarchical SNN from %s "
                         "(%u populations, skipped wiring)",
                         cache_paths[ci], cached->n_populations);
                return cached;
            }
            LOG_WARN("Cached SNN at %s mismatch (inputs=%u/%u outputs=%u/%u)",
                     cache_paths[ci], cached->config.n_inputs, n_inputs,
                     cached->config.n_outputs, n_outputs);
            extern void snn_network_destroy(snn_network_t*);
            snn_network_destroy(cached);
        }
    }

    /* Compute actual total from tier definitions */
    uint32_t actual_total = 0;
    for (uint32_t t = 0; t < NUM_TIERS; t++) {
        actual_total += TIER_DEFS[t].n_pops * TIER_DEFS[t].neurons_per_pop;
    }
    LOG_INFO("Creating hierarchical SNN: %u neurons across %zu tiers "
             "(target=%u, actual=%u)",
             actual_total, NUM_TIERS, target_total_neurons, actual_total);

    /* Count total populations */
    uint32_t total_pops = 0;
    for (uint32_t t = 0; t < NUM_TIERS; t++) {
        total_pops += TIER_DEFS[t].n_pops;
    }

    /* Create SNN with base config */
    snn_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.n_inputs = n_inputs;
    cfg.n_outputs = n_outputs;
    /* Lightweight mode: tier populations use CSR synapse storage, NOT
     * neural_network_t neurons. Set n_hidden = 0 so the base network only
     * allocates input_pop + output_pop (tiny: ~3K neurons × 14KB = 43KB).
     * The 1.8M tier neurons use ~1 GB CSR instead of ~25 GB neuron_t. */
    cfg.n_hidden = 0;
    cfg.dt = 1.0f;  /* 1ms timestep */
    cfg.v_rest = -65.0f;
    cfg.v_reset = -70.0f;
    cfg.v_thresh = -50.0f;
    cfg.tau_mem = 20.0f;
    cfg.tau_syn = 5.0f;
    cfg.t_ref = 2.0f;
    cfg.input_current_scale = 45.0f;
    cfg.learning_rate = 0.001f;
    cfg.train_mode = SNN_TRAIN_R_STDP;
    cfg.enable_stdp = true;
    cfg.encoder.time_window = 100.0f;  /* 100ms encoding window */
    cfg.decoder.time_window = 100.0f;  /* 100ms decoding window (rate-based) */

    snn_network_t* net = snn_network_create(&cfg);
    if (!net) {
        LOG_ERROR("Failed to create base SNN network");
        return NULL;
    }

    /* Track population indices per tier for wiring.
     * Initialize all entries to 0 so early goto wire_connections
     * leaves unset tiers with safe (empty) ranges. */
    uint32_t tier_start_pop[NUM_TIERS + 1];
    memset(tier_start_pop, 0, sizeof(tier_start_pop));

    /* Note: snn_network_create already created input/hidden/output populations.
     * We need to add the additional populations. The initial 3 populations
     * (from snn_network_create) serve as tier 0 input + tier 7 output stubs.
     * We'll add tiers 1-6 as additional hidden populations. */

    /* Actually, let's just use the initial populations as-is and add more.
     * The key is wiring them correctly. Record all population indices. */
    uint32_t pop_idx = net->n_populations;  /* next free population slot */
    uint32_t pop_map[128];  /* pop_map[flat_idx] = actual population index in network */
    uint32_t flat_idx = 0;

    /* Map initial populations (created by snn_network_create) */
    /* Pop 0 = input, Pop 1 = hidden, Pop 2 = output from default create */
    /* We'll re-use pop 0 as tier 0 pop 0, and add the rest */

    /* P4.1: layer-specific pyramidal subtype per tier.
     *   tiers 2,3 (L2_pattern, L3_concept) → SNN_NSC_PYRAMIDAL_L23
     *   tier 4   (L4_integr)               → SNN_NSC_PYRAMIDAL_L4_STELLATE
     *   tier 5   (L5_exec)                 → SNN_NSC_PYRAMIDAL_L5_BETZ
     *   tier 6   (L6_project)              → SNN_NSC_PYRAMIDAL (corticothalamic;
     *                                        no special LIF profile measured
     *                                        for L6 modular intratelencephalic)
     * Tiers 0,1,7 (input / L1_feature / output) keep PYRAMIDAL — these are
     * stubs / sensory-relay / output-aggregation, not classical cortical
     * pyramidal layers, so a layer-specific tag would be misleading. */
    static const neuron_subclass_t TIER_SUBCLASS[] = {
        SNN_NSC_PYRAMIDAL,                  /* 0: input */
        SNN_NSC_PYRAMIDAL,                  /* 1: L1_feature */
        SNN_NSC_PYRAMIDAL_L23,              /* 2: L2_pattern */
        SNN_NSC_PYRAMIDAL_L23,              /* 3: L3_concept */
        SNN_NSC_PYRAMIDAL_L4_STELLATE,      /* 4: L4_integr */
        SNN_NSC_PYRAMIDAL_L5_BETZ,          /* 5: L5_exec */
        SNN_NSC_PYRAMIDAL,                  /* 6: L6_project */
        SNN_NSC_PYRAMIDAL,                  /* 7: output */
    };

    /* For simplicity: add ALL tier populations as new hidden populations,
     * ignoring the default 3. The default input/output pops are tiny stubs
     * that we can leave unused. The real work happens in our tier pops. */
    for (uint32_t t = 0; t < NUM_TIERS; t++) {
        tier_start_pop[t] = flat_idx;
        for (uint32_t p = 0; p < TIER_DEFS[t].n_pops; p++) {
            /* Add a new population with the tier's neuron count */
            neuron_type_t ntype = NEURON_GENERIC_LIF;
            char pop_name[64];
            snprintf(pop_name, sizeof(pop_name), "%s_%u",
                     TIER_DEFS[t].name, p);
            int rc = snn_network_add_population_lightweight(net,
                TIER_DEFS[t].neurons_per_pop,
                ntype, pop_name);

            if (rc < 0) {
                LOG_ERROR("Failed to add population %u (tier %s, pop %u/%u): rc=%d",
                          flat_idx, TIER_DEFS[t].name, p, TIER_DEFS[t].n_pops, rc);
                /* Continue with what we have — partial hierarchy is better than none */
                goto wire_connections;
            }
            pop_map[flat_idx] = (uint32_t)rc;  /* rc is the population index */
            (void)snn_network_set_pop_subclass(net, (uint32_t)rc,
                                               TIER_SUBCLASS[t]);
            /* Wave E FFI fix — tier pyramidal pops get a 1-step axonal
             * conduction delay (≈1 ms at default dt). Combined with PV's
             * 0-step (fast/myelinated soma) this restores the 1-3 ms
             * thalamus→pyr-AMPA vs PV→pyr-GABA window required for
             * canonical feed-forward inhibition.
             * See docs/claude/ffi-timing-audit-2026-04-27.md. */
            (void)snn_network_set_pop_conduction_delay(net, (uint32_t)rc, 1);
            /* Wave G — per-neuron LIF heterogeneity. Tier pyramidal pops
             * get σ = 0.10 (10 % relative σ on τ_mem and v_thresh) which
             * matches the cortical principal-cell distribution width.
             * PV/SOM/VIP/TRN interneurons are tighter and stay at σ = 0
             * for now (intrinsic uniformity is part of their fast-spiking
             * synchrony role). 2026-04-27. */
            (void)snn_network_set_pop_heterogeneity(net, (uint32_t)rc, 0.10f);
            /* Wave H — dendritic compartments. Tier-pyramidal pops with
             * any PYRAMIDAL_* subclass get the two-compartment treatment
             * IFF the runtime flag is non-zero at creation time.
             * Interneurons (PV/SOM/VIP/TRN) stay single-compartment by
             * design (no apical arbour worth modelling) and are wired
             * elsewhere in this file with their own subclass tag.
             * See docs/claude/wave-h-dendritic-design-2026-04-27.md. */
            if (snn_tune_get_dendritic_enabled() != 0.0f) {
                neuron_subclass_t sc = TIER_SUBCLASS[t];
                if (sc == SNN_NSC_PYRAMIDAL
                    || sc == SNN_NSC_PYRAMIDAL_L23
                    || sc == SNN_NSC_PYRAMIDAL_L4_STELLATE
                    || sc == SNN_NSC_PYRAMIDAL_L5_BETZ) {
                    int dend_rc = snn_network_enable_dendritic(
                        net, (uint32_t)rc);
                    if (dend_rc != 0) {
                        LOG_WARN("Dendritic enable failed for pop %u tier %s "
                                 "(rc=%d) — staying single-compartment",
                                 (uint32_t)rc, TIER_DEFS[t].name, dend_rc);
                    }
                }
            }
            flat_idx++;
        }
    }
wire_connections:
    tier_start_pop[NUM_TIERS] = flat_idx;  /* Always set, even on partial creation */
    LOG_INFO("Created %u populations (target %u). Wiring connections...", flat_idx, total_pops);

    /* Wire feedforward connections between adjacent tiers.
     * Fluctuation-driven: split I_syn budget across the K source pops in tier t. */
    uint32_t total_connections = 0;
    for (uint32_t t = 0; t + 1 < NUM_TIERS; t++) {
        float ff_conn = TIER_DEFS[t].ff_connectivity;
        if (ff_conn <= 0.0f) continue;
        uint32_t k_src = TIER_DEFS[t].n_pops;
        float per_conn_budget = SNN_I_SYN_BUDGET / (float)k_src;
        float w_mean = snn_fluct_weight(per_conn_budget,
                                        TIER_DEFS[t].neurons_per_pop,
                                        ff_conn,
                                        SNN_PRESYN_RATE_DEFAULT);
        LOG_INFO("Tier %s→%s FF: w=%.3f (k_src=%u n=%u conn=%.4f)",
                 TIER_DEFS[t].name, TIER_DEFS[t + 1].name, w_mean,
                 k_src, TIER_DEFS[t].neurons_per_pop, ff_conn);

        for (uint32_t sp = tier_start_pop[t]; sp < tier_start_pop[t + 1]; sp++) {
            for (uint32_t dp = tier_start_pop[t + 1]; dp < tier_start_pop[t + 2]; dp++) {
                if (sp >= flat_idx || dp >= flat_idx) continue;
                int nc = snn_network_connect_populations(net,
                    pop_map[sp], pop_map[dp],
                    SNN_TOPO_RANDOM, ff_conn,
                    SYNAPSE_AMPA, w_mean, w_mean * 0.3f);
                if (nc > 0) total_connections += (uint32_t)nc;
            }
        }
    }
    LOG_INFO("Feedforward: %u connections", total_connections);

    /* Wire within-tier recurrent connections (L2-L5).
     * Mix 80% excitatory / 20% inhibitory (GABA at 4× |E|). Net drive per
     * recurrent spike = 0.8·w − 0.2·4·w = 0 → neutral, recurrence contributes
     * variance without mean-dominating. Previously used 2× I which gave net
     * +0.4·w → runaway saturation (observed 99% firing at 1.8M neurons). */
    uint32_t recurrent_connections = 0;
    for (uint32_t t = 0; t < NUM_TIERS; t++) {
        float rec_conn = TIER_DEFS[t].recurrent_connectivity;
        if (rec_conn <= 0.0f) continue;

        uint32_t k_src_rec = TIER_DEFS[t].n_pops - 1;
        if (k_src_rec < 1) k_src_rec = 1;
        float rec_budget_share = SNN_I_SYN_BUDGET / (float)k_src_rec;
        float w_exc = snn_fluct_weight(rec_budget_share,
                                       TIER_DEFS[t].neurons_per_pop,
                                       rec_conn,
                                       SNN_PRESYN_RATE_DEFAULT);
        LOG_INFO("Tier %s recurrent: w_exc=%.3f (pure E→E AMPA, Dale-correct; "
                 "I balance from PV/SOM/VIP)",
                 TIER_DEFS[t].name, w_exc);

        /* Dale's principle (Wave C, 2026-04-27): pyramidal neurons in real
         * cortex are uniformly glutamatergic — they cannot emit GABA at any
         * subset of their terminals. Inhibitory tone for the recurrent loop
         * is supplied by the dedicated PV/SOM/VIP sub-pops wired below
         * (P2.2). Earlier mod-5 dual-class wiring (AMPA + GABA_A from the
         * same pyr source) was a pre-P2.2 placeholder that became redundant
         * AND biophysically incorrect once interneurons were in place.
         * See docs/claude/dale-audit-2026-04-27.md.
         *
         * E/I balance note: removing the 20% GABA branch means net per-spike
         * recurrent drive becomes +0.8·w_exc (was 0). The PV→pyr / SOM→pyr
         * GABA_A pathways must compensate; rate-controlled homeostasis
         * (existing brake rules) handles the rest. */
        for (uint32_t sp = tier_start_pop[t]; sp < tier_start_pop[t + 1]; sp++) {
            for (uint32_t dp = tier_start_pop[t]; dp < tier_start_pop[t + 1]; dp++) {
                if (sp == dp) continue;  /* no self-connections */
                if (sp >= flat_idx || dp >= flat_idx) continue;
                int nc = snn_network_connect_populations(net,
                    pop_map[sp], pop_map[dp],
                    SNN_TOPO_RANDOM, rec_conn,
                    SYNAPSE_AMPA, w_exc, 0.05f);
                if (nc > 0) {
                    recurrent_connections += (uint32_t)nc;
                } else if (nc < 0) {
                    LOG_ERROR("Recurrent connect failed: pop %u→%u, tier %u, rc=%d",
                              pop_map[sp], pop_map[dp], t, nc);
                } else {
                    LOG_WARN("Recurrent connect returned 0: pop %u→%u (src=%u dst=%u neurons), "
                             "connectivity=%.6f, tier %u",
                             pop_map[sp], pop_map[dp],
                             net->populations[pop_map[sp]] ? net->populations[pop_map[sp]]->n_neurons : 0,
                             net->populations[pop_map[dp]] ? net->populations[pop_map[dp]]->n_neurons : 0,
                             rec_conn, t);
                }
            }
        }
    }
    LOG_INFO("Recurrent: %u connections", recurrent_connections);

    /* P2.2: PV/SOM/VIP interneuron sub-pops with disinhibition chain.
     *
     * Real cortex has 3 main GABAergic types in roughly fixed proportions
     * (Tremblay et al. 2016, Pfeffer et al. 2013):
     *   PV  (parvalbumin, fast-spiking basket): ~40% of GABA, soma-targeting,
     *        gamma-rhythm sustainer.
     *   SOM (somatostatin, Martinotti):          ~30%, apical-dendrite-targeting,
     *        gates top-down NMDA on pyramidal apicals.
     *   VIP (vasoactive intestinal peptide):     ~10-15%, targets SOM with
     *        GABA — the disinhibition arm of the canonical attention circuit
     *        (Lee et al. 2013, Karnani et al. 2016).
     *
     * Canonical microcircuit for attentional gain:
     *   ACh / top-down → VIP → ⊣SOM → ⊣pyramidal apical → top-down NMDA
     *   gets through (without VIP active, SOM keeps the apical clamped and
     *   top-down NMDA cannot drive coincidence detection).
     *
     * We add 3 sub-pops per recurrent tier (T2..T6), tagged with subclass
     * so snn_pop_lif_params() applies fast-spiking τ for PV, slow τ for SOM,
     * and the network default for VIP. Wiring per tier:
     *   pyramidal → PV   AMPA, sparse (PV needs FF drive to track tier rate)
     *   pyramidal → SOM  AMPA, sparse (SOM tracks tier activity)
     *   prev-tier-pyr → VIP AMPA, very sparse (long-range drive proxy for ACh)
     *   PV  → pyramidal  GABA_A, fast somatic gate
     *   SOM → pyramidal  GABA_A, slow gate (in CB substrate, GABA_A targeting
     *                    the same neurons that get top-down NMDA — gating
     *                    happens via shared g_gaba_a hyperpolarising the
     *                    apical-equivalent compartment).
     *   VIP → SOM        GABA_A, the disinhibition arm.
     *
     * Sizes follow the design report: PV ~12% / SOM ~5% / VIP ~3% of one
     * tier-pyr pop's neuron count (kept per-pop here, not per-tier-total,
     * to keep the new pop count modest at +15 across 5 tiers). Weights
     * are intentionally low-gain at first deployment — the existing
     * mod-5 recurrent inhibition continues to provide the primary E-I
     * balance; the new pops add the BIOLOGICAL TOPOLOGY hooks. P2.2-tune
     * will rebalance once we observe steady-state firing rates. */
    static const uint32_t INH_TIERS[] = { 2, 3, 4, 5, 6 };
    #define NUM_INH_TIERS ((uint32_t)(sizeof(INH_TIERS)/sizeof(INH_TIERS[0])))
    #define INH_PV_FRAC      0.12f   /* of one tier-pyr pop's neurons */
    #define INH_SOM_FRAC     0.05f
    #define INH_VIP_FRAC     0.03f
    #define INH_DRIVE_FRAC   0.15f   /* fraction of SNN_I_SYN_BUDGET on inh wiring */
    #define INH_PYR_TO_INH_CONN  0.005f
    #define INH_INH_TO_PYR_CONN  0.005f
    #define INH_VIP_TO_SOM_CONN  0.010f
    #define INH_LONG_FF_CONN     0.002f

    uint32_t inh_pop_pv [NUM_INH_TIERS];
    uint32_t inh_pop_som[NUM_INH_TIERS];
    uint32_t inh_pop_vip[NUM_INH_TIERS];
    bool     inh_ok    [NUM_INH_TIERS];
    for (uint32_t i = 0; i < NUM_INH_TIERS; i++) {
        inh_ok[i] = false;
        uint32_t t = INH_TIERS[i];
        if (t >= NUM_TIERS) continue;
        uint32_t pyr_n = TIER_DEFS[t].neurons_per_pop;
        uint32_t pv_n  = (uint32_t)(pyr_n * INH_PV_FRAC);
        uint32_t som_n = (uint32_t)(pyr_n * INH_SOM_FRAC);
        uint32_t vip_n = (uint32_t)(pyr_n * INH_VIP_FRAC);
        if (pv_n < 256) pv_n = 256;
        if (som_n < 128) som_n = 128;
        if (vip_n < 64) vip_n = 64;

        char nm[64];
        snprintf(nm, sizeof(nm), "%s_PV", TIER_DEFS[t].name);
        int rc_pv = snn_network_add_population_lightweight(net, pv_n, NEURON_GENERIC_LIF, nm);
        snprintf(nm, sizeof(nm), "%s_SOM", TIER_DEFS[t].name);
        int rc_som = snn_network_add_population_lightweight(net, som_n, NEURON_GENERIC_LIF, nm);
        snprintf(nm, sizeof(nm), "%s_VIP", TIER_DEFS[t].name);
        int rc_vip = snn_network_add_population_lightweight(net, vip_n, NEURON_GENERIC_LIF, nm);

        if (rc_pv < 0 || rc_som < 0 || rc_vip < 0) {
            LOG_WARN("Inh sub-pops for tier %s incomplete (rc_pv=%d rc_som=%d rc_vip=%d) — "
                     "skipping disinhibition wiring for this tier",
                     TIER_DEFS[t].name, rc_pv, rc_som, rc_vip);
            continue;
        }
        inh_pop_pv [i] = (uint32_t)rc_pv;
        inh_pop_som[i] = (uint32_t)rc_som;
        inh_pop_vip[i] = (uint32_t)rc_vip;
        inh_ok[i] = true;

        (void)snn_network_set_pop_subclass(net, inh_pop_pv [i], SNN_NSC_PV);
        (void)snn_network_set_pop_subclass(net, inh_pop_som[i], SNN_NSC_SOM);
        (void)snn_network_set_pop_subclass(net, inh_pop_vip[i], SNN_NSC_VIP);

        /* Wave D — gap-junction (Connexin-36) coupling between PV basket
         * cells. Sized so PV synchrony emerges at ~40 Hz under typical
         * drive; only set on PV pops (SOM/VIP have no significant gap
         * coupling in cortex). The hot loop only applies this when
         * conductance_mode is ON, so a current-mode network is unaffected. */
        (void)snn_network_set_pop_gap_coupling(net, inh_pop_pv[i], 0.05f);

        /* Wave E FFI fix — PV interneurons get 0-step conduction delay
         * (fast-spiking, locally myelinated; the canonical PV pathway
         * is the FAST arm of feed-forward inhibition). SOM/VIP get 1
         * step (matching pyramidal default — they project across longer
         * distances). This delay differential is what makes PV's GABA
         * arrive within 1-3 ms of pyramidal AMPA: pyr→PV chain takes
         * 1 step (pyr conduct) + integration + 0 step (PV conduct),
         * vs the bypassed direct thalamic→pyr arm at 0 step (input_pop
         * is the source — input_pop default delay is 0).
         * See docs/claude/ffi-timing-audit-2026-04-27.md. */
        (void)snn_network_set_pop_conduction_delay(net, inh_pop_pv[i],  0);
        (void)snn_network_set_pop_conduction_delay(net, inh_pop_som[i], 1);
        (void)snn_network_set_pop_conduction_delay(net, inh_pop_vip[i], 1);
    }

    uint32_t inh_connections = 0;
    for (uint32_t i = 0; i < NUM_INH_TIERS; i++) {
        if (!inh_ok[i]) continue;
        uint32_t t = INH_TIERS[i];
        uint32_t pyr_n = TIER_DEFS[t].neurons_per_pop;

        float w_pyr_to_inh = snn_fluct_weight(
            SNN_I_SYN_BUDGET * INH_DRIVE_FRAC,
            pyr_n, INH_PYR_TO_INH_CONN, SNN_PRESYN_RATE_DEFAULT);
        float w_pv_to_pyr = snn_fluct_weight(
            SNN_I_SYN_BUDGET * INH_DRIVE_FRAC,
            (uint32_t)(pyr_n * INH_PV_FRAC), INH_INH_TO_PYR_CONN, SNN_PRESYN_RATE_DEFAULT);
        float w_som_to_pyr = snn_fluct_weight(
            SNN_I_SYN_BUDGET * INH_DRIVE_FRAC * 0.6f,  /* slower, weaker than PV */
            (uint32_t)(pyr_n * INH_SOM_FRAC), INH_INH_TO_PYR_CONN, SNN_PRESYN_RATE_DEFAULT);
        float w_vip_to_som = snn_fluct_weight(
            SNN_I_SYN_BUDGET * INH_DRIVE_FRAC,
            (uint32_t)(pyr_n * INH_VIP_FRAC), INH_VIP_TO_SOM_CONN, SNN_PRESYN_RATE_DEFAULT);

        /* Pyr → PV / SOM (each tier-pyr pop fans out to the single PV/SOM pop) */
        for (uint32_t sp = tier_start_pop[t]; sp < tier_start_pop[t + 1] && sp < flat_idx; sp++) {
            int nc;
            nc = snn_network_connect_populations(net,
                pop_map[sp], inh_pop_pv[i],
                SNN_TOPO_RANDOM, INH_PYR_TO_INH_CONN,
                SYNAPSE_AMPA, w_pyr_to_inh, w_pyr_to_inh * 0.3f);
            if (nc > 0) inh_connections += (uint32_t)nc;
            nc = snn_network_connect_populations(net,
                pop_map[sp], inh_pop_som[i],
                SNN_TOPO_RANDOM, INH_PYR_TO_INH_CONN,
                SYNAPSE_AMPA, w_pyr_to_inh, w_pyr_to_inh * 0.3f);
            if (nc > 0) inh_connections += (uint32_t)nc;
        }

        /* Wave E (FFI fix, 2026-04-27): direct input_pop → PV thalamic
         * afferent. Real thalamocortical projections fan out to BOTH
         * pyramidal cells AND PV interneurons in parallel. Without this,
         * PV can only spike after a pyramidal in its tier has already
         * fired (intra-tier pyr → PV collateral above), making the
         * "feed-forward" inhibition arm structurally feedback. With this
         * direct afferent, PV's faster τ_mem (10 ms vs pyr 18-25 ms)
         * lets it spike sooner under matched thalamic drive — restoring
         * the canonical FFI temporal precision window.
         * See docs/claude/ffi-timing-audit-2026-04-27.md. */
        if (net->input_pop) {
            float w_thal_pv = snn_fluct_weight(
                SNN_I_SYN_BUDGET_INPUT,
                net->input_pop->n_neurons,
                SNN_INPUT_FANOUT_CONNECTIVITY,
                SNN_PRESYN_RATE_INPUT_POP);
            int nc = snn_network_connect_populations(net,
                0,  /* pop 0 = input_pop */
                inh_pop_pv[i],
                SNN_TOPO_RANDOM, SNN_INPUT_FANOUT_CONNECTIVITY,
                SYNAPSE_AMPA, w_thal_pv, w_thal_pv * 0.3f);
            if (nc > 0) inh_connections += (uint32_t)nc;
        }

        /* prev-tier-pyr → VIP (long-range FF drive proxy for ACh / top-down) */
        if (t > 0) {
            uint32_t pt = t - 1;
            float w_long = snn_fluct_weight(
                SNN_I_SYN_BUDGET * INH_DRIVE_FRAC,
                TIER_DEFS[pt].neurons_per_pop, INH_LONG_FF_CONN, SNN_PRESYN_RATE_DEFAULT);
            for (uint32_t sp = tier_start_pop[pt]; sp < tier_start_pop[pt + 1] && sp < flat_idx; sp++) {
                int nc = snn_network_connect_populations(net,
                    pop_map[sp], inh_pop_vip[i],
                    SNN_TOPO_RANDOM, INH_LONG_FF_CONN,
                    SYNAPSE_AMPA, w_long, w_long * 0.3f);
                if (nc > 0) inh_connections += (uint32_t)nc;
            }
        }

        /* PV → pyr  (fast somatic gate)
         * SOM → pyr (slow gate; co-located with top-down NMDA on same pop) */
        for (uint32_t dp = tier_start_pop[t]; dp < tier_start_pop[t + 1] && dp < flat_idx; dp++) {
            int nc;
            nc = snn_network_connect_populations(net,
                inh_pop_pv[i], pop_map[dp],
                SNN_TOPO_RANDOM, INH_INH_TO_PYR_CONN,
                SYNAPSE_GABA_A, -w_pv_to_pyr, w_pv_to_pyr * 0.05f);
            if (nc > 0) inh_connections += (uint32_t)nc;
            nc = snn_network_connect_populations(net,
                inh_pop_som[i], pop_map[dp],
                SNN_TOPO_RANDOM, INH_INH_TO_PYR_CONN,
                SYNAPSE_GABA_A, -w_som_to_pyr, w_som_to_pyr * 0.05f);
            if (nc > 0) inh_connections += (uint32_t)nc;
        }

        /* VIP → SOM (GABA_A — the disinhibition arm: silencing SOM releases pyr) */
        {
            int nc = snn_network_connect_populations(net,
                inh_pop_vip[i], inh_pop_som[i],
                SNN_TOPO_RANDOM, INH_VIP_TO_SOM_CONN,
                SYNAPSE_GABA_A, -w_vip_to_som, w_vip_to_som * 0.05f);
            if (nc > 0) inh_connections += (uint32_t)nc;
        }

        LOG_INFO("Inh tier %s: PV=%u SOM=%u VIP=%u (subclass-tagged)",
                 TIER_DEFS[t].name,
                 inh_pop_pv[i], inh_pop_som[i], inh_pop_vip[i]);
    }
    LOG_INFO("Disinhibition: %u connections across %u tiers",
             inh_connections, NUM_INH_TIERS);

    /* Wire skip connections */
    uint32_t skip_connections = 0;
    for (uint32_t s = 0; s < NUM_SKIPS; s++) {
        uint32_t st = SKIP_DEFS[s].src_tier;
        uint32_t dt = SKIP_DEFS[s].dst_tier;
        float sc = SKIP_DEFS[s].connectivity;
        if (st >= NUM_TIERS || dt >= NUM_TIERS) continue;
        /* Skip: small supplementary share of budget (~1/8). */
        float skip_w = snn_fluct_weight(SNN_I_SYN_BUDGET / 8.0f,
                                        TIER_DEFS[st].neurons_per_pop,
                                        sc, SNN_PRESYN_RATE_DEFAULT);
        LOG_INFO("Skip %s→%s: w=%.3f (conn=%.4f)",
                 TIER_DEFS[st].name, TIER_DEFS[dt].name, skip_w, sc);

        for (uint32_t sp = tier_start_pop[st]; sp < tier_start_pop[st + 1]; sp++) {
            for (uint32_t dp = tier_start_pop[dt]; dp < tier_start_pop[dt + 1]; dp++) {
                if (sp >= flat_idx || dp >= flat_idx) continue;
                int nc = snn_network_connect_populations(net,
                    pop_map[sp], pop_map[dp],
                    SNN_TOPO_RANDOM, sc,
                    SYNAPSE_AMPA, skip_w, skip_w * 0.3f);
                if (nc > 0) skip_connections += (uint32_t)nc;
            }
        }
    }
    LOG_INFO("Skip: %u connections", skip_connections);

    /* P1.1: Top-down feedback (descending NMDA projections).
     *
     * Real cortex has massive descending projections L5/L6 → L4 → L2/3 → L1
     * that carry top-down predictions and modulate sensory processing
     * (Larkum 2013, Bastos et al. 2012, Friston's predictive coding).
     * Without these, every theory of cortical inference (predictive
     * coding, active inference, Bayesian brain) cannot operate at the
     * substrate level — the substrate can only do bottom-up sensory
     * accumulation.
     *
     * Topology — two pathways mirroring biology:
     *   L5_exec    → L3_concept   (frontal → posterior, action-context bias)
     *   L6_project → L2_pattern   (output expectation → mid-level features)
     *
     * Receptor: SYNAPSE_NMDA. Top-down terminates onto apical dendrites
     * of pyramidal neurons. NMDA's slow τ (≈ 100 ms) and Mg²⁺ block
     * make it a coincidence detector + slow modulator: at rest the Mg
     * block keeps it nearly silent, so top-down alone cannot drive
     * spiking. When the postsyn neuron is already depolarized by
     * bottom-up AMPA, the block opens and NMDA contributes — the
     * mechanism that lets top-down BIAS what feedforward already
     * delivers, without overriding it.
     *
     * Budget: 0.2 × FF (Larkum measured top-down PSPs at ~20% of
     * feedforward). Connectivity: 0.001 (0.1% — sparse, like real
     * descending projections; the L5 pop sends one synapse per ~1000
     * dst neurons rather than dense coverage). Per-synapse weight is
     * computed by snn_fluct_weight against an NMDA-specific scaling
     * factor 2.0 (which compensates for partial Mg block at depolarized
     * potentials, NOT at rest):
     *   v_rest -65 mV: m ≈ 0.06 → eff drive ≈ 2.0 × 0.06 = 0.12 of FF
     *     target ≈ 0.45 mV — well below threshold, behaves as quiet
     *     modulator ("not driver") in agreement with Larkum/Bastos.
     *   v_post -50 mV (depolarized by AMPA): m ≈ 0.78 → eff drive ≈
     *     1.56 of FF — full modulatory contribution exactly when the
     *     postsyn cell is already engaged. This is the coincidence-
     *     detection regime.
     *   v_post  +0 mV (peak): m ≈ 0.78, but driving force (E_nmda - V)
     *     collapses, so contribution self-limits.
     * Earlier (6.0× at rest) over-compensated Mg block: at -50 mV the
     * scaled drive reached ~5× FF — top-down would have been a driver,
     * not a modulator (P1.1 walkthrough finding). 2.0× is the value
     * Spruston 2008 / Larkum 2013 measurements support. */
    static const struct {
        uint32_t src_tier;  /* descending source */
        uint32_t dst_tier;  /* superficial target */
        float    connectivity;
    } TOP_DOWN_DEFS[] = {
        { 5, 3, 0.001f },  /* L5_exec   → L3_concept */
        { 6, 2, 0.001f },  /* L6_project → L2_pattern */
    };
    #define NUM_TOP_DOWN ((uint32_t)(sizeof(TOP_DOWN_DEFS)/sizeof(TOP_DOWN_DEFS[0])))
    #define TOP_DOWN_BUDGET_FRAC  0.2f   /* Larkum: top-down PSP ≈ 20% of FF */
    #define TOP_DOWN_NMDA_SCALE   2.0f   /* compensate Mg block at depol V, NOT at rest (see header comment) */

    uint32_t top_down_connections = 0;
    for (uint32_t i = 0; i < NUM_TOP_DOWN; i++) {
        uint32_t st = TOP_DOWN_DEFS[i].src_tier;
        uint32_t dt = TOP_DOWN_DEFS[i].dst_tier;
        float    tc = TOP_DOWN_DEFS[i].connectivity;
        if (st >= NUM_TIERS || dt >= NUM_TIERS) continue;
        float td_w = snn_fluct_weight(
            SNN_I_SYN_BUDGET * TOP_DOWN_BUDGET_FRAC * TOP_DOWN_NMDA_SCALE,
            TIER_DEFS[st].neurons_per_pop,
            tc, SNN_PRESYN_RATE_DEFAULT);
        LOG_INFO("Top-down NMDA %s→%s: w=%.3f (conn=%.4f)",
                 TIER_DEFS[st].name, TIER_DEFS[dt].name, td_w, tc);

        for (uint32_t sp = tier_start_pop[st]; sp < tier_start_pop[st + 1]; sp++) {
            for (uint32_t dp = tier_start_pop[dt]; dp < tier_start_pop[dt + 1]; dp++) {
                if (sp >= flat_idx || dp >= flat_idx) continue;
                int nc = snn_network_connect_populations(net,
                    pop_map[sp], pop_map[dp],
                    SNN_TOPO_RANDOM, tc,
                    SYNAPSE_NMDA, td_w, td_w * 0.3f);
                if (nc > 0) top_down_connections += (uint32_t)nc;
            }
        }
    }
    LOG_INFO("Top-down NMDA: %u connections (L5→L3, L6→L2)", top_down_connections);

    /* Wire input_pop (pop 0) → tier 0 populations.
     * The base network's input_pop has n_inputs neurons (e.g. 1024).
     * Tier 0 has 4 × 20K = 80K neurons. We fan out from input_pop to
     * all tier 0 populations so each input drives multiple tier-0 neurons.
     * Connectivity 10% = each input neuron drives ~2000 tier-0 neurons. */
    uint32_t input_fanout_conn = 0;
    if (tier_start_pop[0] < flat_idx && net->input_pop) {
        /* input_pop is the sole source. Use the smaller INPUT-specific
         * budget (not the full downstream budget) because input pops
         * also get external cortex drive — combining both at full budget
         * produced 37% firing rates that crashed through the LIF
         * threshold cliff under homeostatic scale-down. */
        float in_w = snn_fluct_weight(SNN_I_SYN_BUDGET_INPUT,
                                      net->input_pop->n_neurons,
                                      SNN_INPUT_FANOUT_CONNECTIVITY,
                                      SNN_PRESYN_RATE_INPUT_POP);
        LOG_INFO("Input fanout weight (fluctuation-driven): w=%.3f "
                 "(input_n=%u conn=%.3f rate=%.2f)",
                 in_w, net->input_pop->n_neurons,
                 SNN_INPUT_FANOUT_CONNECTIVITY, SNN_PRESYN_RATE_INPUT_POP);
        for (uint32_t dp = tier_start_pop[0]; dp < tier_start_pop[1] && dp < flat_idx; dp++) {
            int nc = snn_network_connect_populations(net,
                0,  /* pop 0 = input_pop */
                pop_map[dp],
                SNN_TOPO_RANDOM, SNN_INPUT_FANOUT_CONNECTIVITY,
                SYNAPSE_AMPA, in_w, in_w * 0.3f);
            if (nc > 0) input_fanout_conn += (uint32_t)nc;
        }
    }
    LOG_INFO("Input fan-out: input_pop → tier 0: %u connections", input_fanout_conn);

    /* Wire tier 7 populations → output_pop (pop 2).
     * Tier 7 has 4 × 64K = 256K neurons. The output_pop has n_outputs
     * neurons (e.g. 2048). We converge tier 7 activity into output_pop
     * so snn_network_get_outputs() captures the hierarchy's final state.
     * Connectivity 5% = each output neuron receives from ~12800 tier-7 neurons. */
    uint32_t output_converge_conn = 0;
    if (NUM_TIERS > 0 && tier_start_pop[NUM_TIERS - 1] < flat_idx) {
        uint32_t last_tier = NUM_TIERS - 1;
        uint32_t k_src_out = TIER_DEFS[last_tier].n_pops;
        float out_w = snn_fluct_weight(SNN_I_SYN_BUDGET / (float)k_src_out,
                                       TIER_DEFS[last_tier].neurons_per_pop,
                                       SNN_OUTPUT_CONVERGE_CONNECTIVITY,
                                       SNN_PRESYN_RATE_DEFAULT);
        for (uint32_t sp = tier_start_pop[last_tier];
             sp < tier_start_pop[NUM_TIERS] && sp < flat_idx; sp++) {
            int nc = snn_network_connect_populations(net,
                pop_map[sp],
                2,  /* pop 2 = output_pop */
                SNN_TOPO_RANDOM, SNN_OUTPUT_CONVERGE_CONNECTIVITY,
                SYNAPSE_AMPA, out_w, out_w * 0.3f);
            if (nc > 0) {
                output_converge_conn += (uint32_t)nc;
            } else {
                LOG_WARN("Output convergence returned %d: pop %u→2 (src=%u, dst=%u neurons)",
                         nc, pop_map[sp],
                         net->populations[pop_map[sp]] ? net->populations[pop_map[sp]]->n_neurons : 0,
                         net->output_pop ? net->output_pop->n_neurons : 0);
            }
        }
    }
    LOG_INFO("Output convergence: tier 7 → output_pop: %u connections",
             output_converge_conn);

    /* P3.1: Reticular thalamic nucleus (TRN).
     *
     * TRN is a thin GABAergic shell wrapping the dorsal thalamus. It receives
     * collaterals from BOTH ascending sensory thalamic relay axons and
     * descending corticothalamic (L6) axons, and projects GABA_A inhibition
     * back onto the thalamic relay. This loop implements stimulus-specific
     * gain control: when L6 expects a feature, the matching TRN sector
     * fires and gates the corresponding sensory channel in or out. Without
     * this, the substrate has no biological mechanism for attentional gain
     * on sensory input — flagged in the substrate-correctness audit.
     *
     * Tier 0 here plays the dorsal-thalamic-relay role (ff_conn=0.005, no
     * recurrence — matches LGN/MGN/VPL anatomy). We add a single TRN pop
     * (~10K neurons, ~12% of tier 0 size, biologically realistic) wired:
     *   tier 0  → TRN     NMDA, sparse 0.001 (relay collaterals)
     *   L6      → TRN     NMDA, sparse 0.001 (corticothalamic feedback)
     *   TRN     → tier 0  GABA_A, denser 0.005 (gating inhibition)
     *
     * NMDA on the inputs: TRN integrates slowly and L6→TRN PSPs are
     * NMDA-dominated in vivo. GABA_A on the output: TRN→relay inhibition
     * is fast/phasic. Note this is NEURON_GENERIC_LIF for now; the
     * proper PV-style GABAergic neuron type lands in P2.1. */
    #define TRN_NEURONS              10000u
    #define TRN_INPUT_CONNECTIVITY   0.001f
    #define TRN_OUTPUT_CONNECTIVITY  0.005f
    #define TRN_INPUT_BUDGET_FRAC    0.3f

    uint32_t trn_input_conn = 0, trn_output_conn = 0;
    int trn_pop_rc = snn_network_add_population_lightweight(
        net, TRN_NEURONS, NEURON_GENERIC_LIF, "thalamus_reticular");
    if (trn_pop_rc >= 0) {
        uint32_t trn_pop_idx = (uint32_t)trn_pop_rc;
        /* Tag with TRN subclass — picks up bursting τ_mem (12 ms) and
         * longer t_ref (3 ms) profile via snn_pop_lif_params(). */
        (void)snn_network_set_pop_subclass(net, trn_pop_idx, SNN_NSC_TRN);

        /* Tier 0 (sensory thalamic relay) → TRN: NMDA modulatory */
        if (NUM_TIERS > 0 && tier_start_pop[0] < flat_idx) {
            float w_in_t0 = snn_fluct_weight(
                SNN_I_SYN_BUDGET * TRN_INPUT_BUDGET_FRAC * TOP_DOWN_NMDA_SCALE,
                TIER_DEFS[0].neurons_per_pop,
                TRN_INPUT_CONNECTIVITY, SNN_PRESYN_RATE_DEFAULT);
            for (uint32_t sp = tier_start_pop[0];
                 sp < tier_start_pop[1] && sp < flat_idx; sp++) {
                int nc = snn_network_connect_populations(net,
                    pop_map[sp], trn_pop_idx,
                    SNN_TOPO_RANDOM, TRN_INPUT_CONNECTIVITY,
                    SYNAPSE_NMDA, w_in_t0, w_in_t0 * 0.3f);
                if (nc > 0) trn_input_conn += (uint32_t)nc;
            }
        }

        /* L6_project (tier 6) → TRN: NMDA corticothalamic feedback */
        if (NUM_TIERS > 6 && tier_start_pop[6] < flat_idx) {
            float w_in_l6 = snn_fluct_weight(
                SNN_I_SYN_BUDGET * TRN_INPUT_BUDGET_FRAC * TOP_DOWN_NMDA_SCALE,
                TIER_DEFS[6].neurons_per_pop,
                TRN_INPUT_CONNECTIVITY, SNN_PRESYN_RATE_DEFAULT);
            for (uint32_t sp = tier_start_pop[6];
                 sp < tier_start_pop[7] && sp < flat_idx; sp++) {
                int nc = snn_network_connect_populations(net,
                    pop_map[sp], trn_pop_idx,
                    SNN_TOPO_RANDOM, TRN_INPUT_CONNECTIVITY,
                    SYNAPSE_NMDA, w_in_l6, w_in_l6 * 0.3f);
                if (nc > 0) trn_input_conn += (uint32_t)nc;
            }
        }

        /* TRN → tier 0: GABA_A gating inhibition. Full I_syn budget so a
         * coordinated TRN ensemble can substantially reduce relay firing.
         * Negative weight follows the codebase convention (the per-receptor
         * deposit absolute-values into g_gaba_a regardless of sign). */
        if (NUM_TIERS > 0 && tier_start_pop[0] < flat_idx) {
            float w_out = snn_fluct_weight(
                SNN_I_SYN_BUDGET,
                TRN_NEURONS,
                TRN_OUTPUT_CONNECTIVITY, SNN_PRESYN_RATE_DEFAULT);
            for (uint32_t dp = tier_start_pop[0];
                 dp < tier_start_pop[1] && dp < flat_idx; dp++) {
                int nc = snn_network_connect_populations(net,
                    trn_pop_idx, pop_map[dp],
                    SNN_TOPO_RANDOM, TRN_OUTPUT_CONNECTIVITY,
                    SYNAPSE_GABA_A, -w_out, w_out * 0.05f);
                if (nc > 0) trn_output_conn += (uint32_t)nc;
            }
        }

        LOG_INFO("Reticular thalamus: pop %u (%u neurons), "
                 "%u input syns (tier0+L6→TRN, NMDA), "
                 "%u output syns (TRN→tier0, GABA_A)",
                 trn_pop_idx, TRN_NEURONS, trn_input_conn, trn_output_conn);
    } else {
        LOG_WARN("Failed to add reticular thalamus population: rc=%d", trn_pop_rc);
    }

    /* Finalize CSR storage on all lightweight populations.
     * This sorts the COO entries by destination neuron and builds
     * the row_ptr index for O(1) per-neuron lookup during stepping. */
    snn_network_finalize_connections(net);

    LOG_INFO("Hierarchical SNN complete: %u neurons, %u connections "
             "(ff=%u, rec=%u, skip=%u, td=%u, in_fan=%u, out_conv=%u, "
             "trn_in=%u, trn_out=%u)",
             actual_total + TRN_NEURONS,
             total_connections + recurrent_connections + skip_connections
                 + top_down_connections + input_fanout_conn
                 + output_converge_conn + trn_input_conn + trn_output_conn,
             total_connections, recurrent_connections, skip_connections,
             top_down_connections, input_fanout_conn, output_converge_conn,
             trn_input_conn, trn_output_conn);

    /* Save initial CSR to sidecar path so the first checkpoint has it.
     * Subsequent saves happen during brain.save() checkpoint cycle.
     * Disk guard: refuse if < 15 GB free (gzipped .snn is ~10-12 GB).
     * Without this check, a low-disk pod fills up trying to write
     * the cache and crashes the brain init. */
    {
        struct statvfs _st;
        if (statvfs("checkpoints/athena", &_st) == 0) {
            double free_gb = (double)_st.f_bavail * _st.f_frsize / (1024.0 * 1024.0 * 1024.0);
            /* Aligned with Python disk guard (20 GB). Audit bug #8. */
            if (free_gb < 20.0) {
                LOG_WARN("Skipping initial SNN sidecar save: only %.1f GB free "
                         "(need 20+). Brain will work but next restart re-wires SNN.",
                         free_gb);
            } else {
                extern int snn_network_save(snn_network_t* network, const char* path);
                if (snn_network_save(net, SNN_SIDECAR_PATH) == 0) {
                    LOG_INFO("Saved initial SNN to %s (subsequent saves via checkpoint cycle)",
                             SNN_SIDECAR_PATH);
                }
            }
        }
    }

    return net;
}
