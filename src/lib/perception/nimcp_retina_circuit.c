/**
 * @file nimcp_retina_circuit.c
 * @brief Biologically structured retina from per-cell base modules.
 *
 * WHAT: photoreceptor mosaic (rods + L/M/S cones) -> horizontal cells ->
 *       ON/OFF bipolars (midget, diffuse, rod) -> amacrines (AII, wide-field)
 *       -> ganglion cells (midget/parasol, ON/OFF) -> optic nerve -> LGN.
 * WHY:  Athena's vision needs the retina's computations (light adaptation,
 *       center-surround, ON/OFF, colour opponency, scotopic/photopic) done by
 *       a retina, from the project's own cell modules, not by a filter bank.
 * HOW:  1 ms steps. Every chemical synapse is vesicle release into a
 *       neurotransmitter adapter site (chemistry layer), whose receptor
 *       occupancy, normalised by the occupancy of a maximally driven site,
 *       is the postsynaptic activation s in [0,1]. Graded cells integrate
 *       conductances; spiking cells are Izhikevich neuron models; ganglion
 *       inputs pass through dendritic spines; astrocytes sense and modulate
 *       bipolar->ganglion synapses; ganglion spikes travel the optic nerve.
 *
 * CELL MODELS
 *  Photoreceptor (graded): excitation E = illuminance * (spectral weights .
 *    linear RGB), Naka-Rushton R = E / (E + sigma), sigma = sigma0 + A where A
 *    is a slow running mean of E (Weber light adaptation; rods may only adapt
 *    up to ROD_ADAPT_MAX*sigma0, so they saturate in daylight). A first-order
 *    phototransduction filter gives r(t); V = -40 mV - 30 mV * r. Rods are
 *    100x more sensitive than cones. Gap junctions blur V between neighbours
 *    (rod-rod > rod-cone > cone-cone). Glutamate release from the cone
 *    terminal is a sigmoid of (V - FB_GAIN * horizontal GABA activation): a
 *    dark (depolarised) horizontal network suppresses release, a light one
 *    disinhibits it, which is the surround. Rods have no feedback here.
 *  Horizontal (graded): AMPA input from surrounding cones, strong H-H gap
 *    coupling (space constant ~2 cell spacings, so the surround is wider than
 *    any centre), GABA release back onto cone terminals.
 *  Bipolar (graded): OFF = AMPA/kainate (sign-preserving, g = G*s); ON and rod
 *    bipolars = mGluR6 (sign-inverting, g = G*(1-s)). GABA from wide-field
 *    amacrines on all terminals, glycine from AII on OFF terminals, AII gap
 *    junctions on ON cone bipolars. Ribbon release depresses (plasticity/stp,
 *    one stp_state_t per ribbon: diffuse = transient, midget = sustained, rod
 *    bipolar = deep depletion under daylight drive, which mutes the rod
 *    pathway in photopic light).
 *  AII amacrine (graded): rod-bipolar glutamate in; gap junctions to AII
 *    neighbours and ON cone bipolars; glycine out to OFF bipolars.
 *  Wide-field amacrine (spiking, Izhikevich FS): ON-OFF transient — each
 *    diffuse-bipolar input counts only its rise above its own 300 ms mean
 *    (desensitisation); GABA out to bipolar terminals and ganglion cells.
 *  Ganglion (spiking, Izhikevich): bipolar glutamate through core/dendrite
 *    spines, cable attenuation per dendritic segment, GABA inhibition, noise.
 *
 *  The gains and half-activation voltages below were hand-calibrated against
 *  the smoke test (maintained rates 15-35 Hz, ON/OFF balance, centre >
 *  uniform at moderate contrast); they are not fitted to recordings.
 *
 * BASE MODULES (all used as-is; the 2026-11-23 upstream fixes removed every
 * workaround this file used to carry):
 *  - nimcp_nt_adapter_*: created ready to use, advanced with
 *    nimcp_nt_adapter_step(dt_ms); per-transmitter Kd / Hill / clearance, so
 *    each transmitter's activation is normalised by its own calibration
 *    (occupancy of a site released at R_MAX). Release is whole vesicles so
 *    each one can drive the ribbon's STP state.
 *  - dendrite_spine_process_input(): real AMPA/NMDA conductances. Our drive
 *    is graded every 1 ms, so spines are AMPA-only (nmda_receptors = 0;
 *    otherwise NMDA integrates to its cap). Cable attenuation per segment is
 *    dendrite_get_attenuation(). dendrite_step()/receive_input() are not used:
 *    the spine currents are summed at the soma directly.
 *  - astrocyte_update_calcium(): graded Li-Rinzel, pool refilled upstream.
 *  - neuron_model_get_lif_vtable() exists now; Izhikevich is kept because the
 *    midget/parasol firing differences use its adaptation parameters.
 */

#include "perception/nimcp_retina_circuit.h"
#include "perception/nimcp_optic_nerve.h"
#include "core/neuron_models/nimcp_neuron_model.h"
#include "core/neuron_models/nimcp_izhikevich.h"
#include "core/dendrite/nimcp_dendrite.h"
#include "integration/adapters/chemistry/nimcp_neurotransmitter_adapter.h"
#include "plasticity/stp/nimcp_stp.h"
#include "glial/astrocytes/nimcp_astrocytes.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(retina_circuit)

//=============================================================================
// Constants
//=============================================================================

/* Release sites (ribbons / boutons) per cell in the neurotransmitter adapters */
#define PR_RIBBONS  8
#define H_SITES     4
#define BC_RIBBONS  4
#define AII_SITES   4
#define AM_SITES    2

#define R_MAX       0.08f   /* max sustained release, vesicles/ms/site (pool refill 0.1) */

/* Photoreceptors */
#define CONE_SIGMA0     1.0f
#define ROD_SIGMA0      0.01f   /* rods ~100x more sensitive */
#define ROD_ADAPT_MAX   4.0f    /* rods adapt at most to (1+4) sigma0, then saturate */
#define CONE_TAU_ADAPT  300.0f
#define ROD_TAU_ADAPT   1000.0f
#define CONE_TAU_R      10.0f
#define ROD_TAU_R       50.0f
#define PR_V_DARK       (-40.0f)
#define PR_V_SAT        (-70.0f)
#define PR_V_HALF       (-65.0f)  /* cone terminal, includes the H-feedback offset */
#define ROD_V_HALF      (-48.0f)  /* rod spherule: no H feedback in this model */
#define PR_V_SLOPE      4.0f
#define FB_GAIN_MV      28.0f   /* horizontal GABA feedback on the cone terminal */
#define GAP_ROD_ROD     0.05f
#define GAP_ROD_CONE    0.08f
#define GAP_CONE_CONE   0.01f

/* Horizontal cells */
#define H_TAU    20.0f
#define H_EL     (-70.0f)
#define H_GE     2.5f
#define H_GGAP   20.0f   /* strong H-H coupling: the surround is wider than any centre */
#define H_SUBSTEPS 10    /* explicit Euler needs dt*(g_L+g_e+4 g_gap)/tau < 1 */
#define H_V_HALF (-35.0f)
#define H_SLOPE  3.0f

/* Bipolar cells */
#define BC_TAU      10.0f
#define BC_EL       (-60.0f)
#define BC_G_OFF    1.2f
#define BC_G_ON     1.2f
#define BC_G_ROD    1.2f
#define BC_G_AM     0.5f
#define BC_G_GLY    0.15f
#define BC_G_AII    0.7f
#define BC_V_HALF   (-40.0f)
#define BC_V_HALF_OFF (-48.0f)  /* calibrated so ON and OFF have similar maintained release */
#define BC_V_HALF_ROD (-46.0f)  /* rod bipolars: high gain for single-photon-range signals */
#define BC_SLOPE    4.0f

/* AII amacrine */
#define AII_TAU    10.0f
#define AII_EL     (-60.0f)
#define AII_GE     4.0f
#define AII_GGAP   0.3f
#define AII_V_HALF (-45.0f)
#define AII_SLOPE  4.0f

/* Wide-field amacrine and ganglion cells (Izhikevich current units) */
#define AM_G_IN     60.0f
#define AM_BIAS     (0.0f)
#define AM_DESENS_TAU 300.0f
#define AM_NOISE_FLOOR 0.04f   /* rises smaller than this (quantal release noise) are ignored */
#define RGC_G_EXC_MIDGET  60.0f
#define RGC_G_EXC_PARASOL 35.0f
#define RGC_BIAS_MIDGET  (-34.0f)
#define RGC_BIAS_PARASOL (-15.0f)
#define RGC_G_INH   8.0f

/* Astrocytes */
#define ASTRO_PERIOD_MS 5
#define ASTRO_STIM_GAIN 30.0f
#define ASTRO_UPTAKE_K  0.3f
#define ASTRO_GLIO_K    0.25f


#define RETINA_MAGIC   0x52544E41u  /* "RTNA" */
#define RETINA_VERSION 1u

enum { BC_MIDGET_ON = 0, BC_MIDGET_OFF, BC_DIFFUSE_ON, BC_DIFFUSE_OFF, BC_ROD };

//=============================================================================
// Small helpers
//=============================================================================

/** Compressed sparse rows: weighted input links per cell. */
typedef struct {
    uint32_t n;       /**< rows */
    uint32_t* off;    /**< [n+1] */
    uint32_t* idx;    /**< [nnz] source index */
    float* w;         /**< [nnz] weight */
} rcsr_t;

static void csr_free(rcsr_t* c)
{
    nimcp_free(c->off);
    nimcp_free(c->idx);
    nimcp_free(c->w);
    memset(c, 0, sizeof(*c));
}

static inline float sq(float a) { return a * a; }
static inline float sigm(float x) { return 1.0F / (1.0F + expf(-x)); }

/**
 * Build links from every destination to the sources within `radius`
 * (Gaussian weights, sigma), falling back to the `min_k` nearest eligible
 * sources when none are in range. `self_pop` skips i == j (same population).
 * `normalize` scales each row to sum 1; otherwise weights are `gconst`.
 */
