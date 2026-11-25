/**
 * @file nimcp_brain_wired_noise_immune.c
 * @brief Wired-module group BRAIN_WIRED_NOISE_IMMUNE. See include/core/brain/nimcp_brain_wired.h.
 *
 * Two families of previously-uncompiled bridges are owned and driven here.
 *
 * 1. PINK-NOISE PLASTICITY BRIDGES (1/f parameter fluctuations)
 *    Each bridge owns a pink-noise generator and produces "noisy" versions of
 *    the parameters of one plasticity mechanism. The bridges themselves never
 *    write into a live subsystem, so this group does it, under strict rules:
 *
 *      live = baseline * (1 + max_rel * tanh(z / 2))
 *
 *    where z is the bridge's relative fluctuation normalised to zero mean /
 *    unit variance (running EMA). The applied deviation is therefore always
 *    within +/- max_rel of the baseline (default 2%, env
 *    NIMCP_WIRED_NOISE_MAX_REL in [0, 0.1]; 0 = compute only, never write).
 *    The baseline is the value the subsystem had before we touched it; if
 *    anyone else changes the live value (a setter RPC, a checkpoint load) we
 *    adopt the new value as the baseline instead of fighting it. Every value
 *    written is checked with isfinite() first; a non-finite live value is left
 *    alone. Baselines are persisted (sidecar) and restored on destroy so the
 *    fluctuation never random-walks across save/load cycles.
 *
 *    bridge                    -> live parameter it modulates
 *    sfa_pink_noise            -> SNN per-population AHP adaptation (tau_ms, gain_mv)
 *    glio_pink_noise           -> astrocyte network Ca2+ wave threshold (under net lock)
 *    bcm_pink_noise            -> homeostatic BCM sliding-threshold ceiling (meta_params.max_theta)
 *    elig_pink_noise           -> SNN training eligibility-trace decay rate (1/tau)
 *    homeo_pink_noise          -> homeostatic synaptic-scaling target rate
 *    meta_pink_noise           -> homeostatic metaplasticity theta sliding tau
 *    struct_pink_noise         -> structural plasticity formation / pruning rates
 *
 *    Bridges whose subsystem is NULL are not created; tick() retries lazily
 *    (every 1024 ticks) so subsystems created after cognitive init still get
 *    their bridge.
 *
 * 2. IMMUNE BRIDGES (information / optimisation / GPU <-> brain immune system)
 *    Created only when brain->immune_system exists (it is OFF in production,
 *    in which case everything here is a no-op). When on, they are driven every
 *    64 ticks with real signals:
 *      shannon_immune      <- normalised entropy of the training features,
 *                             brain->last_shannon_metrics.bottleneck_score
 *      cross_modal_immune  <- brain->last_cross_modal_metrics (MI / efficiency)
 *      gpu_neuron_immune   <- brain->gpu_ctx error string + allocation stats
 *                             (created only when a GPU context exists; never
 *                             allocates GPU memory, no gpu_neural_network)
 *      qa_immune           <- immune inflammation -> annealer temp/iter factors
 *                             (created when brain->quantum_annealer exists)
 *    Their outputs (capacity / efficiency / batch / temperature factors) are
 *    reported through status_json.
 *
 * Tick cost: one counter increment on most ticks. Every 4th tick: ~20 pink
 * noise samples + <= 2 x n_populations float stores (+ one O(n) pass over the
 * features for BCM activity). Every 64th tick with immune on: one O(n) feature
 * histogram + a handful of immune stat reads.
 */
#include "core/brain/nimcp_brain_wired.h"
#include "core/brain/nimcp_brain_internal.h"

#include "core/neuron_models/nimcp_sfa_pink_noise_bridge.h"
#include "glial/astrocytes/nimcp_gliotransmission_pink_noise_bridge.h"
#include "plasticity/bcm/nimcp_bcm_pink_noise_bridge.h"
#include "plasticity/eligibility/nimcp_eligibility_pink_noise_bridge.h"
#include "plasticity/homeostatic/nimcp_homeostatic_pink_noise_bridge.h"
#include "plasticity/metaplasticity/nimcp_metaplasticity_pink_noise_bridge.h"
#include "plasticity/structural/nimcp_structural_pink_noise_bridge.h"
#include "plasticity/structural/nimcp_structural_plasticity.h"
#include "plasticity/homeostatic/nimcp_homeostatic.h"
#include "plasticity/bcm/nimcp_bcm.h"
#include "glial/astrocytes/nimcp_astrocytes.h"
#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_adaptation.h"

#include "gpu/immune/nimcp_gpu_neuron_immune_bridge.h"
#include "information/immune/nimcp_cross_modal_immune_bridge.h"
#include "information/immune/nimcp_shannon_immune_bridge.h"
#include "optimization/immune/nimcp_quantum_annealing_immune_bridge.h"
#include "gpu/context/nimcp_gpu_context.h"

#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Tunables                                                                   */
/* ------------------------------------------------------------------------- */
#define NI_NOISE_EVERY        4u      /* ticks between pink-noise updates (~66 ms) */
#define NI_IMMUNE_EVERY       64u     /* ticks between immune bridge updates (~1 s) */
#define NI_ENSURE_EVERY       1024u   /* ticks between lazy-create retries */
#define NI_DEFAULT_MAX_REL    0.02f   /* +/-2% max deviation from baseline */
#define NI_MAX_REL_CEIL       0.10f
#define NI_STAT_ALPHA         0.01f   /* EMA rate for z-normalisation */
#define NI_SIDE_MAGIC         0x4E4F494Du  /* "NOIM" */
#define NI_SIDE_VERSION       1u