static int csr_build(rcsr_t* c, uint32_t nd, const float* dx, const float* dy,
                     uint32_t ns, const float* sx, const float* sy, const uint8_t* ok,
                     float radius, float sigma, uint32_t min_k, bool normalize,
                     bool self_pop, float gconst)
{
    memset(c, 0, sizeof(*c));
    c->n = nd;
    c->off = (uint32_t*)nimcp_calloc(nd + 1, sizeof(uint32_t));
    uint32_t cap = 64, nnz = 0;
    c->idx = (uint32_t*)nimcp_malloc(cap * sizeof(uint32_t));
    c->w = (float*)nimcp_malloc(cap * sizeof(float));
    uint8_t* used = (uint8_t*)nimcp_calloc(ns ? ns : 1, 1);
    if (!c->off || !c->idx || !c->w || !used) { nimcp_free(used); csr_free(c); return -1; }

    for (uint32_t i = 0; i < nd; i++) {
        c->off[i] = nnz;
        uint32_t row0 = nnz;
        memset(used, 0, ns ? ns : 1);
        for (uint32_t pass = 0; pass < 2; pass++) {
            uint32_t want = (pass == 0) ? UINT32_MAX : min_k;
            if (pass == 1 && nnz - row0 >= min_k) break;
            for (uint32_t k = (pass == 0) ? 0 : (nnz - row0); k < want; k++) {
                int32_t best = -1;
                float bd = INFINITY;
                for (uint32_t j = 0; j < ns; j++) {
                    if ((ok && !ok[j]) || used[j] || (self_pop && i == j)) continue;
                    float d2 = sq(dx[i] - sx[j]) + sq(dy[i] - sy[j]);
                    if (pass == 0) {
                        if (d2 <= radius * radius) { best = (int32_t)j; bd = d2; break; }
                    } else if (d2 < bd) { bd = d2; best = (int32_t)j; }
                }
                if (best < 0) break;
                used[best] = 1;
                if (nnz == cap) {
                    cap *= 2;
                    uint32_t* ni = (uint32_t*)nimcp_realloc(c->idx, cap * sizeof(uint32_t));
                    if (ni) c->idx = ni;
                    float* nw = (float*)nimcp_realloc(c->w, cap * sizeof(float));
                    if (nw) c->w = nw;
                    if (!ni || !nw) { nimcp_free(used); csr_free(c); return -1; }
                }
                c->idx[nnz] = (uint32_t)best;
                c->w[nnz] = normalize ? expf(-bd / (2.0F * sigma * sigma)) : gconst;
                nnz++;
            }
        }
        if (normalize) {
            float s = 0.0F;
            for (uint32_t k = row0; k < nnz; k++) s += c->w[k];
            if (s > 0.0F) for (uint32_t k = row0; k < nnz; k++) c->w[k] /= s;
        }
    }
    c->off[nd] = nnz;
    nimcp_free(used);
    return 0;
}

/** Weighted mean of s over a row. */
static inline float csr_dot(const rcsr_t* c, uint32_t i, const float* s)
{
    float a = 0.0F;
    for (uint32_t k = c->off[i]; k < c->off[i + 1]; k++) a += c->w[k] * s[c->idx[k]];
    return a;
}

/** xorshift64* + Box-Muller (per-retina RNG: no shared state). */
static inline uint64_t rng_next(uint64_t* s)
{
    *s ^= *s >> 12; *s ^= *s << 25; *s ^= *s >> 27;
    return *s * 2685821657736338717ULL;
}
static inline float rng_unif(uint64_t* s) { return (float)((rng_next(s) >> 40) + 0.5) / 16777216.0F; }
static inline float rng_gauss(uint64_t* s)
{
    float u1 = rng_unif(s), u2 = rng_unif(s);
    return sqrtf(-2.0F * logf(u1)) * cosf(6.2831853F * u2);
}

static double wall_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e3 + (double)ts.tv_nsec / 1e6;
}

//=============================================================================
// Neurotransmitter release pool (one adapter per transmitter/population)
//=============================================================================

typedef struct {
    nimcp_nt_adapter_t ad;
    uint32_t n_sites;
    float* acc;        /**< fractional release accumulator per site */
    float* occ;        /**< receptor occupancy per site (last update) */
} nt_pool_t;

static int nt_pool_init(nt_pool_t* p, uint32_t n_sites, neurotransmitter_type_t nt, uint64_t* rng)
{
    memset(p, 0, sizeof(*p));
    nimcp_nt_adapter_config_t c = nimcp_nt_adapter_default_config();
    c.num_synapses = n_sites;
    c.default_nt = nt;
    p->ad = nimcp_nt_adapter_create(&c);
    if (!p->ad) return -1;
    p->n_sites = n_sites;
    p->acc = (float*)nimcp_calloc(n_sites, sizeof(float));
    p->occ = (float*)nimcp_calloc(n_sites, sizeof(float));
    if (!p->acc || !p->occ) return -1;
    /* random phase so the sites of one cell do not release in lock-step */
    for (uint32_t i = 0; i < n_sites; i++) p->acc[i] = rng ? rng_unif(rng) : 0.0F;
    return 0;
}

static void nt_pool_free(nt_pool_t* p)
{
    if (p->ad) nimcp_nt_adapter_destroy(p->ad);
    nimcp_free(p->acc);
    nimcp_free(p->occ);
    memset(p, 0, sizeof(*p));
}

/** Accumulate rate*dt at a site; release whole vesicles. Returns vesicles released. */
static inline uint32_t nt_release(nt_pool_t* p, uint32_t site, float rate, float dt_ms)
{
    p->acc[site] += rate * dt_ms;
    if (p->acc[site] < 1.0F) return 0;
    uint32_t n = (uint32_t)p->acc[site];
    p->acc[site] -= (float)n;
    nimcp_nt_adapter_release(p->ad, (int)site, n);
    return n;
}

static void nt_pool_advance(nt_pool_t* p, float dt_ms)
{
    nimcp_nt_adapter_step(p->ad, dt_ms);    /* exact-exponential kinetics, any dt */
    uint32_t cnt = 0;
    nimcp_nt_adapter_get_occupancy(p->ad, p->occ, p->n_sites, &cnt);
}

/** Mean occupancy of `per` consecutive sites of cell i, normalised by occ_sat. */
static inline float nt_cell_s(const nt_pool_t* p, uint32_t i, uint32_t per, float inv_sat)
{
    float a = 0.0F;
    const float* o = p->occ + (size_t)i * per;
    for (uint32_t k = 0; k < per; k++) a += o[k];
    a *= inv_sat / (float)per;
    return a > 1.5F ? 1.5F : a;
}

/** Occupancy of a site released at R_MAX in steady state (normaliser). */
static float nt_calibrate_occ_sat(neurotransmitter_type_t nt)
{
    nt_pool_t p;
    if (nt_pool_init(&p, 1, nt, NULL) != 0) { nt_pool_free(&p); return 0.2F; }
    double sum = 0.0;
    int n = 0;
    for (int t = 0; t < 600; t++) {
        nt_release(&p, 0, R_MAX, 1.0F);
        nt_pool_advance(&p, 1.0F);
        if (t >= 200) { sum += p.occ[0]; n++; }
    }
    nt_pool_free(&p);
    float s = (float)(sum / n);
    return s > 1e-4F ? s : 0.2F;
}

/** Steady somatic current of a STUBBY, AMPA-only spine driven with graded
 *  input 1.0 every ms (normaliser: drive -> fraction of maximal current). */
static float spine_calibrate(void)
{
    dendrite_config_t dc;
    memset(&dc, 0, sizeof(dc));
    dc.total_length = 20.0F;
    dc.mean_diameter = 1.0F;
    dendrite_t* d = dendrite_create(&dc);
    if (!d) return 1.0F;
    segment_config_t sc;
    memset(&sc, 0, sizeof(sc));
    sc.parent_segment = UINT32_MAX;
    sc.length = 20.0F;
    sc.diameter = 1.0F;
    sc.path_distance = 10.0F;
    float out = 1.0F;
    if (dendrite_create_segments(d, 1, &sc)) {
        uint32_t sp = dendrite_add_spine(d, 0, SPINE_TYPE_STUBBY, 0);
        if (sp != UINT32_MAX) {
            d->spines[sp].nmda_receptors = 0.0F;
            for (int i = 0; i < 400; i++) out = dendrite_spine_process_input(d, sp, 1.0F, (uint64_t)i * 1000);
        }
    }
    dendrite_destroy(d);
    return out > 1e-9F ? out : 1.0F;
}

//=============================================================================
// Retina structure
//=============================================================================

struct retina_circuit {
    retina_circuit_config_t cfg;
    uint64_t rng;
    uint64_t t_us;
    float lin_lut[256];
    float occ_inv_sat;
    float occ_inv_sat_gaba, occ_inv_sat_gly;
    float spine_inv_gain;

    /* --- photoreceptors --- */
    uint32_t n_pr;
    uint8_t* pr_type;
    float *pr_x, *pr_y;
    float* pr_E;        /**< excitation of the current frame */
    float* pr_A;        /**< adaptation state */
    float* pr_r;        /**< phototransduction output 0..1 */
    float* pr_V;        /**< soma voltage (mV) */
    float* pr_Vtmp;
    float* pr_r_acc;    /**< sum of r over the call */
    rcsr_t pr_gap;      /**< weights = coupling */
    rcsr_t pr_fb;       /**< cone <- horizontal GABA feedback */
    nt_pool_t pr_glu;
    float* s_pr;

    /* --- horizontal --- */
    uint32_t n_h;
    float *h_x, *h_y, *h_V, *h_Vtmp;
    rcsr_t h_in, h_gap;
    nt_pool_t h_gaba;
    float* s_h;

    /* --- bipolar --- */
    uint32_t n_bc, n_midget, n_diffuse, n_rodbc;
    uint8_t* bc_type;
    uint8_t* bc_cone;   /**< centre cone type (midget) */
    float *bc_x, *bc_y, *bc_V;
    rcsr_t bc_in;       /**< from photoreceptors */
    rcsr_t bc_am;       /**< GABA from wide-field amacrines */
    rcsr_t bc_gly;      /**< glycine from AII (OFF only) */
    rcsr_t bc_aii;      /**< gap to AII (ON cone only), w = conductance */
    stp_state_t* bc_stp;/**< [n_bc * BC_RIBBONS] */
    nt_pool_t bc_glu;
    float* s_bc;
    float* bc_sbar;     /**< slow mean of s_bc (amacrine desensitisation) */
    float* bc_gain;     /**< astrocyte modulation of each bipolar's output */

    /* --- AII --- */
    uint32_t n_aii;
    float *aii_x, *aii_y, *aii_V, *aii_Vtmp, *aii_Igap;
    rcsr_t aii_in, aii_gap;
    nt_pool_t aii_gly;
    float* s_aii;

    /* --- wide-field amacrine --- */
    uint32_t n_am;
    float *am_x, *am_y;
    neuron_model_state_t* am_nm;
    rcsr_t am_in;
    nt_pool_t am_gaba;
    float* s_am;
    uint32_t* am_spikes;

    /* --- ganglion --- */
    uint32_t n_rgc;
    uint8_t* rgc_type;
    uint8_t* rgc_cone;
    float *rgc_x, *rgc_y, *rgc_bias, *rgc_gexc;
    neuron_model_state_t* rgc_nm;
    rcsr_t rgc_in;      /**< from bipolars */
    rcsr_t rgc_am;      /**< GABA from wide-field amacrines */
    dendrite_t** rgc_dend;
    uint32_t* link_spine;  /**< per rgc_in link */
    float* link_att;       /**< per rgc_in link */
    uint16_t* rgc_spk;     /**< this call */
    uint16_t* lgn_spk;     /**< this call */
    uint8_t* spk_now;      /**< this step */
    float* rgc_rate;       /**< this call, Hz (for myelination) */
    uint32_t n_spines;

    /* --- astrocytes --- */
    uint32_t n_astro;
    astrocyte_t** astro;
    uint32_t* bc_astro;
    float* astro_load;
    float* astro_gain;

    /* --- optic nerve --- */
    optic_nerve_t* nerve;

    /* --- output maps --- */
    float* on_map;
    float* off_map;
    rcsr_t map_on, map_off;  /**< map cell -> RGCs */
    float* tmp_rate;

    uint32_t n_gap;
};

//=============================================================================
// Configuration
//=============================================================================

retina_circuit_config_t retina_circuit_config_default(void)
{
    retina_circuit_config_t c;
    memset(&c, 0, sizeof(c));
    c.mosaic_w = 16;
    c.mosaic_h = 16;
    c.fovea_radius = 0.5F;
    c.peripheral_cone_fraction = 0.25F;
    c.horizontal_grid = 8;
    c.diffuse_grid = 7;
    c.rod_bipolar_grid = 6;
    c.aii_grid = 4;
    c.wide_amacrine_grid = 4;
    c.astrocyte_grid = 4;
    c.map_w = 8;
    c.map_h = 8;
    c.dt_ms = 1.0F;
    c.illuminance = 10.0F;
    c.display_gamma = 2.2F;
    c.rgc_noise = 2.0F;
    c.enable_dendrites = true;
    c.enable_astrocytes = true;
    c.enable_optic_nerve = true;
    c.nerve = optic_nerve_config_default();
    c.seed = 0x5EE5A11ULL;
    return c;
}

static void grid_positions(uint32_t g, float* x, float* y)
{
    for (uint32_t j = 0; j < g; j++)
        for (uint32_t i = 0; i < g; i++) {
            x[j * g + i] = ((float)i + 0.5F) / (float)g;
            y[j * g + i] = ((float)j + 0.5F) / (float)g;
        }
}

#define ALLOC(ptr, n, T) do { (ptr) = (T*)nimcp_calloc(((n) != 0) ? (size_t)(n) : 1u, sizeof(T)); if (!(ptr)) goto fail; } while (0)

//=============================================================================
// Creation
//=============================================================================

static neuron_model_state_t make_izh(float a, float b, float c, float d)
{
    izhikevich_params_t p = izhikevich_create_params(a, b, c, d);
    return neuron_model_create(neuron_model_get_izhikevich_vtable(), &p);
}

/** Build a ganglion cell's dendrite: one spine per bipolar input. */
static int build_rgc_dendrite(retina_circuit_t* r, uint32_t g, bool parasol)
{
    dendrite_config_t dc;
    memset(&dc, 0, sizeof(dc));
    dc.id = g;
    dc.type = DENDRITE_TYPE_BASAL;
    dc.target_neuron_id = g;
    dc.total_length = parasol ? 150.0F : 40.0F;
    dc.mean_diameter = parasol ? 1.2F : 0.9F;
    dc.integration_window_ms = 20.0F;
    dendrite_t* d = dendrite_create(&dc);
    if (!d) return -1;
    r->rgc_dend[g] = d;

    /* proximal trunk + distal branches (parasol: 3 branches, midget: 1) */
    segment_config_t sc[4];
    uint32_t nseg = parasol ? 4 : 2;
    memset(sc, 0, sizeof(sc));
    for (uint32_t s = 0; s < nseg; s++) {
        sc[s].type = s ? DENDRITE_SEGMENT_DISTAL : DENDRITE_SEGMENT_PROXIMAL;
        sc[s].parent_segment = s ? 0 : UINT32_MAX;
        sc[s].length = parasol ? (s ? 40.0F : 30.0F) : 20.0F;
        sc[s].diameter = parasol ? (s ? 1.0F : 1.5F) : (s ? 0.8F : 1.0F);
        sc[s].path_distance = parasol ? (s ? 40.0F + 20.0F * (float)s : 15.0F) : (s ? 30.0F : 10.0F);
    }
    if (!dendrite_create_segments(d, nseg, sc)) return -1;

    /* strongest input on the trunk, the rest round-robin on the branches */
    uint32_t k0 = r->rgc_in.off[g], k1 = r->rgc_in.off[g + 1];
    uint32_t best = k0;
    for (uint32_t k = k0; k < k1; k++) if (r->rgc_in.w[k] > r->rgc_in.w[best]) best = k;
    uint32_t rr = 1;
    for (uint32_t k = k0; k < k1; k++) {
        uint32_t seg = (k == best || nseg == 1) ? 0 : rr;
        if (k != best) { rr++; if (rr >= nseg) rr = 1; }
        uint32_t sp = dendrite_add_spine(d, seg, SPINE_TYPE_STUBBY, k);
        if (sp == UINT32_MAX) return -1;
        /* Graded (mean-field) drive every 1 ms: AMPA only. Fed continuously,
           the NMDA conductance (tau 100 ms) integrates to its 20 nS cap and
           would turn every ribbon into a constant current. */
        d->spines[sp].nmda_receptors = 0.0F;
        r->link_spine[k] = sp;
        r->link_att[k] = dendrite_get_attenuation(d, seg);
        r->n_spines++;
    }
    return 0;
}