/* Scalar modulation channels (index-stable: persisted in the sidecar). */
enum {
    NI_CH_HOMEO_TARGET = 0,   /* homeostatic scaling_params.target_rate   */
    NI_CH_META_THETA_TAU,     /* homeostatic meta_params.theta_tau        */
    NI_CH_BCM_MAX_THETA,      /* homeostatic meta_params.max_theta        */
    NI_CH_STRUCT_FORM,        /* structural config.formation_rate         */
    NI_CH_STRUCT_PRUNE,       /* structural config.pruning_rate           */
    NI_CH_ELIG_DECAY,         /* snn_training_ctx->eligibility_decay      */
    NI_CH_ASTRO_CA_THRESH,    /* astrocyte_network->calcium_threshold_um  */
    NI_CH_COUNT
};

static const char* const k_ch_names[NI_CH_COUNT] = {
    "homeo_target_rate", "meta_theta_tau", "bcm_max_theta",
    "struct_formation_rate", "struct_pruning_rate",
    "elig_decay", "astro_ca_threshold"
};

typedef struct {
    float*   target;      /* live parameter currently bound (NULL = unbound) */
    float    base;        /* baseline (un-noised) value                      */
    float    last;        /* last value we wrote                             */
    float    mean;        /* EMA of raw relative fluctuation                 */
    float    var;         /* EMA of its variance                             */
    float    applied;     /* last applied relative deviation                 */
    uint8_t  init;        /* base valid                                      */
    uint8_t  wrote;       /* last valid                                      */
    uint8_t  base_from_sidecar;
    uint64_t writes;
} ni_chan_t;

typedef struct {
    const void* pop;      /* snn_population_t* the channels below belong to */
    ni_chan_t   tau;
    ni_chan_t   gain;
} ni_sfa_pop_t;

typedef struct {
    /* pink-noise bridges */
    sfa_pink_noise_bridge_t*    sfa;
    glio_pink_noise_bridge_t*   glio;
    bcm_pink_noise_bridge_t*    bcm;
    elig_pink_noise_bridge_t*   elig;
    homeo_pink_noise_bridge_t*  homeo;
    meta_pink_noise_bridge_t*   meta;
    struct_pink_noise_bridge_t* structural;

    bcm_params_t                bcm_snapshot;    /* private base params for the BCM bridge   */
    synaptic_scaling_params_t   homeo_snapshot;  /* private base params for the homeo bridge */

    ni_chan_t     ch[NI_CH_COUNT];
    ni_sfa_pop_t  sfa_pop[SNN_MAX_POPULATIONS];
    ni_chan_t     sfa_rel_tau;   /* normalisation state for the shared SFA signals */
    ni_chan_t     sfa_rel_gain;

    /* sidecar baselines waiting for their channel to bind (load before bind) */
    float    pending_base[NI_CH_COUNT];
    uint8_t  pending_valid[NI_CH_COUNT];

    /* immune bridges */
    shannon_immune_bridge_t*     shannon;
    cross_modal_immune_bridge_t* cross_modal;
    gpu_neuron_immune_bridge_t*  gpu;
    qa_immune_bridge_t*          qa;
    brain_immune_system_t*       immune_bound;   /* immune system the bridges were made with */
    char     gpu_last_err[128];
    float    last_entropy;
    uint64_t immune_updates;
    uint64_t gpu_errors_reported;

    float    max_rel;
    uint64_t ticks;
    uint64_t noise_updates;
    float    immune_dt_acc_ms;
    float    last_activity;
} ni_state_t;

/* ------------------------------------------------------------------------- */
/* Small helpers                                                              */
/* ------------------------------------------------------------------------- */
static inline float ni_load(const float* p) {
    float v;
    __atomic_load(p, &v, __ATOMIC_RELAXED);
    return v;
}

static inline void ni_store(float* p, float v) {
    __atomic_store(p, &v, __ATOMIC_RELAXED);
}

static float ni_env_max_rel(void) {
    const char* s = getenv("NIMCP_WIRED_NOISE_MAX_REL");
    if (!s || !*s) {
        return NI_DEFAULT_MAX_REL;
    }
    float v = strtof(s, NULL);
    if (!isfinite(v) || v < 0.0f) {
        return NI_DEFAULT_MAX_REL;
    }
    return v > NI_MAX_REL_CEIL ? NI_MAX_REL_CEIL : v;
}

/* Turn a raw relative fluctuation into a bounded, zero-mean deviation. */
static float ni_normalise(ni_chan_t* c, float rel_raw, float max_rel) {
    if (!isfinite(rel_raw)) {
        return 0.0f;
    }
    float d = rel_raw - c->mean;
    c->mean += NI_STAT_ALPHA * d;
    c->var = (1.0f - NI_STAT_ALPHA) * c->var + NI_STAT_ALPHA * d * d;
    if (!isfinite(c->mean) || !isfinite(c->var)) {
        c->mean = 0.0f;
        c->var = 0.0f;
        return 0.0f;
    }
    if (c->var < 1e-18f) {
        return 0.0f;
    }
    float z = d / sqrtf(c->var);
    float r = max_rel * tanhf(0.5f * z);
    return isfinite(r) ? r : 0.0f;
}

/* (Re)bind a channel to a live parameter; adopts baseline on first bind. */
static void ni_bind(ni_state_t* st, int idx, float* target) {
    ni_chan_t* c = &st->ch[idx];
    if (c->target == target) {
        return;
    }
    c->target = target;
    c->init = 0;
    c->wrote = 0;
    c->base_from_sidecar = 0;
    if (!target) {
        return;
    }
    float cur = ni_load(target);
    if (!isfinite(cur)) {
        return;
    }
    c->base = cur;
    c->init = 1;
    /* A sidecar baseline wins if the live value is just our own old noise. */
    if (st->pending_valid[idx]) {
        float pb = st->pending_base[idx];
        float tol = 2.5f * NI_MAX_REL_CEIL * fabsf(pb);
        if (isfinite(pb) && fabsf(cur - pb) <= tol) {
            c->base = pb;
            c->base_from_sidecar = 1;
            ni_store(target, pb);
        }
        st->pending_valid[idx] = 0;
    }
}