retina_circuit_t* retina_circuit_create(const retina_circuit_config_t* cfg_in)
{
    retina_circuit_heartbeat("create", 0.0F);
    uint8_t* is_cone = NULL; uint8_t* is_rod = NULL; uint8_t* is_lm = NULL;
    retina_circuit_t* r = (retina_circuit_t*)nimcp_calloc(1, sizeof(*r));
    if (!r) return NULL;
    r->cfg = cfg_in ? *cfg_in : retina_circuit_config_default();
    retina_circuit_config_t* c = &r->cfg;
    if (c->mosaic_w < 4 || c->mosaic_h < 4 || c->dt_ms <= 0.0F || c->dt_ms > 2.0F) goto fail;
    if (c->display_gamma <= 0.0F) c->display_gamma = 2.2F;
    if (c->map_w == 0) c->map_w = 8;
    if (c->map_h == 0) c->map_h = 8;
    r->rng = c->seed ? c->seed : 0x5EE5A11ULL;
    for (int i = 0; i < 256; i++) r->lin_lut[i] = powf((float)i / 255.0F, c->display_gamma);
    r->occ_inv_sat = 1.0F / nt_calibrate_occ_sat(NT_GLUTAMATE);
    r->occ_inv_sat_gaba = 1.0F / nt_calibrate_occ_sat(NT_GABA);
    r->occ_inv_sat_gly = 1.0F / nt_calibrate_occ_sat(NT_GLYCINE);
    r->spine_inv_gain = 1.0F / spine_calibrate();

    /* ---------------- photoreceptor mosaic ---------------- */
    r->n_pr = c->mosaic_w * c->mosaic_h;
    ALLOC(r->pr_type, r->n_pr, uint8_t);
    ALLOC(r->pr_x, r->n_pr, float); ALLOC(r->pr_y, r->n_pr, float);
    ALLOC(r->pr_E, r->n_pr, float); ALLOC(r->pr_A, r->n_pr, float);
    ALLOC(r->pr_r, r->n_pr, float); ALLOC(r->pr_V, r->n_pr, float);
    ALLOC(r->pr_Vtmp, r->n_pr, float); ALLOC(r->pr_r_acc, r->n_pr, float);
    ALLOC(r->s_pr, r->n_pr, float);
    ALLOC(is_cone, r->n_pr, uint8_t); ALLOC(is_rod, r->n_pr, uint8_t); ALLOC(is_lm, r->n_pr, uint8_t);
    for (uint32_t j = 0; j < c->mosaic_h; j++) {
        for (uint32_t i = 0; i < c->mosaic_w; i++) {
            uint32_t p = j * c->mosaic_w + i;
            r->pr_x[p] = ((float)i + 0.5F) / (float)c->mosaic_w;
            r->pr_y[p] = ((float)j + 0.5F) / (float)c->mosaic_h;
            float ecc = sqrtf(sq(2.0F * r->pr_x[p] - 1.0F) + sq(2.0F * r->pr_y[p] - 1.0F));
            bool cone = (ecc < c->fovea_radius) || (rng_unif(&r->rng) < c->peripheral_cone_fraction);
            uint8_t t = RETINA_PR_ROD;
            if (cone) {
                float u = rng_unif(&r->rng);
                if (ecc >= 0.15F && u < 1.0F / 16.0F) t = RETINA_PR_S;        /* no S in the foveola */
                else t = (rng_unif(&r->rng) < 10.0F / 15.0F) ? RETINA_PR_L : RETINA_PR_M; /* L:M = 2:1 */
            }
            r->pr_type[p] = t;
            is_cone[p] = (t != RETINA_PR_ROD);
            is_rod[p] = (t == RETINA_PR_ROD);
            is_lm[p] = (t == RETINA_PR_L || t == RETINA_PR_M);
            r->pr_V[p] = PR_V_DARK;
        }
    }
    /* gap junctions between 4-neighbours; coupling depends on the pair */
    {
        if (csr_build(&r->pr_gap, r->n_pr, r->pr_x, r->pr_y, r->n_pr, r->pr_x, r->pr_y,
                      NULL, 1.0F / (float)c->mosaic_w + 1e-4F, 1.0F, 0, false, true, 0.0F) != 0)
            goto fail;
        for (uint32_t p = 0; p < r->n_pr; p++)
            for (uint32_t k = r->pr_gap.off[p]; k < r->pr_gap.off[p + 1]; k++) {
                uint32_t q = r->pr_gap.idx[k];
                r->pr_gap.w[k] = (is_rod[p] && is_rod[q]) ? GAP_ROD_ROD
                               : (is_rod[p] || is_rod[q]) ? GAP_ROD_CONE : GAP_CONE_CONE;
                r->n_gap++;
            }
        r->n_gap /= 2;
    }
    if (nt_pool_init(&r->pr_glu, r->n_pr * PR_RIBBONS, NT_GLUTAMATE, &r->rng) != 0) goto fail;

    /* ---------------- horizontal cells ---------------- */
    r->n_h = c->horizontal_grid * c->horizontal_grid;
    ALLOC(r->h_x, r->n_h, float); ALLOC(r->h_y, r->n_h, float);
    ALLOC(r->h_V, r->n_h, float); ALLOC(r->h_Vtmp, r->n_h, float); ALLOC(r->s_h, r->n_h, float);
    grid_positions(c->horizontal_grid, r->h_x, r->h_y);
    for (uint32_t h = 0; h < r->n_h; h++) r->h_V[h] = -30.0F;
    if (csr_build(&r->h_in, r->n_h, r->h_x, r->h_y, r->n_pr, r->pr_x, r->pr_y, is_cone,
                  0.2F, 0.12F, 2, true, false, 0.0F) != 0) goto fail;
    if (csr_build(&r->h_gap, r->n_h, r->h_x, r->h_y, r->n_h, r->h_x, r->h_y, NULL,
                  1.0F / (float)c->horizontal_grid + 1e-4F, 1.0F, 0, false, true, H_GGAP) != 0) goto fail;
    r->n_gap += (r->h_gap.off[r->n_h]) / 2;
    if (csr_build(&r->pr_fb, r->n_pr, r->pr_x, r->pr_y, r->n_h, r->h_x, r->h_y, NULL,
                  0.2F, 0.12F, 1, true, false, 0.0F) != 0) goto fail;
    if (nt_pool_init(&r->h_gaba, r->n_h * H_SITES, NT_GABA, &r->rng) != 0) goto fail;

    /* ---------------- bipolar cells ---------------- */
    {
        /* midget: one ON + one OFF per foveal L/M cone */
        uint32_t nm = 0;
        for (uint32_t p = 0; p < r->n_pr; p++) {
            float ecc = sqrtf(sq(2.0F * r->pr_x[p] - 1.0F) + sq(2.0F * r->pr_y[p] - 1.0F));
            if (is_lm[p] && ecc < c->fovea_radius) nm++;
        }
        /* rod bipolars only where rods are */
        uint32_t gr = c->rod_bipolar_grid, nr = 0;
        float* rx = NULL; float* ry = NULL;
        ALLOC(rx, gr * gr, float); ALLOC(ry, gr * gr, float);
        float* gx = NULL; float* gy = NULL;
        ALLOC(gx, gr * gr, float); ALLOC(gy, gr * gr, float);
        grid_positions(gr, gx, gy);
        for (uint32_t k = 0; k < gr * gr; k++) {
            bool any = false;
            for (uint32_t p = 0; p < r->n_pr && !any; p++)
                if (is_rod[p] && sq(gx[k] - r->pr_x[p]) + sq(gy[k] - r->pr_y[p]) < 0.12F * 0.12F) any = true;
            if (any) { rx[nr] = gx[k]; ry[nr] = gy[k]; nr++; }
        }
        nimcp_free(gx); nimcp_free(gy);
        uint32_t nd = c->diffuse_grid * c->diffuse_grid;
        r->n_midget = nm; r->n_diffuse = nd; r->n_rodbc = nr;
        r->n_bc = 2 * nm + 2 * nd + nr;
        ALLOC(r->bc_type, r->n_bc, uint8_t); ALLOC(r->bc_cone, r->n_bc, uint8_t);
        ALLOC(r->bc_x, r->n_bc, float); ALLOC(r->bc_y, r->n_bc, float); ALLOC(r->bc_V, r->n_bc, float);
        ALLOC(r->s_bc, r->n_bc, float); ALLOC(r->bc_gain, r->n_bc, float); ALLOC(r->bc_sbar, r->n_bc, float);
        uint32_t* mid_cone = NULL;
        ALLOC(mid_cone, nm, uint32_t);
        uint32_t b = 0;
        for (uint32_t p = 0; p < r->n_pr; p++) {
            float ecc = sqrtf(sq(2.0F * r->pr_x[p] - 1.0F) + sq(2.0F * r->pr_y[p] - 1.0F));
            if (is_lm[p] && ecc < c->fovea_radius) mid_cone[b++] = p;
        }
        float* dx = NULL; float* dy = NULL;
        ALLOC(dx, nd, float); ALLOC(dy, nd, float);
        grid_positions(c->diffuse_grid, dx, dy);
        for (uint32_t i = 0; i < r->n_bc; i++) {
            uint8_t t; float x, y; uint8_t cone = RETINA_PR_TYPE_COUNT;
            if (i < nm)              { t = BC_MIDGET_ON;  x = r->pr_x[mid_cone[i]];      y = r->pr_y[mid_cone[i]];      cone = r->pr_type[mid_cone[i]]; }
            else if (i < 2 * nm)     { t = BC_MIDGET_OFF; x = r->pr_x[mid_cone[i - nm]]; y = r->pr_y[mid_cone[i - nm]]; cone = r->pr_type[mid_cone[i - nm]]; }
            else if (i < 2 * nm + nd){ t = BC_DIFFUSE_ON; x = dx[i - 2 * nm];            y = dy[i - 2 * nm]; }
            else if (i < 2 * nm + 2 * nd) { t = BC_DIFFUSE_OFF; x = dx[i - 2 * nm - nd]; y = dy[i - 2 * nm - nd]; }
            else                     { t = BC_ROD; x = rx[i - 2 * nm - 2 * nd]; y = ry[i - 2 * nm - 2 * nd]; }
            r->bc_type[i] = t; r->bc_cone[i] = cone; r->bc_x[i] = x; r->bc_y[i] = y;
            r->bc_V[i] = BC_EL; r->bc_gain[i] = 1.0F;
        }
        nimcp_free(rx); nimcp_free(ry); nimcp_free(dx); nimcp_free(dy);

        /* inputs: build per class then merge into one CSR */
        uint32_t* off = NULL;
        ALLOC(off, r->n_bc + 1, uint32_t);
        uint32_t cap = r->n_bc * 16, nnz = 0;
        uint32_t* idx = (uint32_t*)nimcp_malloc(cap * sizeof(uint32_t));
        float* w = (float*)nimcp_malloc(cap * sizeof(float));
        if (!idx || !w) { nimcp_free(idx); nimcp_free(w); nimcp_free(mid_cone); nimcp_free(off); goto fail; }
        rcsr_t tmp;
        for (uint32_t i = 0; i < r->n_bc; i++) {
            off[i] = nnz;
            uint8_t t = r->bc_type[i];
            if (t == BC_MIDGET_ON || t == BC_MIDGET_OFF) {
                idx[nnz] = mid_cone[t == BC_MIDGET_ON ? i : i - nm]; w[nnz] = 1.0F; nnz++;
                continue;
            }
            const uint8_t* ok = (t == BC_ROD) ? is_rod : is_lm;
            float rad = (t == BC_ROD) ? 0.12F : 0.12F, sig = (t == BC_ROD) ? 0.08F : 0.07F;
            if (csr_build(&tmp, 1, &r->bc_x[i], &r->bc_y[i], r->n_pr, r->pr_x, r->pr_y, ok,
                          rad, sig, 2, true, false, 0.0F) != 0) { nimcp_free(idx); nimcp_free(w); nimcp_free(mid_cone); nimcp_free(off); goto fail; }
            for (uint32_t k = 0; k < tmp.off[1] && nnz < cap; k++) { idx[nnz] = tmp.idx[k]; w[nnz] = tmp.w[k]; nnz++; }
            csr_free(&tmp);
        }
        off[r->n_bc] = nnz;
        r->bc_in.n = r->n_bc; r->bc_in.off = off; r->bc_in.idx = idx; r->bc_in.w = w;
        nimcp_free(mid_cone);
    }
    if (nt_pool_init(&r->bc_glu, r->n_bc * BC_RIBBONS, NT_GLUTAMATE, &r->rng) != 0) goto fail;
    ALLOC(r->bc_stp, r->n_bc * BC_RIBBONS, stp_state_t);
    for (uint32_t i = 0; i < r->n_bc; i++) {
        uint8_t t = r->bc_type[i];
        /* diffuse (parasol pathway) ribbons are transient, midget sustained;
           rod-bipolar ribbons deplete under sustained (photopic) drive, which
           is what silences the rod pathway in daylight */
        stp_params_t sp = (t == BC_DIFFUSE_ON || t == BC_DIFFUSE_OFF)
                        ? stp_create_params(0.10F, 300.0F, 20.0F)
                        : (t == BC_ROD) ? stp_create_params(0.15F, 600.0F, 20.0F)
                        : stp_create_params(0.05F, 200.0F, 20.0F);
        for (uint32_t k = 0; k < BC_RIBBONS; k++) stp_init(&r->bc_stp[i * BC_RIBBONS + k], &sp, 0);
    }

    /* ---------------- AII amacrines ---------------- */
    r->n_aii = c->aii_grid * c->aii_grid;
    ALLOC(r->aii_x, r->n_aii, float); ALLOC(r->aii_y, r->n_aii, float);
    ALLOC(r->aii_V, r->n_aii, float); ALLOC(r->aii_Vtmp, r->n_aii, float);
    ALLOC(r->aii_Igap, r->n_aii, float); ALLOC(r->s_aii, r->n_aii, float);
    grid_positions(c->aii_grid, r->aii_x, r->aii_y);
    for (uint32_t a = 0; a < r->n_aii; a++) r->aii_V[a] = AII_EL;
    {
        uint8_t* is_rbc = NULL; uint8_t* is_oncone = NULL; uint8_t* is_off = NULL;
        ALLOC(is_rbc, r->n_bc, uint8_t); ALLOC(is_oncone, r->n_bc, uint8_t); ALLOC(is_off, r->n_bc, uint8_t);
        for (uint32_t i = 0; i < r->n_bc; i++) {
            is_rbc[i] = r->bc_type[i] == BC_ROD;
            is_oncone[i] = r->bc_type[i] == BC_MIDGET_ON || r->bc_type[i] == BC_DIFFUSE_ON;
            is_off[i] = r->bc_type[i] == BC_MIDGET_OFF || r->bc_type[i] == BC_DIFFUSE_OFF;
        }
        int rc = csr_build(&r->aii_in, r->n_aii, r->aii_x, r->aii_y, r->n_bc, r->bc_x, r->bc_y,
                           is_rbc, 0.3F, 0.15F, 0, true, false, 0.0F);
        rc |= csr_build(&r->aii_gap, r->n_aii, r->aii_x, r->aii_y, r->n_aii, r->aii_x, r->aii_y,
                        NULL, 1.0F / (float)c->aii_grid + 1e-4F, 1.0F, 0, false, true, AII_GGAP);
        /* ON cone bipolar -> nearest AII (gap); OFF bipolar <- nearest AII (glycine) */
        rc |= csr_build(&r->bc_aii, r->n_bc, r->bc_x, r->bc_y, r->n_aii, r->aii_x, r->aii_y,
                        NULL, 0.0F, 1.0F, 1, false, false, BC_G_AII);
        rc |= csr_build(&r->bc_gly, r->n_bc, r->bc_x, r->bc_y, r->n_aii, r->aii_x, r->aii_y,
                        NULL, 0.25F, 0.15F, 1, true, false, 0.0F);
        /* blank the rows that do not apply */
        for (uint32_t i = 0; i < r->n_bc; i++) {
            if (!is_oncone[i]) for (uint32_t k = r->bc_aii.off[i]; k < r->bc_aii.off[i + 1]; k++) r->bc_aii.w[k] = 0.0F;
            else r->n_gap++;
            if (!is_off[i]) for (uint32_t k = r->bc_gly.off[i]; k < r->bc_gly.off[i + 1]; k++) r->bc_gly.w[k] = 0.0F;
        }
        r->n_gap += r->aii_gap.off[r->n_aii] / 2;
        nimcp_free(is_rbc); nimcp_free(is_oncone); nimcp_free(is_off);
        if (rc != 0) goto fail;
    }
    if (nt_pool_init(&r->aii_gly, r->n_aii * AII_SITES, NT_GLYCINE, &r->rng) != 0) goto fail;

    /* ---------------- wide-field amacrines (spiking) ---------------- */
    r->n_am = c->wide_amacrine_grid * c->wide_amacrine_grid;
    ALLOC(r->am_x, r->n_am, float); ALLOC(r->am_y, r->n_am, float);
    ALLOC(r->s_am, r->n_am, float); ALLOC(r->am_spikes, r->n_am, uint32_t);
    ALLOC(r->am_nm, r->n_am, neuron_model_state_t);
    grid_positions(c->wide_amacrine_grid, r->am_x, r->am_y);
    for (uint32_t a = 0; a < r->n_am; a++) {
        r->am_nm[a] = make_izh(0.1F, 0.2F, -65.0F, 2.0F);   /* fast spiking */
        if (!r->am_nm[a]) goto fail;
    }
    {
        uint8_t* is_diff = NULL;
        ALLOC(is_diff, r->n_bc, uint8_t);
        for (uint32_t i = 0; i < r->n_bc; i++)
            is_diff[i] = r->bc_type[i] == BC_DIFFUSE_ON || r->bc_type[i] == BC_DIFFUSE_OFF;
        int rc = csr_build(&r->am_in, r->n_am, r->am_x, r->am_y, r->n_bc, r->bc_x, r->bc_y,
                           is_diff, 0.45F, 0.25F, 2, true, false, 0.0F);
        rc |= csr_build(&r->bc_am, r->n_bc, r->bc_x, r->bc_y, r->n_am, r->am_x, r->am_y,
                        NULL, 0.45F, 0.3F, 1, true, false, 0.0F);
        nimcp_free(is_diff);
        if (rc != 0) goto fail;
    }
    if (nt_pool_init(&r->am_gaba, r->n_am * AM_SITES, NT_GABA, &r->rng) != 0) goto fail;

    /* ---------------- ganglion cells ---------------- */
    r->n_rgc = 2 * r->n_midget + 2 * r->n_diffuse;
    ALLOC(r->rgc_type, r->n_rgc, uint8_t); ALLOC(r->rgc_cone, r->n_rgc, uint8_t);
    ALLOC(r->rgc_x, r->n_rgc, float); ALLOC(r->rgc_y, r->n_rgc, float); ALLOC(r->rgc_bias, r->n_rgc, float); ALLOC(r->rgc_gexc, r->n_rgc, float);
    ALLOC(r->rgc_nm, r->n_rgc, neuron_model_state_t);
    ALLOC(r->rgc_dend, r->n_rgc, dendrite_t*);
    ALLOC(r->rgc_spk, r->n_rgc, uint16_t); ALLOC(r->lgn_spk, r->n_rgc, uint16_t);
    ALLOC(r->spk_now, r->n_rgc, uint8_t); ALLOC(r->rgc_rate, r->n_rgc, float);
    ALLOC(r->tmp_rate, r->n_rgc, float);
    {
        /* RGC g mirrors bipolar g for the cone-driven bipolars (same layout) */
        uint8_t* is_don = NULL; uint8_t* is_doff = NULL;
        ALLOC(is_don, r->n_bc, uint8_t); ALLOC(is_doff, r->n_bc, uint8_t);
        for (uint32_t i = 0; i < r->n_bc; i++) {
            is_don[i] = r->bc_type[i] == BC_DIFFUSE_ON;
            is_doff[i] = r->bc_type[i] == BC_DIFFUSE_OFF;
        }
        uint32_t* off = NULL;
        ALLOC(off, r->n_rgc + 1, uint32_t);
        uint32_t cap = r->n_rgc * 12, nnz = 0;
        uint32_t* idx = (uint32_t*)nimcp_malloc(cap * sizeof(uint32_t));
        float* w = (float*)nimcp_malloc(cap * sizeof(float));
        if (!idx || !w) { nimcp_free(idx); nimcp_free(w); nimcp_free(off); nimcp_free(is_don); nimcp_free(is_doff); goto fail; }
        for (uint32_t g = 0; g < r->n_rgc; g++) {
            off[g] = nnz;
            uint32_t bc = g;   /* midget ON/OFF and parasol share index with their bipolar */
            uint8_t t = (g < r->n_midget) ? RETINA_RGC_MIDGET_ON
                      : (g < 2 * r->n_midget) ? RETINA_RGC_MIDGET_OFF
                      : (g < 2 * r->n_midget + r->n_diffuse) ? RETINA_RGC_PARASOL_ON
                      : RETINA_RGC_PARASOL_OFF;
            r->rgc_type[g] = t;
            r->rgc_x[g] = r->bc_x[bc];
            r->rgc_y[g] = r->bc_y[bc];
            r->rgc_cone[g] = (t <= RETINA_RGC_MIDGET_OFF) ? r->bc_cone[bc] : RETINA_PR_TYPE_COUNT;
            if (t <= RETINA_RGC_MIDGET_OFF) {
                idx[nnz] = bc; w[nnz] = 1.0F; nnz++;
            } else {
                rcsr_t tmp;
                if (csr_build(&tmp, 1, &r->rgc_x[g], &r->rgc_y[g], r->n_bc, r->bc_x, r->bc_y,
                              t == RETINA_RGC_PARASOL_ON ? is_don : is_doff,
                              0.16F, 0.06F, 1, true, false, 0.0F) != 0) {
                    nimcp_free(idx); nimcp_free(w); nimcp_free(off); nimcp_free(is_don); nimcp_free(is_doff); goto fail;
                }
                for (uint32_t k = 0; k < tmp.off[1] && nnz < cap; k++) { idx[nnz] = tmp.idx[k]; w[nnz] = tmp.w[k]; nnz++; }
                csr_free(&tmp);
            }
        }
        off[r->n_rgc] = nnz;
        r->rgc_in.n = r->n_rgc; r->rgc_in.off = off; r->rgc_in.idx = idx; r->rgc_in.w = w;
        nimcp_free(is_don); nimcp_free(is_doff);
        if (csr_build(&r->rgc_am, r->n_rgc, r->rgc_x, r->rgc_y, r->n_am, r->am_x, r->am_y,
                      NULL, 0.45F, 0.3F, 1, true, false, 0.0F) != 0) goto fail;
    }
    ALLOC(r->link_spine, r->rgc_in.off[r->n_rgc], uint32_t);
    ALLOC(r->link_att, r->rgc_in.off[r->n_rgc], float);
    for (uint32_t k = 0; k < r->rgc_in.off[r->n_rgc]; k++) r->link_att[k] = 1.0F;
    for (uint32_t g = 0; g < r->n_rgc; g++) {
        bool parasol = r->rgc_type[g] >= RETINA_RGC_PARASOL_ON;
        /* midget: regular spiking, sustained; parasol: fast, little adaptation */
        r->rgc_nm[g] = parasol ? make_izh(0.1F, 0.2F, -65.0F, 2.0F) : make_izh(0.02F, 0.2F, -65.0F, 6.0F);
        if (!r->rgc_nm[g]) goto fail;
        r->rgc_bias[g] = parasol ? RGC_BIAS_PARASOL : RGC_BIAS_MIDGET;
        r->rgc_gexc[g] = parasol ? RGC_G_EXC_PARASOL : RGC_G_EXC_MIDGET;
        if (c->enable_dendrites && build_rgc_dendrite(r, g, parasol) != 0) goto fail;
    }

    /* ---------------- astrocytes ---------------- */
    if (c->enable_astrocytes && c->astrocyte_grid > 0) {
        uint32_t ga = c->astrocyte_grid;
        r->n_astro = ga * ga;
        float* ax = NULL; float* ay = NULL;
        ALLOC(ax, r->n_astro, float); ALLOC(ay, r->n_astro, float);
        grid_positions(ga, ax, ay);
        ALLOC(r->astro, r->n_astro, astrocyte_t*);
        ALLOC(r->bc_astro, r->n_bc, uint32_t);
        ALLOC(r->astro_load, r->n_astro, float); ALLOC(r->astro_gain, r->n_astro, float);
        for (uint32_t a = 0; a < r->n_astro; a++) {
            r->astro[a] = astrocyte_create(a, ASTROCYTE_TYPE_V1_SENSORY, ax[a] * 1000.0F,
                                           ay[a] * 1000.0F, 0.0F, 1000.0F / (float)ga);
            r->astro_gain[a] = 1.0F;
            if (!r->astro[a]) { nimcp_free(ax); nimcp_free(ay); goto fail; }
        }
        /* each bipolar output terminal belongs to its nearest astrocyte */
        for (uint32_t i = 0; i < r->n_bc; i++) {
            uint32_t best = 0; float bd = INFINITY;
            for (uint32_t a = 0; a < r->n_astro; a++) {
                float d2 = sq(r->bc_x[i] - ax[a]) + sq(r->bc_y[i] - ay[a]);
                if (d2 < bd) { bd = d2; best = a; }
            }
            r->bc_astro[i] = best;
            astrocyte_assign_synapse(r->astro[best], i);
        }
        nimcp_free(ax); nimcp_free(ay);
    }

    /* ---------------- optic nerve ---------------- */
    if (c->enable_optic_nerve) {
        float* diam = NULL;
        ALLOC(diam, r->n_rgc, float);
        for (uint32_t g = 0; g < r->n_rgc; g++)
            diam[g] = (r->rgc_type[g] >= RETINA_RGC_PARASOL_ON) ? 1.5F : 0.8F;
        r->nerve = optic_nerve_create(r->n_rgc, diam, &c->nerve);
        nimcp_free(diam);
        if (!r->nerve) goto fail;
    }

    /* ---------------- output maps ---------------- */
    {
        uint32_t nmap = c->map_w * c->map_h;
        ALLOC(r->on_map, nmap, float); ALLOC(r->off_map, nmap, float);
        float* mx = NULL; float* my = NULL; uint8_t* on = NULL; uint8_t* off = NULL;
        ALLOC(mx, nmap, float); ALLOC(my, nmap, float);
        ALLOC(on, r->n_rgc, uint8_t); ALLOC(off, r->n_rgc, uint8_t);
        for (uint32_t j = 0; j < c->map_h; j++)
            for (uint32_t i = 0; i < c->map_w; i++) {
                mx[j * c->map_w + i] = ((float)i + 0.5F) / (float)c->map_w;
                my[j * c->map_w + i] = ((float)j + 0.5F) / (float)c->map_h;
            }
        for (uint32_t g = 0; g < r->n_rgc; g++) {
            on[g] = r->rgc_type[g] == RETINA_RGC_MIDGET_ON || r->rgc_type[g] == RETINA_RGC_PARASOL_ON;
            off[g] = !on[g];
        }
        float rad = 0.5F / (float)(c->map_w > c->map_h ? c->map_h : c->map_w) * 1.42F;
        int rc = csr_build(&r->map_on, nmap, mx, my, r->n_rgc, r->rgc_x, r->rgc_y, on, rad, rad, 1, true, false, 0.0F);
        rc |= csr_build(&r->map_off, nmap, mx, my, r->n_rgc, r->rgc_x, r->rgc_y, off, rad, rad, 1, true, false, 0.0F);
        nimcp_free(mx); nimcp_free(my); nimcp_free(on); nimcp_free(off);
        if (rc != 0) goto fail;
    }

    nimcp_free(is_cone); nimcp_free(is_rod); nimcp_free(is_lm);
    retina_circuit_heartbeat("create", 1.0F);
    return r;

fail:
    nimcp_free(is_cone); nimcp_free(is_rod); nimcp_free(is_lm);
    retina_circuit_destroy(r);
    return NULL;
}

void retina_circuit_destroy(retina_circuit_t* r)
{
    if (!r) return;
    nt_pool_free(&r->pr_glu); nt_pool_free(&r->h_gaba); nt_pool_free(&r->bc_glu);
    nt_pool_free(&r->aii_gly); nt_pool_free(&r->am_gaba);
    csr_free(&r->pr_gap); csr_free(&r->pr_fb); csr_free(&r->h_in); csr_free(&r->h_gap);
    csr_free(&r->bc_in); csr_free(&r->bc_am); csr_free(&r->bc_gly); csr_free(&r->bc_aii);
    csr_free(&r->aii_in); csr_free(&r->aii_gap); csr_free(&r->am_in);
    csr_free(&r->rgc_in); csr_free(&r->rgc_am); csr_free(&r->map_on); csr_free(&r->map_off);
    if (r->am_nm) for (uint32_t a = 0; a < r->n_am; a++) neuron_model_destroy(r->am_nm[a]);
    if (r->rgc_nm) for (uint32_t g = 0; g < r->n_rgc; g++) neuron_model_destroy(r->rgc_nm[g]);
    if (r->rgc_dend) for (uint32_t g = 0; g < r->n_rgc; g++) dendrite_destroy(r->rgc_dend[g]);
    if (r->astro) for (uint32_t a = 0; a < r->n_astro; a++) astrocyte_destroy(r->astro[a]);
    optic_nerve_destroy(r->nerve);
    void* bufs[] = {
        r->pr_type, r->pr_x, r->pr_y, r->pr_E, r->pr_A, r->pr_r, r->pr_V, r->pr_Vtmp, r->pr_r_acc, r->s_pr,
        r->h_x, r->h_y, r->h_V, r->h_Vtmp, r->s_h,
        r->bc_type, r->bc_cone, r->bc_x, r->bc_y, r->bc_V, r->bc_stp, r->s_bc, r->bc_sbar, r->bc_gain,
        r->aii_x, r->aii_y, r->aii_V, r->aii_Vtmp, r->aii_Igap, r->s_aii,
        r->am_x, r->am_y, r->am_nm, r->s_am, r->am_spikes,
        r->rgc_type, r->rgc_cone, r->rgc_x, r->rgc_y, r->rgc_bias, r->rgc_gexc, r->rgc_nm, r->rgc_dend,
        r->link_spine, r->link_att, r->rgc_spk, r->lgn_spk, r->spk_now, r->rgc_rate, r->tmp_rate,
        r->astro, r->bc_astro, r->astro_load, r->astro_gain, r->on_map, r->off_map
    };
    for (size_t i = 0; i < sizeof(bufs) / sizeof(bufs[0]); i++) nimcp_free(bufs[i]);
    nimcp_free(r);
}