/* Write baseline * (1 + rel) into the channel's bound parameter. */
static void ni_apply(ni_chan_t* c, float rel, float lo, float hi) {
    if (!c->target) {
        return;
    }
    float cur = ni_load(c->target);
    if (!isfinite(cur)) {
        return;                     /* never touch a poisoned value */
    }
    if (!c->init) {
        c->base = cur;
        c->init = 1;
    } else if (c->wrote && cur != c->last) {
        c->base = cur;              /* someone else changed it: adopt */
    }
    float v = c->base * (1.0f + rel);
    if (!isfinite(v)) {
        return;
    }
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    ni_store(c->target, v);
    c->last = v;
    c->wrote = 1;
    c->applied = rel;
    c->writes++;
}

/* Put the baseline back if the live value is still the one we wrote. */
static void ni_restore(ni_chan_t* c) {
    if (!c->target || !c->init || !c->wrote) {
        return;
    }
    if (ni_load(c->target) == c->last && isfinite(c->base)) {
        ni_store(c->target, c->base);
    }
    c->wrote = 0;
}

static homeostatic_config_t* ni_homeo_cfg(struct brain_struct* brain) {
    /* homeostatic_controller_struct is opaque but its FIRST member is
     * homeostatic_config_t config (src/plasticity/homeostatic/nimcp_homeostatic.c);
     * a pointer to a struct may be converted to a pointer to its first member
     * (C11 6.7.2.1p15). There is no public accessor for the live config. */
    return brain->homeostatic ? (homeostatic_config_t*)(void*)brain->homeostatic : NULL;
}

static structural_plasticity_config_t* ni_struct_cfg(struct brain_struct* brain) {
    /* Same first-member rule: struct structural_plasticity_system starts with
     * structural_plasticity_config_t config
     * (src/plasticity/structural/nimcp_structural_plasticity.c). */
    return brain->structural_plasticity
        ? (structural_plasticity_config_t*)(void*)brain->structural_plasticity : NULL;
}

/* ------------------------------------------------------------------------- */
/* Bridge creation (idempotent; also used for lazy retries from tick)         */
/* ------------------------------------------------------------------------- */
static void ni_ensure_noise_bridges(struct brain_struct* brain, ni_state_t* st) {
    homeostatic_config_t* hc = ni_homeo_cfg(brain);

    if (!st->sfa && brain->snn_network && brain->snn_network->n_populations > 0) {
        st->sfa = sfa_pink_noise_create(NULL);
        if (st->sfa) {
            (void)sfa_pink_noise_connect(st->sfa, brain->snn_network);
        }
    }
    if (!st->glio && brain->astrocyte_network) {
        st->glio = glio_pink_noise_create(NULL);
        if (st->glio) {
            (void)glio_pink_noise_connect(st->glio, brain->astrocyte_network);
        }
    }
    if (!st->bcm && hc && hc->enable_metaplasticity) {
        st->bcm = bcm_pink_noise_create(NULL);
        if (st->bcm) {
            st->bcm_snapshot = bcm_params_cortical();
            (void)bcm_pink_noise_connect_bcm(st->bcm, &st->bcm_snapshot);
        }
    }
    if (!st->elig && brain->snn_training_ctx) {
        st->elig = elig_pink_noise_create(NULL);
        if (st->elig) {
            (void)elig_pink_noise_connect(st->elig, brain->snn_training_ctx);
        }
    }
    if (!st->homeo && hc && hc->enable_synaptic_scaling) {
        st->homeo = homeo_pink_noise_create(NULL);
        if (st->homeo) {
            st->homeo_snapshot = hc->scaling_params;
            (void)homeo_pink_noise_connect_scaling(st->homeo, &st->homeo_snapshot);
        }
    }
    if (!st->meta && hc && hc->enable_metaplasticity) {
        st->meta = meta_pink_noise_create(NULL);
        if (st->meta) {
            (void)meta_pink_noise_connect_meta(st->meta, brain->homeostatic);
        }
    }
    if (!st->structural && brain->structural_plasticity) {
        st->structural = struct_pink_noise_create(NULL);
        if (st->structural) {
            (void)struct_pink_noise_connect(st->structural, brain->structural_plasticity);
        }
    }
}

static void ni_destroy_immune_bridges(ni_state_t* st) {
    if (st->shannon)     { shannon_immune_destroy(st->shannon);         st->shannon = NULL; }
    if (st->cross_modal) { cross_modal_immune_destroy(st->cross_modal); st->cross_modal = NULL; }
    if (st->gpu)         { gpu_neuron_immune_destroy(st->gpu);          st->gpu = NULL; }
    if (st->qa)          { qa_immune_destroy(st->qa);                   st->qa = NULL; }
    st->immune_bound = NULL;
}

static void ni_ensure_immune_bridges(struct brain_struct* brain, ni_state_t* st) {
    brain_immune_system_t* imm = brain->immune_system;
    if (st->immune_bound && st->immune_bound != imm) {
        ni_destroy_immune_bridges(st);          /* immune system replaced/removed */
    }
    if (!imm) {
        return;                                 /* immune OFF: no-op */
    }
    st->immune_bound = imm;
    if (!st->shannon) {
        st->shannon = shannon_immune_create(NULL, imm, NULL);
    }
    if (!st->cross_modal) {
        st->cross_modal = cross_modal_immune_create(NULL, imm, NULL);
    }
    if (!st->gpu && brain->gpu_ctx) {
        /* No gpu_neural_network_t exists on the brain; the bridge runs its
         * cytokine path only and allocates no GPU memory. */
        st->gpu = gpu_neuron_immune_create(NULL, imm, NULL);
    }
    if (!st->qa && brain->quantum_annealer) {
        st->qa = qa_immune_create(NULL, brain->quantum_annealer, imm);
    }
}

/* ------------------------------------------------------------------------- */
/* Pink-noise drive                                                           */
/* ------------------------------------------------------------------------- */
static float ni_ratio_rel(float noisy, float base) {
    if (!isfinite(noisy) || !isfinite(base) || fabsf(base) < 1e-12f) {
        return 0.0f;
    }
    return noisy / base - 1.0f;
}

static void ni_drive_sfa(struct brain_struct* brain, ni_state_t* st, float max_rel) {
    if (!st->sfa || sfa_pink_noise_update(st->sfa) != 0) {
        return;
    }
    float r_tau  = ni_normalise(&st->sfa_rel_tau,
                                ni_ratio_rel(sfa_pink_noise_get_tau(st->sfa), SFA_DEFAULT_TAU), max_rel);
    float r_gain = ni_normalise(&st->sfa_rel_gain,
                                ni_ratio_rel(sfa_pink_noise_get_strength(st->sfa), SFA_DEFAULT_STRENGTH),
                                max_rel);
    st->sfa_rel_tau.applied = r_tau;
    st->sfa_rel_gain.applied = r_gain;
    if (max_rel <= 0.0f) {
        return;
    }
    struct snn_network_s* net = brain->snn_network;
    if (!net || !net->populations) {
        return;
    }
    uint32_t np = net->n_populations;
    if (np > SNN_MAX_POPULATIONS) np = SNN_MAX_POPULATIONS;
    if (np > net->populations_capacity) np = net->populations_capacity;
    for (uint32_t i = 0; i < np; i++) {
        snn_population_t* pop = net->populations[i];
        ni_sfa_pop_t* sp = &st->sfa_pop[i];
        snn_adaptation_state_t* ahp = pop ? pop->ahp : NULL;
        if (sp->pop != (const void*)pop || (ahp && sp->tau.target != &ahp->tau_ms)) {
            memset(sp, 0, sizeof(*sp));
            sp->pop = pop;
            if (ahp) {
                sp->tau.target  = &ahp->tau_ms;
                sp->gain.target = &ahp->gain_mv;
            }
        }
        if (!ahp) {
            continue;
        }
        ni_apply(&sp->tau,  r_tau,  0.5f, 1e6f);    /* tau stays strictly positive */
        ni_apply(&sp->gain, r_gain, 0.0f, 1e3f);    /* gain stays non-negative     */
    }
}

static void ni_drive_noise(struct brain_struct* brain, ni_state_t* st,
                           const float* f, uint32_t n) {
    const float mr = st->max_rel;
    homeostatic_config_t* hc = ni_homeo_cfg(brain);
    structural_plasticity_config_t* sc = ni_struct_cfg(brain);

    /* Re-resolve live targets every update (subsystems may be recreated). */
    ni_bind(st, NI_CH_HOMEO_TARGET,   (st->homeo && hc) ? &hc->scaling_params.target_rate : NULL);
    ni_bind(st, NI_CH_META_THETA_TAU, (st->meta && hc) ? &hc->meta_params.theta_tau : NULL);
    ni_bind(st, NI_CH_BCM_MAX_THETA,  (st->bcm && hc) ? &hc->meta_params.max_theta : NULL);
    ni_bind(st, NI_CH_STRUCT_FORM,    (st->structural && sc) ? &sc->formation_rate : NULL);
    ni_bind(st, NI_CH_STRUCT_PRUNE,   (st->structural && sc) ? &sc->pruning_rate : NULL);
    ni_bind(st, NI_CH_ELIG_DECAY,     (st->elig && brain->snn_training_ctx)
                                          ? &brain->snn_training_ctx->eligibility_decay : NULL);
    ni_bind(st, NI_CH_ASTRO_CA_THRESH, (st->glio && brain->astrocyte_network)
                                          ? &brain->astrocyte_network->calcium_threshold_um : NULL);

    /* Homeostatic synaptic-scaling target rate. */
    if (st->homeo && homeo_pink_noise_update(st->homeo) == 0) {
        float rel = ni_normalise(&st->ch[NI_CH_HOMEO_TARGET],
            ni_ratio_rel(homeo_pink_noise_get_noisy_target_rate(st->homeo),
                         st->homeo_snapshot.target_rate), mr);
        if (mr > 0.0f) ni_apply(&st->ch[NI_CH_HOMEO_TARGET], rel, 1e-6f, 1e4f);
    }

    /* Metaplasticity: theta sliding time constant. */
    if (st->meta && meta_pink_noise_update(st->meta) == 0) {
        float rel = ni_normalise(&st->ch[NI_CH_META_THETA_TAU],
            ni_ratio_rel(st->meta->noisy_baseline_tau, METAPLASTICITY_DEFAULT_BASELINE_TAU), mr);
        if (mr > 0.0f) ni_apply(&st->ch[NI_CH_META_THETA_TAU], rel, 1.0f, 1e9f);
    }

    /* BCM: sliding-threshold ceiling, activity-dependent noise. */
    if (st->bcm) {
        float act = st->last_activity;
        if (f && n > 0) {
            double acc = 0.0;
            uint32_t cnt = 0;
            for (uint32_t i = 0; i < n; i++) {
                if (isfinite(f[i])) { acc += fabs((double)f[i]); cnt++; }
            }
            act = cnt ? (float)(acc / cnt) : 0.0f;
            if (act > 1.0f) act = 1.0f;
            st->last_activity = act;
        }
        if (bcm_pink_noise_update_with_activity(st->bcm, act) == 0) {
            float rel = ni_normalise(&st->ch[NI_CH_BCM_MAX_THETA],
                ni_ratio_rel(bcm_pink_noise_get_noisy_threshold(st->bcm), 0.5f), mr);
            if (mr > 0.0f && hc) {
                /* keep max_theta strictly above min_theta */
                float lo = hc->meta_params.min_theta * 1.01f + 1e-6f;
                ni_apply(&st->ch[NI_CH_BCM_MAX_THETA], rel, isfinite(lo) ? lo : 1e-6f, 1e6f);
            }
        }
    }

    /* Structural plasticity: formation / pruning rates. */
    if (st->structural && struct_pink_noise_update(st->structural) == 0) {
        float rg = ni_normalise(&st->ch[NI_CH_STRUCT_FORM],
            ni_ratio_rel(struct_pink_noise_get_growth_rate(st->structural), 0.01f), mr);
        float rp = ni_normalise(&st->ch[NI_CH_STRUCT_PRUNE],
            ni_ratio_rel(struct_pink_noise_get_prune_rate(st->structural), 0.005f), mr);
        if (mr > 0.0f) {
            ni_apply(&st->ch[NI_CH_STRUCT_FORM],  rg, 0.0f, 1.0f);
            ni_apply(&st->ch[NI_CH_STRUCT_PRUNE], rp, 0.0f, 1.0f);
        }
    }

    /* Eligibility traces: SNN R-STDP/eProp trace decay rate. */
    if (st->elig && elig_pink_noise_update(st->elig) == 0) {
        float rel = ni_normalise(&st->ch[NI_CH_ELIG_DECAY],
            ni_ratio_rel(elig_pink_noise_get_noisy_decay(st->elig), 0.1f), mr);
        if (mr > 0.0f) ni_apply(&st->ch[NI_CH_ELIG_DECAY], rel, 1e-7f, 1e3f);
    }

    /* Gliotransmission: stronger Ca2+ wave drive == lower propagation threshold. */
    if (st->glio && glio_pink_noise_update(st->glio) == 0) {
        float rel = ni_normalise(&st->ch[NI_CH_ASTRO_CA_THRESH],
            -(glio_pink_noise_get_calcium_wave(st->glio) - 1.0f), mr);
        astrocyte_network_t* an = brain->astrocyte_network;
        if (mr > 0.0f && an && st->ch[NI_CH_ASTRO_CA_THRESH].target) {
            /* trylock: never stall the training thread behind an astro step */
            if (nimcp_mutex_trylock(&an->lock) == NIMCP_SUCCESS) {
                ni_apply(&st->ch[NI_CH_ASTRO_CA_THRESH], rel, 1e-6f, 1e6f);
                nimcp_mutex_unlock(&an->lock);
            }
        }
    }

    /* Spike-frequency adaptation: SNN AHP tau / gain per population. */
    ni_drive_sfa(brain, st, mr);

    st->noise_updates++;
}