//=============================================================================
// Simulation
//=============================================================================

/* Spectral weights, linear sRGB -> cone / rod excitation (white = 1).
 * Cones: Hunt-Pointer-Estevez LMS of the sRGB primaries; rod: scotopic V'(lambda). */
static const float SPECTRAL[RETINA_PR_TYPE_COUNT][3] = {
    { 0.04F,   0.58F,   0.38F   },   /* rod */
    { 0.3139F, 0.6395F, 0.0466F },   /* L */
    { 0.1554F, 0.7579F, 0.0867F },   /* M */
    { 0.0178F, 0.1094F, 0.8728F },   /* S */
};

/** Area-sample the frame onto the mosaic: per-photoreceptor excitation E. */
static void sample_frame(retina_circuit_t* r, const uint8_t* px, uint32_t w, uint32_t h, uint32_t ch)
{
    const retina_circuit_config_t* c = &r->cfg;
    for (uint32_t p = 0; p < r->n_pr; p++) {
        uint32_t i = p % c->mosaic_w, j = p / c->mosaic_w;
        uint32_t x0 = i * w / c->mosaic_w, x1 = (i + 1) * w / c->mosaic_w;
        uint32_t y0 = j * h / c->mosaic_h, y1 = (j + 1) * h / c->mosaic_h;
        if (x1 <= x0) x1 = x0 + 1;
        if (y1 <= y0) y1 = y0 + 1;
        if (x1 > w) x1 = w;
        if (y1 > h) y1 = h;
        float rgb[3] = { 0.0F, 0.0F, 0.0F };
        uint32_t n = 0;
        for (uint32_t y = y0; y < y1; y++)
            for (uint32_t x = x0; x < x1; x++) {
                const uint8_t* q = px + ((size_t)y * w + x) * ch;
                if (ch >= 3) { rgb[0] += r->lin_lut[q[0]]; rgb[1] += r->lin_lut[q[1]]; rgb[2] += r->lin_lut[q[2]]; }
                else { float v = r->lin_lut[q[0]]; rgb[0] += v; rgb[1] += v; rgb[2] += v; }
                n++;
            }
        const float* sw = SPECTRAL[r->pr_type[p]];
        float e = (sw[0] * rgb[0] + sw[1] * rgb[1] + sw[2] * rgb[2]) / (float)(n ? n : 1);
        r->pr_E[p] = e * c->illuminance;
    }
}