/* ------------------------------------------------------------------------- */
/* Immune drive                                                               */
/* ------------------------------------------------------------------------- */
static float ni_feature_entropy(const float* f, uint32_t n) {
    if (!f || n < 4) {
        return -1.0f;
    }
    float lo = INFINITY, hi = -INFINITY;
    for (uint32_t i = 0; i < n; i++) {
        if (!isfinite(f[i])) continue;
        if (f[i] < lo) lo = f[i];
        if (f[i] > hi) hi = f[i];
    }
    if (!(hi > lo)) {
        return 0.0f;                         /* constant input: zero entropy */
    }
    enum { BINS = 16 };
    uint32_t hist[BINS] = {0};
    uint32_t tot = 0;
    float scale = (float)BINS / (hi - lo);
    for (uint32_t i = 0; i < n; i++) {
        if (!isfinite(f[i])) continue;
        int b = (int)((f[i] - lo) * scale);
        if (b < 0) b = 0;
        if (b >= BINS) b = BINS - 1;
        hist[b]++;
        tot++;
    }
    if (!tot) {
        return -1.0f;
    }
    double h = 0.0;
    for (int b = 0; b < BINS; b++) {
        if (!hist[b]) continue;
        double p = (double)hist[b] / tot;
        h -= p * log2(p);
    }
    float hn = (float)(h / log2((double)BINS));
    return isfinite(hn) ? hn : -1.0f;
}

static void ni_drive_immune(struct brain_struct* brain, ni_state_t* st,
                            const float* f, uint32_t n) {
    if (!st->immune_bound || st->immune_bound != brain->immune_system) {
        return;
    }

    if (st->shannon) {
        (void)shannon_immune_apply_cytokine_effects(st->shannon);
        (void)shannon_immune_apply_inflammation_effects(st->shannon);
        float h = ni_feature_entropy(f, n);
        if (h >= 0.0f) {
            st->last_entropy = h;
            (void)shannon_immune_detect_pattern_threat(st->shannon, h);
            (void)shannon_immune_detect_anomaly(st->shannon, h);
        }
        shannon_network_metrics_t sm = brain->last_shannon_metrics;
        if (sm.num_synapses > 0 && isfinite(sm.bottleneck_score)) {
            float cap = sm.bottleneck_score < 0.0f ? 0.0f
                      : (sm.bottleneck_score > 1.0f ? 1.0f : sm.bottleneck_score);
            (void)shannon_immune_trigger_capacity_stress(st->shannon, cap);
        }
    }

    if (st->cross_modal) {
        (void)cross_modal_immune_apply_cytokine_effects(st->cross_modal);
        (void)cross_modal_immune_apply_inflammation_effects(st->cross_modal);
        multi_modal_integration_t cm = brain->last_cross_modal_metrics;
        if (cm.num_modalities >= 2 && cm.num_modalities <= 4) {
            float hmin = INFINITY;
            for (uint32_t i = 0; i < cm.num_modalities; i++) {
                if (isfinite(cm.individual_entropy[i]) && cm.individual_entropy[i] < hmin) {
                    hmin = cm.individual_entropy[i];
                }
            }
            float binding = (isfinite(hmin) && hmin > 1e-6f && isfinite(cm.total_mutual_info))
                          ? cm.total_mutual_info / hmin : 0.0f;
            binding = binding < 0.0f ? 0.0f : (binding > 1.0f ? 1.0f : binding);
            float eff = isfinite(cm.integration_efficiency) ? cm.integration_efficiency : 0.0f;
            eff = eff < 0.0f ? 0.0f : (eff > 1.0f ? 1.0f : eff);
            (void)cross_modal_immune_detect_binding_failure(st->cross_modal, binding);
            (void)cross_modal_immune_detect_mismatch(st->cross_modal, eff);
            (void)cross_modal_immune_trigger_bottleneck_stress(st->cross_modal, eff);
        }
    }

    if (st->gpu) {
        (void)gpu_neuron_immune_update(st->gpu);   /* cytokine path; GPU-network parts skipped */
        if (brain->gpu_ctx) {
            size_t alloc = 0, peak = 0;
            nimcp_gpu_memory_stats(brain->gpu_ctx, &alloc, &peak, NULL);  /* no CUDA call */
            st->gpu->error_state.memory_used_bytes = (uint64_t)alloc;
            const char* msg = nimcp_gpu_context_get_error(brain->gpu_ctx);
            if (msg && msg[0] && strcmp(msg, "No error") != 0 &&
                strncmp(msg, st->gpu_last_err, sizeof(st->gpu_last_err) - 1) != 0) {
                snprintf(st->gpu_last_err, sizeof(st->gpu_last_err), "%s", msg);
                if (gpu_neuron_immune_trigger_error_response(st->gpu, -1, msg) == 0) {
                    st->gpu_errors_reported++;
                }
            }
        }
    }

    if (st->qa) {
        if (qa_immune_update(st->qa) == 0) {
            (void)qa_immune_apply_modulation(st->qa);
        }
    }

    st->immune_updates++;
}

/* ------------------------------------------------------------------------- */
/* Lifecycle                                                                  */
/* ------------------------------------------------------------------------- */
int brain_wired_noise_immune_create(struct brain_struct* brain) {
    if (!brain) {
        return -1;
    }
    if (brain->wired[BRAIN_WIRED_NOISE_IMMUNE]) {
        return 0;
    }
    ni_state_t* st = (ni_state_t*)nimcp_calloc(1, sizeof(ni_state_t));
    if (!st) {
        return -1;
    }
    st->max_rel = ni_env_max_rel();
    ni_ensure_noise_bridges(brain, st);
    ni_ensure_immune_bridges(brain, st);
    brain->wired[BRAIN_WIRED_NOISE_IMMUNE] = st;
    fprintf(stderr, "[WIRED] NOISE_IMMUNE: noise sfa=%d glio=%d bcm=%d elig=%d homeo=%d meta=%d struct=%d "
            "max_rel=%.3f | immune=%s shannon=%d cross_modal=%d gpu=%d qa=%d\n",
            st->sfa != NULL, st->glio != NULL, st->bcm != NULL, st->elig != NULL,
            st->homeo != NULL, st->meta != NULL, st->structural != NULL, (double)st->max_rel,
            brain->immune_system ? "on" : "off",
            st->shannon != NULL, st->cross_modal != NULL, st->gpu != NULL, st->qa != NULL);
    return 0;
}

void brain_wired_noise_immune_tick(struct brain_struct* brain, const float* f, uint32_t n, float dt_ms) {
    if (!brain) {
        return;
    }
    ni_state_t* st = (ni_state_t*)brain->wired[BRAIN_WIRED_NOISE_IMMUNE];
    if (!st) {
        return;
    }
    st->ticks++;
    if (isfinite(dt_ms) && dt_ms > 0.0f) {
        st->immune_dt_acc_ms += dt_ms;
    }

    if ((st->ticks % NI_ENSURE_EVERY) == 0) {
        ni_ensure_noise_bridges(brain, st);
        ni_ensure_immune_bridges(brain, st);
    }
    if ((st->ticks % NI_NOISE_EVERY) == 0) {
        ni_drive_noise(brain, st, f, n);
    }
    if ((st->ticks % NI_IMMUNE_EVERY) == 0) {
        ni_drive_immune(brain, st, f, n);
        st->immune_dt_acc_ms = 0.0f;
    }
}

/* Sidecar: magic, version, n_channels, per channel {init, base}. */
static void ni_sidecar_path(char* out, size_t cap, const char* path) {
    snprintf(out, cap, "%s.wired_noise_immune", path);
}

int brain_wired_noise_immune_save(struct brain_struct* brain, const char* path) {
    if (!brain || !path) {
        return -1;
    }
    ni_state_t* st = (ni_state_t*)brain->wired[BRAIN_WIRED_NOISE_IMMUNE];
    if (!st) {
        return 0;
    }
    char p[1024];
    ni_sidecar_path(p, sizeof(p), path);
    char tmp[1040];
    snprintf(tmp, sizeof(tmp), "%s.tmp", p);
    FILE* fp = fopen(tmp, "wb");
    if (!fp) {
        return -1;
    }
    uint32_t hdr[3] = { NI_SIDE_MAGIC, NI_SIDE_VERSION, NI_CH_COUNT };
    int ok = fwrite(hdr, sizeof(hdr), 1, fp) == 1;
    for (int i = 0; ok && i < NI_CH_COUNT; i++) {
        const ni_chan_t* c = &st->ch[i];
        uint8_t valid = c->init ? 1 : (st->pending_valid[i] ? 1 : 0);
        float base = c->init ? c->base : st->pending_base[i];
        ok = fwrite(&valid, 1, 1, fp) == 1 && fwrite(&base, sizeof(base), 1, fp) == 1;
    }
    /* SFA: per-population baselines (index-matched on load). */
    uint32_t np = SNN_MAX_POPULATIONS;
    ok = ok && fwrite(&np, sizeof(np), 1, fp) == 1;
    for (uint32_t i = 0; ok && i < np; i++) {
        const ni_sfa_pop_t* sp = &st->sfa_pop[i];
        uint8_t valid = (sp->tau.init && sp->gain.init) ? 1 : 0;
        float b[2] = { sp->tau.base, sp->gain.base };
        ok = fwrite(&valid, 1, 1, fp) == 1 && fwrite(b, sizeof(b), 1, fp) == 1;
    }
    if (fclose(fp) != 0) {
        ok = 0;
    }
    if (!ok || rename(tmp, p) != 0) {
        remove(tmp);
        return -1;
    }
    return 0;
}