/** One integration step (dt ms) of the whole circuit. */
static void retina_step(retina_circuit_t* r)
{
    const float dt = r->cfg.dt_ms;
    const uint64_t t_us = r->t_us;
    const uint64_t t_ms = t_us / 1000;

    /* ---- 1. photoreceptors: phototransduction, adaptation, gap junctions ---- */
    for (uint32_t p = 0; p < r->n_pr; p++) {
        bool rod = r->pr_type[p] == RETINA_PR_ROD;
        float E = r->pr_E[p];
        float tau_a = rod ? ROD_TAU_ADAPT : CONE_TAU_ADAPT;
        r->pr_A[p] += dt / tau_a * (E - r->pr_A[p]);
        float s0 = rod ? ROD_SIGMA0 : CONE_SIGMA0;
        float A = r->pr_A[p];
        if (rod && A > ROD_ADAPT_MAX * s0) A = ROD_ADAPT_MAX * s0;
        float R = E / (E + s0 + A);                     /* Naka-Rushton, Weber-adapted */
        r->pr_r[p] += dt / (rod ? ROD_TAU_R : CONE_TAU_R) * (R - r->pr_r[p]);
        r->pr_Vtmp[p] = PR_V_DARK + (PR_V_SAT - PR_V_DARK) * r->pr_r[p];
        r->pr_r_acc[p] += r->pr_r[p];
    }
    for (uint32_t p = 0; p < r->n_pr; p++) {
        float v = r->pr_Vtmp[p];
        for (uint32_t k = r->pr_gap.off[p]; k < r->pr_gap.off[p + 1]; k++)
            v += r->pr_gap.w[k] * (r->pr_Vtmp[r->pr_gap.idx[k]] - r->pr_Vtmp[p]);
        r->pr_V[p] = v;
        /* terminal: horizontal-cell GABA hyperpolarises it (dark H = strong) */
        float rate;
        if (r->pr_type[p] != RETINA_PR_ROD)
            rate = R_MAX * sigm((v - FB_GAIN_MV * csr_dot(&r->pr_fb, p, r->s_h) - PR_V_HALF) / PR_V_SLOPE);
        else
            rate = R_MAX * sigm((v - ROD_V_HALF) / PR_V_SLOPE);
        for (uint32_t k = 0; k < PR_RIBBONS; k++) nt_release(&r->pr_glu, p * PR_RIBBONS + k, rate, dt);
    }

    /* ---- 2. horizontal cells ---- */
    for (int sub = 0; sub < H_SUBSTEPS; sub++) {
        for (uint32_t h = 0; h < r->n_h; h++) {
            float V = r->h_V[h];
            float ge = H_GE * csr_dot(&r->h_in, h, r->s_pr);
            float I = (H_EL - V) + ge * (0.0F - V);
            for (uint32_t k = r->h_gap.off[h]; k < r->h_gap.off[h + 1]; k++)
                I += r->h_gap.w[k] * (r->h_V[r->h_gap.idx[k]] - V);
            r->h_Vtmp[h] = V + dt / (float)H_SUBSTEPS / H_TAU * I;
        }
        memcpy(r->h_V, r->h_Vtmp, r->n_h * sizeof(float));
    }
    for (uint32_t h = 0; h < r->n_h; h++) {
        float rate = R_MAX * sigm((r->h_V[h] - H_V_HALF) / H_SLOPE);
        for (uint32_t k = 0; k < H_SITES; k++) nt_release(&r->h_gaba, h * H_SITES + k, rate, dt);
    }

    /* ---- 3. bipolar cells ---- */
    memset(r->aii_Igap, 0, r->n_aii * sizeof(float));
    for (uint32_t i = 0; i < r->n_bc; i++) {
        uint8_t t = r->bc_type[i];
        float V = r->bc_V[i];
        float s = csr_dot(&r->bc_in, i, r->s_pr);
        if (s > 1.0F) s = 1.0F;
        float ge;
        if (t == BC_MIDGET_OFF || t == BC_DIFFUSE_OFF) ge = BC_G_OFF * s;          /* AMPA/kainate */
        else ge = (t == BC_ROD ? BC_G_ROD : BC_G_ON) * (1.0F - s);                 /* mGluR6/TRPM1 */
        float gi = BC_G_AM * csr_dot(&r->bc_am, i, r->s_am) + BC_G_GLY * csr_dot(&r->bc_gly, i, r->s_aii);
        float I = (BC_EL - V) + ge * (0.0F - V) + gi * (-70.0F - V);
        for (uint32_t k = r->bc_aii.off[i]; k < r->bc_aii.off[i + 1]; k++) {
            float g = r->bc_aii.w[k];
            if (g == 0.0F) continue;
            uint32_t a = r->bc_aii.idx[k];
            float ig = g * (r->aii_V[a] - V);
            I += ig;
            r->aii_Igap[a] -= ig;
        }
        V += dt / BC_TAU * I;
        r->bc_V[i] = V;
        float vh = (t == BC_MIDGET_OFF || t == BC_DIFFUSE_OFF) ? BC_V_HALF_OFF
                 : (t == BC_ROD) ? BC_V_HALF_ROD : BC_V_HALF;
        float base = R_MAX * sigm((V - vh) / BC_SLOPE) * r->bc_gain[i];
        for (uint32_t k = 0; k < BC_RIBBONS; k++) {
            stp_state_t* st = &r->bc_stp[i * BC_RIBBONS + k];
            stp_update(st, t_ms);
            float mod = stp_get_modulation(st) / st->params.U;      /* ~ available vesicles */
            uint32_t n = nt_release(&r->bc_glu, i * BC_RIBBONS + k, base * mod, dt);
            for (uint32_t q = 0; q < n; q++) stp_process_spike(st, t_ms);
        }
    }

    /* ---- 4. AII amacrines ---- */
    for (uint32_t a = 0; a < r->n_aii; a++) {
        float V = r->aii_V[a];
        float ge = AII_GE * csr_dot(&r->aii_in, a, r->s_bc);
        float I = (AII_EL - V) + ge * (0.0F - V) + r->aii_Igap[a];
        for (uint32_t k = r->aii_gap.off[a]; k < r->aii_gap.off[a + 1]; k++)
            I += r->aii_gap.w[k] * (r->aii_V[r->aii_gap.idx[k]] - V);
        r->aii_Vtmp[a] = V + dt / AII_TAU * I;
    }
    for (uint32_t a = 0; a < r->n_aii; a++) {
        r->aii_V[a] = r->aii_Vtmp[a];
        float rate = R_MAX * sigm((r->aii_V[a] - AII_V_HALF) / AII_SLOPE);
        for (uint32_t k = 0; k < AII_SITES; k++) nt_release(&r->aii_gly, a * AII_SITES + k, rate, dt);
    }

    /* ---- 5. wide-field amacrines (Izhikevich, 2 x dt/2) ---- */
    for (uint32_t a = 0; a < r->n_am; a++) {
        /* ON-OFF transient drive: each input counts only its rise above its own
           recent mean (receptor desensitisation), so increments (via ON) and
           decrements (via OFF) both excite */
        float x = 0.0F;
        for (uint32_t k = r->am_in.off[a]; k < r->am_in.off[a + 1]; k++) {
            uint32_t b = r->am_in.idx[k];
            float d = r->s_bc[b] - r->bc_sbar[b] - AM_NOISE_FLOOR;
            if (d > 0.0F) x += r->am_in.w[k] * d;
        }
        float I = AM_G_IN * x + AM_BIAS;
        bool spk = false;
        for (int h = 0; h < 2; h++) {
            neuron_model_update(r->am_nm[a], 0.5F * dt, I);
            if (neuron_model_check_spike(r->am_nm[a])) { spk = true; neuron_model_post_spike(r->am_nm[a]); }
        }
        if (spk) {
            r->am_spikes[a]++;
            for (uint32_t k = 0; k < AM_SITES; k++) nimcp_nt_adapter_release(r->am_gaba.ad, (int)(a * AM_SITES + k), 1);
        }
    }

    /* ---- 6. ganglion cells ---- */
    const bool dend = r->cfg.enable_dendrites;
    for (uint32_t g = 0; g < r->n_rgc; g++) {
        float ge = 0.0F;
        for (uint32_t k = r->rgc_in.off[g]; k < r->rgc_in.off[g + 1]; k++) {
            uint32_t b = r->rgc_in.idx[k];
            float drive = r->s_bc[b];
            if (dend) {
                /* through the dendritic spine (neck RC filter), then cable attenuation */
                drive = dendrite_spine_process_input(r->rgc_dend[g], r->link_spine[k], drive, t_us)
                        * r->spine_inv_gain * r->link_att[k];
            }
            ge += r->rgc_in.w[k] * drive;
        }
        float gi = csr_dot(&r->rgc_am, g, r->s_am);
        float I = r->rgc_bias[g] + r->cfg.rgc_noise * rng_gauss(&r->rng);
        bool spk = false;
        for (int h = 0; h < 2; h++) {
            float v = neuron_model_get_voltage(r->rgc_nm[g]);
            float Ih = I + r->rgc_gexc[g] * ge * (0.0F - v) / 65.0F + RGC_G_INH * gi * (-70.0F - v) / 65.0F;
            neuron_model_update(r->rgc_nm[g], 0.5F * dt, Ih);
            if (neuron_model_check_spike(r->rgc_nm[g])) { spk = true; neuron_model_post_spike(r->rgc_nm[g]); }
        }
        r->spk_now[g] = spk;
        if (spk && r->rgc_spk[g] < UINT16_MAX) r->rgc_spk[g]++;
    }

    /* ---- 7. synaptic clefts advance; activations for the next step ---- */
    nt_pool_advance(&r->pr_glu, dt);
    nt_pool_advance(&r->h_gaba, dt);
    nt_pool_advance(&r->bc_glu, dt);
    nt_pool_advance(&r->aii_gly, dt);
    nt_pool_advance(&r->am_gaba, dt);
    for (uint32_t p = 0; p < r->n_pr; p++) r->s_pr[p] = nt_cell_s(&r->pr_glu, p, PR_RIBBONS, r->occ_inv_sat);
    for (uint32_t h = 0; h < r->n_h; h++) r->s_h[h] = nt_cell_s(&r->h_gaba, h, H_SITES, r->occ_inv_sat_gaba);
    for (uint32_t i = 0; i < r->n_bc; i++) {
        r->s_bc[i] = nt_cell_s(&r->bc_glu, i, BC_RIBBONS, r->occ_inv_sat);
        r->bc_sbar[i] += dt / AM_DESENS_TAU * (r->s_bc[i] - r->bc_sbar[i]);
    }
    for (uint32_t a = 0; a < r->n_aii; a++) r->s_aii[a] = nt_cell_s(&r->aii_gly, a, AII_SITES, r->occ_inv_sat_gly);
    for (uint32_t a = 0; a < r->n_am; a++) r->s_am[a] = nt_cell_s(&r->am_gaba, a, AM_SITES, r->occ_inv_sat_gaba);

    /* ---- 8. astrocytes sense their patch's glutamate and modulate it ---- */
    if (r->n_astro && (t_ms % ASTRO_PERIOD_MS) == 0) {
        const float dts = ASTRO_PERIOD_MS / 1000.0F;
        for (uint32_t a = 0; a < r->n_astro; a++) {
            astrocyte_t* as = r->astro[a];
            float load = 0.0F;
            for (uint32_t k = 0; k < as->num_covered_synapses; k++) load += r->s_bc[as->covered_synapse_ids[k]];
            load /= (float)(as->num_covered_synapses ? as->num_covered_synapses : 1);
            r->astro_load[a] += 0.1F * (load - r->astro_load[a]);
            float stim = ASTRO_STIM_GAIN * fmaxf(0.0F, r->astro_load[a] - 0.5F);
            astrocyte_update_calcium(as, dts, stim);
            /* the gliotransmitter pool refills inside astrocyte_update_calcium
             * (fixed upstream 2026-11-23; the local refill workaround is gone) */
            float glio = as->num_covered_synapses ? astrocyte_compute_glutamate_release(as, 0) : 0.0F;
            float uptake = 1.0F / (1.0F + ASTRO_UPTAKE_K * r->astro_load[a] * as->atp_level);
            r->astro_gain[a] = uptake * (1.0F + ASTRO_GLIO_K * glio / 0.15F);
        }
        for (uint32_t i = 0; i < r->n_bc; i++) r->bc_gain[i] = r->astro_gain[r->bc_astro[i]];
    }

    /* ---- 9. optic nerve ---- */
    if (r->nerve) optic_nerve_step(r->nerve, r->spk_now, r->lgn_spk, dt);

    r->t_us = t_us + (uint64_t)llroundf(dt * 1000.0F);
}