int brain_wired_noise_immune_load(struct brain_struct* brain, const char* path) {
    if (!brain || !path) {
        return -1;
    }
    ni_state_t* st = (ni_state_t*)brain->wired[BRAIN_WIRED_NOISE_IMMUNE];
    if (!st) {
        return 1;
    }
    char p[1024];
    ni_sidecar_path(p, sizeof(p), path);
    FILE* fp = fopen(p, "rb");
    if (!fp) {
        return 1;                                   /* fresh state */
    }
    uint32_t hdr[3] = {0};
    if (fread(hdr, sizeof(hdr), 1, fp) != 1 || hdr[0] != NI_SIDE_MAGIC ||
        hdr[1] != NI_SIDE_VERSION || hdr[2] != NI_CH_COUNT) {
        fclose(fp);
        return -1;
    }
    float base[NI_CH_COUNT];
    uint8_t valid[NI_CH_COUNT];
    for (int i = 0; i < NI_CH_COUNT; i++) {
        if (fread(&valid[i], 1, 1, fp) != 1 || fread(&base[i], sizeof(float), 1, fp) != 1) {
            fclose(fp);
            return -1;
        }
    }
    uint32_t np = 0;
    if (fread(&np, sizeof(np), 1, fp) != 1 || np > SNN_MAX_POPULATIONS) {
        fclose(fp);
        return -1;
    }
    float sfa_b[SNN_MAX_POPULATIONS][2];
    uint8_t sfa_v[SNN_MAX_POPULATIONS];
    memset(sfa_v, 0, sizeof(sfa_v));
    for (uint32_t i = 0; i < np; i++) {
        if (fread(&sfa_v[i], 1, 1, fp) != 1 || fread(sfa_b[i], sizeof(float), 2, fp) != 2) {
            fclose(fp);
            return -1;
        }
    }
    fclose(fp);

    /* Scalar channels: either fix the already-bound channel now, or park the
     * baseline until the channel binds. Only accept a sidecar baseline when
     * the live value is within our own noise band of it (otherwise the live
     * value was changed deliberately and becomes the baseline). */
    for (int i = 0; i < NI_CH_COUNT; i++) {
        if (!valid[i] || !isfinite(base[i])) {
            continue;
        }
        ni_chan_t* c = &st->ch[i];
        if (c->target) {
            float cur = ni_load(c->target);
            if (isfinite(cur) && fabsf(cur - base[i]) <= 2.5f * NI_MAX_REL_CEIL * fabsf(base[i])) {
                c->base = base[i];
                c->init = 1;
                c->wrote = 0;
                c->base_from_sidecar = 1;
                ni_store(c->target, base[i]);
            }
        } else {
            st->pending_base[i] = base[i];
            st->pending_valid[i] = 1;
        }
    }

    /* SFA populations (bind now if the SNN exists). */
    struct snn_network_s* net = brain->snn_network;
    if (net && net->populations) {
        uint32_t live = net->n_populations;
        if (live > np) live = np;
        if (live > net->populations_capacity) live = net->populations_capacity;
        for (uint32_t i = 0; i < live; i++) {
            snn_population_t* pop = net->populations[i];
            if (!sfa_v[i] || !pop || !pop->ahp) continue;
            ni_sfa_pop_t* sp = &st->sfa_pop[i];
            memset(sp, 0, sizeof(*sp));
            sp->pop = pop;
            sp->tau.target = &pop->ahp->tau_ms;
            sp->gain.target = &pop->ahp->gain_mv;
            float bt = sfa_b[i][0], bg = sfa_b[i][1];
            float ct = ni_load(sp->tau.target), cg = ni_load(sp->gain.target);
            if (isfinite(bt) && isfinite(ct) && bt > 0.0f &&
                fabsf(ct - bt) <= 2.5f * NI_MAX_REL_CEIL * bt) {
                sp->tau.base = bt; sp->tau.init = 1; sp->tau.base_from_sidecar = 1;
                ni_store(sp->tau.target, bt);
            }
            if (isfinite(bg) && isfinite(cg) && bg >= 0.0f &&
                fabsf(cg - bg) <= 2.5f * NI_MAX_REL_CEIL * fabsf(bg)) {
                sp->gain.base = bg; sp->gain.init = 1; sp->gain.base_from_sidecar = 1;
                ni_store(sp->gain.target, bg);
            }
        }
    }
    return 0;
}

void brain_wired_noise_immune_destroy(struct brain_struct* brain) {
    if (!brain) {
        return;
    }
    ni_state_t* st = (ni_state_t*)brain->wired[BRAIN_WIRED_NOISE_IMMUNE];
    brain->wired[BRAIN_WIRED_NOISE_IMMUNE] = NULL;
    if (!st) {
        return;
    }
    /* Put un-noised baselines back (subsystems are still alive here). Re-check
     * that each channel still points at the subsystem's current field. */
    homeostatic_config_t* hc = ni_homeo_cfg(brain);
    structural_plasticity_config_t* sc = ni_struct_cfg(brain);
    float* live_ptr[NI_CH_COUNT] = {
        hc ? &hc->scaling_params.target_rate : NULL,
        hc ? &hc->meta_params.theta_tau : NULL,
        hc ? &hc->meta_params.max_theta : NULL,
        sc ? &sc->formation_rate : NULL,
        sc ? &sc->pruning_rate : NULL,
        brain->snn_training_ctx ? &brain->snn_training_ctx->eligibility_decay : NULL,
        brain->astrocyte_network ? &brain->astrocyte_network->calcium_threshold_um : NULL,
    };
    for (int i = 0; i < NI_CH_COUNT; i++) {
        if (st->ch[i].target && st->ch[i].target == live_ptr[i]) {
            ni_restore(&st->ch[i]);
        }
    }
    struct snn_network_s* net = brain->snn_network;
    if (net && net->populations) {
        uint32_t np = net->n_populations;
        if (np > SNN_MAX_POPULATIONS) np = SNN_MAX_POPULATIONS;
        if (np > net->populations_capacity) np = net->populations_capacity;
        for (uint32_t i = 0; i < np; i++) {
            snn_population_t* pop = net->populations[i];
            ni_sfa_pop_t* sp = &st->sfa_pop[i];
            if (!pop || !pop->ahp || sp->pop != (const void*)pop) continue;
            if (sp->tau.target == &pop->ahp->tau_ms)   ni_restore(&sp->tau);
            if (sp->gain.target == &pop->ahp->gain_mv) ni_restore(&sp->gain);
        }
    }

    if (st->sfa)        sfa_pink_noise_destroy(st->sfa);
    if (st->glio)       glio_pink_noise_destroy(st->glio);
    if (st->bcm)        bcm_pink_noise_destroy(st->bcm);
    if (st->elig)       elig_pink_noise_destroy(st->elig);
    if (st->homeo)      homeo_pink_noise_destroy(st->homeo);
    if (st->meta)       meta_pink_noise_destroy(st->meta);
    if (st->structural) struct_pink_noise_destroy(st->structural);
    ni_destroy_immune_bridges(st);
    nimcp_free(st);
}

int brain_wired_noise_immune_status_json(struct brain_struct* brain, char* buf, uint32_t cap) {
    if (!buf || cap == 0) {
        return 0;
    }
    ni_state_t* st = brain ? (ni_state_t*)brain->wired[BRAIN_WIRED_NOISE_IMMUNE] : NULL;
    if (!st) {
        return snprintf(buf, cap, "null");
    }
    uint32_t off = 0;
#define NI_PUT(...) do {                                                        \
        if (off < cap) {                                                        \
            int w_ = snprintf(buf + off, cap - off, __VA_ARGS__);               \
            if (w_ > 0) off += ((uint32_t)w_ < cap - off) ? (uint32_t)w_ : cap - off - 1; \
        }                                                                       \
    } while (0)

    NI_PUT("{\"ticks\":%llu,\"noise_updates\":%llu,\"max_rel\":%.4f,\"bridges\":{"
           "\"sfa\":%s,\"glio\":%s,\"bcm\":%s,\"elig\":%s,\"homeo\":%s,\"meta\":%s,\"struct\":%s},"
           "\"channels\":{",
           (unsigned long long)st->ticks, (unsigned long long)st->noise_updates, (double)st->max_rel,
           st->sfa ? "true" : "false", st->glio ? "true" : "false", st->bcm ? "true" : "false",
           st->elig ? "true" : "false", st->homeo ? "true" : "false", st->meta ? "true" : "false",
           st->structural ? "true" : "false");
    for (int i = 0; i < NI_CH_COUNT; i++) {
        const ni_chan_t* c = &st->ch[i];
        NI_PUT("%s\"%s\":{\"bound\":%s,\"base\":%.6g,\"applied_rel\":%.5f,\"writes\":%llu,\"sidecar\":%s}",
               i ? "," : "", k_ch_names[i], c->target ? "true" : "false",
               (double)(c->init ? c->base : 0.0f), (double)c->applied,
               (unsigned long long)c->writes, c->base_from_sidecar ? "true" : "false");
    }
    uint32_t sfa_bound = 0;
    uint64_t sfa_writes = 0;
    for (uint32_t i = 0; i < SNN_MAX_POPULATIONS; i++) {
        if (st->sfa_pop[i].tau.target) {
            sfa_bound++;
            sfa_writes += st->sfa_pop[i].tau.writes + st->sfa_pop[i].gain.writes;
        }
    }
    NI_PUT("},\"sfa\":{\"pops_bound\":%u,\"writes\":%llu,\"tau_rel\":%.5f,\"gain_rel\":%.5f}",
           sfa_bound, (unsigned long long)sfa_writes,
           (double)st->sfa_rel_tau.applied, (double)st->sfa_rel_gain.applied);

    NI_PUT(",\"immune\":{\"on\":%s,\"updates\":%llu,\"feature_entropy\":%.4f",
           st->immune_bound ? "true" : "false", (unsigned long long)st->immune_updates,
           (double)st->last_entropy);
    if (st->shannon) {
        NI_PUT(",\"shannon\":{\"capacity_factor\":%.4f,\"capacity_deficit\":%s}",
               (double)shannon_immune_get_capacity_factor(st->shannon),
               shannon_immune_has_capacity_deficit(st->shannon) ? "true" : "false");
    }
    if (st->cross_modal) {
        NI_PUT(",\"cross_modal\":{\"efficiency_factor\":%.4f,\"binding_strength\":%.4f,\"binding_deficit\":%s}",
               (double)cross_modal_immune_get_efficiency_factor(st->cross_modal),
               (double)cross_modal_immune_get_binding_strength(st->cross_modal),
               cross_modal_immune_has_binding_deficit(st->cross_modal) ? "true" : "false");
    }
    if (st->gpu) {
        NI_PUT(",\"gpu\":{\"batch_factor\":%.4f,\"throttled\":%s,\"errors_reported\":%llu}",
               (double)gpu_neuron_immune_get_batch_factor(st->gpu),
               gpu_neuron_immune_is_throttled(st->gpu) ? "true" : "false",
               (unsigned long long)st->gpu_errors_reported);
    }
    if (st->qa) {
        NI_PUT(",\"quantum_annealing\":{\"temp_factor\":%.4f,\"iter_factor\":%.4f,\"phase\":\"%s\"}",
               (double)qa_immune_get_temp_factor(st->qa), (double)qa_immune_get_iter_factor(st->qa),
               qa_immune_phase_to_string(qa_immune_get_phase(st->qa)));
    }
    NI_PUT("}}");
#undef NI_PUT
    return (int)off;
}