int retina_circuit_see(retina_circuit_t* r, const uint8_t* pixels, uint32_t w,
                       uint32_t h, uint32_t ch, float duration_ms, retina_output_t* out)
{
    if (!r || !pixels || w == 0 || h == 0 || (ch != 1 && ch != 3 && ch != 4) || duration_ms <= 0.0F)
        return -1;
    retina_circuit_heartbeat("see", 0.0F);
    double t0 = wall_now_ms();

    sample_frame(r, pixels, w, h, ch);
    memset(r->rgc_spk, 0, r->n_rgc * sizeof(uint16_t));
    memset(r->lgn_spk, 0, r->n_rgc * sizeof(uint16_t));
    memset(r->pr_r_acc, 0, r->n_pr * sizeof(float));
    optic_nerve_stats_t ns0;
    memset(&ns0, 0, sizeof(ns0));
    if (r->nerve) optic_nerve_get_stats(r->nerve, &ns0);

    uint32_t steps = (uint32_t)lroundf(duration_ms / r->cfg.dt_ms);
    if (steps == 0) steps = 1;
    for (uint32_t s = 0; s < steps; s++) retina_step(r);
    float dur_s = (float)steps * r->cfg.dt_ms / 1000.0F;

    for (uint32_t g = 0; g < r->n_rgc; g++) r->rgc_rate[g] = (float)r->rgc_spk[g] / dur_s;
    if (r->nerve) optic_nerve_update_myelination(r->nerve, r->rgc_rate, dur_s);

    /* maps */
    uint32_t nmap = r->cfg.map_w * r->cfg.map_h;
    for (uint32_t m = 0; m < nmap; m++) {
        r->on_map[m] = csr_dot(&r->map_on, m, r->rgc_rate);
        r->off_map[m] = csr_dot(&r->map_off, m, r->rgc_rate);
    }

    if (out) {
        memset(out, 0, sizeof(*out));
        out->n_rgc = r->n_rgc;
        out->rgc_spikes = r->rgc_spk;
        out->lgn_spikes = r->lgn_spk;
        out->rgc_type = r->rgc_type;
        out->rgc_x = r->rgc_x;
        out->rgc_y = r->rgc_y;
        out->rgc_center_cone = r->rgc_cone;
        for (uint32_t g = 0; g < r->n_rgc; g++) {
            out->type_rate_hz[r->rgc_type[g]] += r->rgc_rate[g];
            out->type_count[r->rgc_type[g]]++;
            out->lgn_total += r->lgn_spk[g];
        }
        for (int t = 0; t < RETINA_RGC_TYPE_COUNT; t++)
            if (out->type_count[t]) out->type_rate_hz[t] /= (float)out->type_count[t];
        out->map_w = r->cfg.map_w;
        out->map_h = r->cfg.map_h;
        out->on_map = r->on_map;
        out->off_map = r->off_map;
        uint32_t nt[RETINA_PR_TYPE_COUNT] = { 0 };
        double lum = 0.0; uint32_t nc = 0;
        for (uint32_t p = 0; p < r->n_pr; p++) {
            uint8_t t = r->pr_type[p];
            out->cone_type_activation[t] += r->pr_r_acc[p] / (float)steps;
            nt[t]++;
            if (t != RETINA_PR_ROD) { lum += r->pr_A[p]; nc++; }
        }
        float cone_sum = 0.0F; uint32_t cone_n = 0;
        for (int t = 0; t < RETINA_PR_TYPE_COUNT; t++) {
            if (t != RETINA_PR_ROD) { cone_sum += out->cone_type_activation[t]; cone_n += nt[t]; }
            if (nt[t]) out->cone_type_activation[t] /= (float)nt[t];
        }
        out->rod_activation = out->cone_type_activation[RETINA_PR_ROD];
        out->cone_activation = cone_n ? cone_sum / (float)cone_n : 0.0F;
        out->luminance_estimate = nc ? (float)(lum / nc) : 0.0F;
        if (r->nerve) {
            optic_nerve_stats_t ns1;
            optic_nerve_get_stats(r->nerve, &ns1);
            double l0 = (double)ns0.mean_latency_ms * (double)ns0.spikes_delivered;
            double l1 = (double)ns1.mean_latency_ms * (double)ns1.spikes_delivered;
            uint64_t dn = ns1.spikes_delivered - ns0.spikes_delivered;
            out->lgn_mean_latency_ms = dn ? (float)((l1 - l0) / (double)dn) : 0.0F;
        }
        out->duration_ms = (float)steps * r->cfg.dt_ms;
        out->sim_time_ms = (double)r->t_us / 1000.0;
        out->wall_ms = wall_now_ms() - t0;
    }
    retina_circuit_heartbeat("see", 1.0F);
    return 0;
}

int retina_circuit_set_illuminance(retina_circuit_t* r, float illuminance)
{
    if (!r || !(illuminance >= 0.0F) || !isfinite(illuminance)) return -1;
    r->cfg.illuminance = illuminance;
    return 0;
}

int retina_circuit_get_illuminance(const retina_circuit_t* r, float* out)
{
    if (!r || !out) return -1;
    *out = r->cfg.illuminance;
    return 0;
}

optic_nerve_t* retina_circuit_get_optic_nerve(retina_circuit_t* r)
{
    return r ? r->nerve : NULL;
}

int retina_circuit_get_counts(const retina_circuit_t* r, retina_circuit_counts_t* o)
{
    if (!r || !o) return -1;
    memset(o, 0, sizeof(*o));
    o->n_photoreceptors = r->n_pr;
    for (uint32_t p = 0; p < r->n_pr; p++) o->n_cone_type[r->pr_type[p]]++;
    o->n_rods = o->n_cone_type[RETINA_PR_ROD];
    o->n_cones = r->n_pr - o->n_rods;
    o->n_horizontal = r->n_h;
    o->n_rod_bipolar = r->n_rodbc;
    o->n_bipolar_on = r->n_midget + r->n_diffuse + r->n_rodbc;
    o->n_bipolar_off = r->n_midget + r->n_diffuse;
    o->n_aii = r->n_aii;
    o->n_amacrine = r->n_aii + r->n_am;
    o->n_rgc = r->n_rgc;
    for (uint32_t g = 0; g < r->n_rgc; g++) o->n_rgc_type[r->rgc_type[g]]++;
    o->n_astrocytes = r->n_astro;
    if (r->nerve) {
        optic_nerve_stats_t ns;
        optic_nerve_get_stats(r->nerve, &ns);
        o->n_oligodendrocytes = ns.n_oligodendrocytes;
    }
    o->n_synapse_sites = r->pr_glu.n_sites + r->h_gaba.n_sites + r->bc_glu.n_sites +
                         r->aii_gly.n_sites + r->am_gaba.n_sites;
    o->n_spines = r->n_spines;
    o->n_gap_junctions = r->n_gap;
    return 0;
}

//=============================================================================
// Persistence: photoreceptor adaptation + graded-cell voltages + myelination
//=============================================================================

int retina_circuit_save(const retina_circuit_t* r, const char* path)
{
    if (!r || !path) return -1;
    /* Write to path.tmp and rename on success: a kill mid-save must not leave
     * a truncated .retina in place of the last good one. */
    size_t tlen = strlen(path) + 5;
    char* tmp = (char*)nimcp_malloc(tlen);
    if (!tmp) return -1;
    snprintf(tmp, tlen, "%s.tmp", path);
    FILE* fp = fopen(tmp, "wb");
    if (!fp) { nimcp_free(tmp); return -1; }
    uint32_t hdr[8] = { RETINA_MAGIC, RETINA_VERSION, r->n_pr, r->n_h, r->n_bc, r->n_aii,
                        r->n_rgc, r->nerve ? 1u : 0u };
    int ok = fwrite(hdr, sizeof(hdr), 1, fp) == 1;
    ok = ok && fwrite(&r->t_us, sizeof(r->t_us), 1, fp) == 1;
    ok = ok && fwrite(r->pr_A, sizeof(float), r->n_pr, fp) == r->n_pr;
    ok = ok && fwrite(r->pr_r, sizeof(float), r->n_pr, fp) == r->n_pr;
    ok = ok && fwrite(r->h_V, sizeof(float), r->n_h, fp) == r->n_h;
    ok = ok && fwrite(r->bc_V, sizeof(float), r->n_bc, fp) == r->n_bc;
    ok = ok && fwrite(r->aii_V, sizeof(float), r->n_aii, fp) == r->n_aii;
    if (ok && r->nerve) ok = optic_nerve_write(r->nerve, fp) == 0;
    if (fclose(fp) != 0) ok = 0;
    if (ok && rename(tmp, path) != 0) ok = 0;
    if (!ok) remove(tmp);
    nimcp_free(tmp);
    return ok ? 0 : -1;
}

int retina_circuit_load(retina_circuit_t* r, const char* path)
{
    if (!r || !path) return -1;
    FILE* fp = fopen(path, "rb");
    if (!fp) return -1;
    uint32_t hdr[8];
    int ok = fread(hdr, sizeof(hdr), 1, fp) == 1 && hdr[0] == RETINA_MAGIC &&
             hdr[1] == RETINA_VERSION && hdr[2] == r->n_pr && hdr[3] == r->n_h &&
             hdr[4] == r->n_bc && hdr[5] == r->n_aii && hdr[6] == r->n_rgc;
    uint64_t t_us = 0;
    /* Read everything into scratch first and commit only after the whole file
     * (nerve included) has read back: a truncated sidecar must leave the live
     * retina untouched, not half-overwritten. */
    size_t n_all = (size_t)2 * r->n_pr + r->n_h + r->n_bc + r->n_aii;
    float* buf = ok ? (float*)nimcp_malloc((n_all ? n_all : 1) * sizeof(float)) : NULL;
    if (!buf) ok = 0;
    float* pr_A = buf;
    float* pr_r = pr_A ? pr_A + r->n_pr : NULL;
    float* h_V = pr_r ? pr_r + r->n_pr : NULL;
    float* bc_V = h_V ? h_V + r->n_h : NULL;
    float* aii_V = bc_V ? bc_V + r->n_bc : NULL;
    ok = ok && fread(&t_us, sizeof(t_us), 1, fp) == 1;
    ok = ok && fread(pr_A, sizeof(float), r->n_pr, fp) == r->n_pr;
    ok = ok && fread(pr_r, sizeof(float), r->n_pr, fp) == r->n_pr;
    ok = ok && fread(h_V, sizeof(float), r->n_h, fp) == r->n_h;
    ok = ok && fread(bc_V, sizeof(float), r->n_bc, fp) == r->n_bc;
    ok = ok && fread(aii_V, sizeof(float), r->n_aii, fp) == r->n_aii;
    /* optic_nerve_read is itself all-or-nothing, and it is the last read, so
     * a failure here still leaves the retina arrays uncommitted. */
    if (ok && hdr[7] && r->nerve) ok = optic_nerve_read(r->nerve, fp) == 0;
    fclose(fp);
    if (ok) {
        memcpy(r->pr_A, pr_A, r->n_pr * sizeof(float));
        memcpy(r->pr_r, pr_r, r->n_pr * sizeof(float));
        memcpy(r->h_V, h_V, r->n_h * sizeof(float));
        memcpy(r->bc_V, bc_V, r->n_bc * sizeof(float));
        memcpy(r->aii_V, aii_V, r->n_aii * sizeof(float));
    }
    nimcp_free(buf);
    if (!ok) return -1;
    /* the nerve and STP clocks keep running forward; only the retina clock is restored
       if it is ahead, so stp timestamps never go backwards */
    if (t_us > r->t_us) r->t_us = t_us;
    return 0;
}
