/**
 * @file nimcp_snn_membrane.h
 * @brief SNN membrane integration — current-mode and conductance-mode kernels.
 *
 * Single-responsibility module owning the LIF membrane equation. Both the
 * legacy current-based and the new conductance-based paths share the same
 * voltage clamp, NaN guards, and integration scheme — this module is the
 * single source of truth for the math.
 *
 * P0 update (per-receptor split): conductance mode now uses four buckets
 * (AMPA / NMDA / GABA_A / GABA_B) with receptor-specific decay τ and
 * reversal potentials. NMDA additionally has a voltage-dependent Mg²⁺
 * block, which is what makes it a slow modulator + coincidence detector
 * rather than just slow AMPA. See docs/claude/cb-phase0-design.md and the
 * P0 design notes in nimcp_snn_types.h for the full migration design.
 *
 * The functions are header-inline `static inline` so the SNN hot loop pays
 * no call overhead. Pure functions of their inputs (no globals, no I/O) so
 * they are trivially unit-testable and thread-safe.
 */

#ifndef NIMCP_SNN_MEMBRANE_H
#define NIMCP_SNN_MEMBRANE_H

#include <stdbool.h>
#include <math.h>
#include "core/synapse_types/nimcp_synapse_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Compute NMDA Mg²⁺ block factor at a given membrane voltage.
 *
 * Standard Jahr-Stevens (1990) formula:
 *     m(V) = 1 / (1 + [Mg²⁺] · exp(-V / 16.13) / 3.57)
 *
 * Numerical values at [Mg²⁺] = 1 mM (the parameterization here uses slope
 * 1/16.13 mV⁻¹ and normalization 3.57; alternate parameterizations in the
 * literature use 1/12.5 / 3.0 and yield slightly different numbers — pin
 * to whichever the kernel actually computes, not to round biological
 * mnemonics):
 *   V = -65 mV  →  m ≈ 0.06   (heavily blocked, near rest)
 *   V = -40 mV  →  m ≈ 0.23   (partially unblocked)
 *   V =   0 mV  →  m ≈ 0.78   (mostly unblocked at peak)
 *   V = +30 mV  →  m ≈ 0.95
 *
 * This is what makes NMDA a coincidence detector: it requires the post-
 * synaptic neuron to already be depolarized (by AMPA, typically) before
 * its conductance becomes effective. Pure NMDA input at rest does ~nothing.
 *
 * @param v_mv   Membrane voltage (mV)
 * @param mg_mm  Extracellular Mg²⁺ concentration (mM); pass 0 to disable
 * @return Block-relief factor in [0, 1]; multiply g_nmda by this.
 */
static inline float snn_membrane_nmda_mg_block(float v_mv, float mg_mm) {
    if (mg_mm <= 0.0f) return 1.0f;  /* user disabled V-gating */
    /* Clamp the exponent argument to avoid overflow at extreme V. The
     * function asymptotes anyway: V → +∞ → block_factor → 1, V → -∞
     * → block_factor → 0. We only need numerical stability. */
    float arg = -v_mv / 16.13f;
    if (arg >  20.0f) arg =  20.0f;   /* expf(20) ≈ 4.85e8, plenty */
    if (arg < -20.0f) arg = -20.0f;
    return 1.0f / (1.0f + mg_mm * expf(arg) / 3.57f);
}

/**
 * @brief Compute one timestep of LIF membrane voltage change (dv).
 *
 * Selects current- or conductance-based dynamics by @p conductance_mode.
 * Same equation tail (leak + drive / tau_eff * dt) in both branches; only
 * the synaptic-drive term differs.
 *
 * Current mode:    dv = ( (v_rest - v) + i_syn )                       / tau * dt
 * Conductance:     dv = ( (v_rest - v)
 *                       + g_ampa  * (E_ampa  - v)
 *                       + g_nmda  * mg_block(v) * (E_nmda - v)
 *                       + g_gaba_a * (E_gaba_a - v)
 *                       + g_gaba_b * (E_gaba_b - v) )                  / tau * dt
 *
 * Any g_* may be 0; that receptor contributes nothing.
 *
 * @param v               Current membrane voltage (mV)
 * @param v_rest          Resting potential (mV)
 * @param tau_eff_ms      Effective membrane time constant (ms; > 0)
 * @param dt_ms           Integration timestep (ms; > 0)
 * @param i_syn           Total synaptic current (current mode only)
 * @param g_ampa          AMPA conductance (CB mode)
 * @param g_nmda          NMDA conductance (CB mode; subject to Mg²⁺ block)
 * @param g_gaba_a        GABA_A conductance (CB mode)
 * @param g_gaba_b        GABA_B conductance (CB mode)
 * @param e_ampa          AMPA reversal potential (mV)
 * @param e_nmda          NMDA reversal potential (mV)
 * @param e_gaba_a        GABA_A reversal potential (mV)
 * @param e_gaba_b        GABA_B reversal potential (mV)
 * @param mg_mm           Extracellular Mg²⁺ concentration (mM); ~1.0 normal
 * @param conductance_mode Select CB (true) or legacy current (false)
 * @return dv (mV)
 *
 * Pure function. No NaN/Inf produced for finite inputs with tau_eff > 0.
 */
static inline float snn_membrane_compute_dv(
    float v, float v_rest,
    float tau_eff_ms, float dt_ms,
    float i_syn,
    float g_ampa, float g_nmda, float g_gaba_a, float g_gaba_b,
    float e_ampa, float e_nmda, float e_gaba_a, float e_gaba_b,
    float mg_mm,
    bool conductance_mode)
{
    if (tau_eff_ms < 1e-3f) tau_eff_ms = 1e-3f;

    const float leak = (v_rest - v);
    float drive;
    if (conductance_mode) {
        const float nmda_block = snn_membrane_nmda_mg_block(v, mg_mm);
        drive = leak
              + g_ampa             * (e_ampa   - v)
              + g_nmda * nmda_block * (e_nmda  - v)
              + g_gaba_a           * (e_gaba_a - v)
              + g_gaba_b           * (e_gaba_b - v);
    } else {
        drive = leak + i_syn;
    }
    float dv = drive * (dt_ms / tau_eff_ms);

    /* Biophysical clamp: a single integration step should not move the
     * membrane by more than ±100 mV. Real cortical neurons swing 60-80 mV
     * across an action potential cycle; values above 100 mV per dt
     * indicate pathological tau collapse, runaway conductance, or an
     * unrescaled-weight CB run. */
    if (dv >  100.0f) dv =  100.0f;
    if (dv < -100.0f) dv = -100.0f;

    /* CB-mode reversal-potential bound: V cannot cross any reversal
     * potential. Take the most extreme excitatory and inhibitory bounds
     * across active receptors. */
    if (conductance_mode) {
        const float v_after = v + dv;
        const float e_max = (e_ampa > e_nmda) ? e_ampa : e_nmda;
        const float e_min = (e_gaba_a < e_gaba_b) ? e_gaba_a : e_gaba_b;
        if (v_after > e_max) dv = e_max - v;
        if (v_after < e_min) dv = e_min - v;
    }
    return dv;
}

/**
 * @brief Decay one neuron's four per-receptor conductances by precomputed factors.
 *
 * Caller computes the four decay factors once per population step (NOT
 * per neuron) to avoid expf() in the inner loop:
 *   decay_X = expf(-dt / tau_X)   for X in {ampa, nmda, gaba_a, gaba_b}.
 *
 * Any pointer may be NULL — that receptor's decay is skipped, matching
 * the pop-level "alloc failed → silent degrade" contract.
 */
static inline void snn_membrane_decay_one(
    float* g_ampa,   float* g_nmda,
    float* g_gaba_a, float* g_gaba_b,
    float decay_ampa,   float decay_nmda,
    float decay_gaba_a, float decay_gaba_b)
{
    if (g_ampa)   *g_ampa   *= decay_ampa;
    if (g_nmda)   *g_nmda   *= decay_nmda;
    if (g_gaba_a) *g_gaba_a *= decay_gaba_a;
    if (g_gaba_b) *g_gaba_b *= decay_gaba_b;
}

/**
 * @brief Deposit a synaptic weight into the correct conductance bucket
 *        for its receptor type, or into i_syn in legacy current mode.
 *
 * Receptor routing in CB mode:
 *   SYNAPSE_AMPA           → g_ampa   += weight   (excitatory, sign-preserved)
 *   SYNAPSE_NMDA           → g_nmda   += weight
 *   SYNAPSE_GABA_A         → g_gaba_a += |weight|  (inhibitory)
 *   SYNAPSE_GABA_B         → g_gaba_b += |weight|
 *   SYNAPSE_GENERIC + others (DOPAMINE / SEROTONIN / ACETYLCHOLINE / ...)
 *                          → fall back to weight-sign routing into AMPA / GABA_A
 *                            so unconnected pops + cognitive bridges that don't
 *                            specify a type stay functional.
 *
 * Caller passes the synapse_type from the per-pop-pair table set up at
 * wiring time (see snn_population_t::synapse_type_per_src).
 *
 * @param i_syn            In/out: current-mode accumulator (may be NULL in CB mode)
 * @param g_ampa           In/out: AMPA conductance (may be NULL)
 * @param g_nmda           In/out: NMDA conductance (may be NULL)
 * @param g_gaba_a         In/out: GABA_A conductance (may be NULL)
 * @param g_gaba_b         In/out: GABA_B conductance (may be NULL)
 * @param weight           Effective weight (already depression-modulated)
 * @param syn_type         Receptor type (synapse_type_t cast to int)
 * @param conductance_mode Select CB (true) or legacy current (false)
 */
static inline void snn_membrane_deposit_synapse(
    float* i_syn,
    float* g_ampa, float* g_nmda, float* g_gaba_a, float* g_gaba_b,
    float weight,
    int syn_type,
    bool conductance_mode)
{
    if (!conductance_mode) {
        if (i_syn) *i_syn += weight;
        return;
    }
    switch (syn_type) {
        case SYNAPSE_AMPA:
            if (g_ampa) *g_ampa += weight;
            break;
        case SYNAPSE_NMDA:
            if (g_nmda) *g_nmda += weight;
            break;
        case SYNAPSE_GABA_A:
            if (g_gaba_a) *g_gaba_a += (weight < 0.0f) ? -weight : weight;
            break;
        case SYNAPSE_GABA_B:
            if (g_gaba_b) *g_gaba_b += (weight < 0.0f) ? -weight : weight;
            break;
        default:
            /* SYNAPSE_GENERIC + neuromodulators + ELECTRICAL: fall back
             * to sign-routing so unconnected sources and cognitive bridges
             * that didn't set a type still work. */
            if (weight >= 0.0f) {
                if (g_ampa) *g_ampa += weight;
            } else {
                if (g_gaba_a) *g_gaba_a += -weight;
            }
            break;
    }
}

/*=============================================================================
 * Wave H — Two-compartment dendritic helpers (basal + apical + NMDA plateau)
 *=============================================================================
 * Adds an OPTIONAL second integration compartment per neuron so apical NMDA
 * plateau spikes can be modelled separately from basal/soma firing. The
 * single-compartment `snn_membrane_compute_dv` above is preserved unchanged
 * for the OFF-path and for interneurons (PV/SOM/VIP), which biology models
 * as single-compartment.
 *
 * Receptor → compartment routing (locked, see wave-h-dendritic-design):
 *   AMPA, GABA_A → basal soma
 *   NMDA, GABA_B → apical tuft
 *
 * Plateau drive: Heaviside(V_apical - V_threshold) × plateau_gain ×
 *                exp(-(t - t0) / tau_plateau).
 * V_threshold = -40 mV, tau_plateau = 50 ms, plateau_gain = 0.5 (defaults).
 */

/** Default Wave H plateau parameters — exposed for tests. */
#define SNN_DEND_V_PLATEAU_THRESHOLD_MV   (-40.0f)
#define SNN_DEND_PLATEAU_GAIN              (0.5f)
#define SNN_DEND_PLATEAU_TAU_MS            (50.0f)
/** Inter-compartmental electrotonic coupling. ~0.05 — strong enough to
 *  carry an apical plateau into the soma but small enough to keep the
 *  compartments distinct. Capped at 0.2 by the doc's failure-mode note. */
#define SNN_DEND_G_COUP_DEFAULT            (0.05f)

/**
 * @brief Detect plateau onset: apical V crosses V_threshold.
 *
 * Caller is responsible for tracking already-active state — this helper
 * tells you whether the apical compartment crossed threshold THIS step.
 * Real plateau machinery sets `plateau_active=1` and `t0=tick` at onset
 * and clears `plateau_active=0` when drive decays below 0.05 of peak.
 *
 * Pure function; no globals.
 */
static inline bool snn_membrane_check_plateau_onset(float v_apical, float v_threshold) {
    return v_apical >= v_threshold;
}

/**
 * @brief Compute one-step dv for a two-compartment neuron (basal + apical).
 *
 * Locked equations (from wave-h-dendritic-design):
 *
 *   dv_basal  = ( (v_rest - v_basal)
 *               + g_ampa_b * (E_ampa - v_basal)
 *               + g_gaba_a_b * (E_gaba_a - v_basal)
 *               + g_coup * (v_apical - v_basal)
 *             ) / tau_basal * dt
 *
 *   dv_apical = ( (v_rest - v_apical)
 *               + g_nmda_a * mg_block(v_apical) * (E_nmda - v_apical)
 *               + g_gaba_b_a * (E_gaba_b - v_apical)
 *               + g_coup * (v_basal - v_apical)
 *               + plateau_drive
 *             ) / tau_apical * dt
 *
 *   plateau_drive = (plateau_active ? plateau_gain * exp(-t_since_onset/tau_plateau) : 0)
 *
 * Soma threshold check is on basal V only (the spike output of the cell).
 *
 * Each dv is independently bounded by ±100 mV per step, matching the
 * single-compartment biophysical clamp. Reversal-potential bounding is
 * NOT enforced inside this helper — apical V can briefly cross E_nmda
 * via plateau drive (a known biological reality at peak NMDA spike) and
 * basal V can cross E_ampa via electrotonic coupling. The caller's outer
 * loop applies the simple ±100 mV per-step swing limit which is enough
 * for stability without distorting BAC firing.
 *
 * Pure function. No NaN/Inf for finite inputs with tau > 0.
 */
static inline void snn_membrane_compute_dv_two_compartment(
    const float v_basal, const float v_apical,
    const float v_rest, const float tau_b, const float tau_a, const float dt,
    const float g_ampa_b, const float g_gaba_a_b,
    const float g_nmda_a, const float g_gaba_b_a,
    const float g_coup,
    const float e_ampa, const float e_nmda,
    const float e_gaba_a, const float e_gaba_b,
    const float mg_mm,
    const bool plateau_active,
    const float t_since_plateau_onset,
    const float plateau_gain,
    const float tau_plateau,
    float* dv_basal_out, float* dv_apical_out)
{
    float tau_b_eff = (tau_b < 1e-3f) ? 1e-3f : tau_b;
    float tau_a_eff = (tau_a < 1e-3f) ? 1e-3f : tau_a;
    float tau_p_eff = (tau_plateau < 1e-3f) ? 1e-3f : tau_plateau;

    /* === Basal compartment === */
    const float drive_b = (v_rest - v_basal)
                        + g_ampa_b   * (e_ampa   - v_basal)
                        + g_gaba_a_b * (e_gaba_a - v_basal)
                        + g_coup     * (v_apical - v_basal);
    float dv_b = drive_b * (dt / tau_b_eff);
    if (dv_b >  100.0f) dv_b =  100.0f;
    if (dv_b < -100.0f) dv_b = -100.0f;

    /* === Apical compartment === */
    const float nmda_block = snn_membrane_nmda_mg_block(v_apical, mg_mm);
    /* Plateau drive — Heaviside × decay. plateau_active is an external
     * flag tracked by the caller (set on first crossing, cleared when
     * drive below 0.05 of peak). One Heaviside per neuron per step:
     * the inner drive does NOT add a separate H(V - V_threshold) term
     * because the active-flag IS the latched Heaviside. */
    float plateau_drive = 0.0f;
    if (plateau_active) {
        float arg = -t_since_plateau_onset / tau_p_eff;
        if (arg < -50.0f) arg = -50.0f;  /* expf saturation guard */
        plateau_drive = plateau_gain * expf(arg);
    }
    const float drive_a = (v_rest - v_apical)
                        + g_nmda_a   * nmda_block * (e_nmda   - v_apical)
                        + g_gaba_b_a              * (e_gaba_b - v_apical)
                        + g_coup                  * (v_basal  - v_apical)
                        + plateau_drive;
    float dv_a = drive_a * (dt / tau_a_eff);
    if (dv_a >  100.0f) dv_a =  100.0f;
    if (dv_a < -100.0f) dv_a = -100.0f;

    if (dv_basal_out)  *dv_basal_out  = dv_b;
    if (dv_apical_out) *dv_apical_out = dv_a;
}

/**
 * @brief Wave H deposit helper — routes a synapse weight to the correct
 *        compartment based on receptor type.
 *
 * Routing (locked):
 *   AMPA   → g_ampa_basal
 *   GABA_A → g_gaba_a_basal
 *   NMDA   → g_nmda_apical
 *   GABA_B → g_gaba_b_apical
 *   GENERIC + neuromodulators → sign-routed (positive→AMPA-basal,
 *                                            negative→GABA_A-basal)
 *
 * When `dendritic_enabled` is false this simply forwards to the legacy
 * single-compartment `snn_membrane_deposit_synapse` so the OFF-path is
 * bit-identical.
 *
 * The compartment decision is made ONCE per deposit (per the SOLID
 * constraint — never inside a per-synapse inner loop branch on the flag).
 */
static inline void snn_membrane_deposit_synapse_compartmental(
    float* i_syn,
    /* legacy single-compartment buckets (used when dendritic_enabled=false) */
    float* g_ampa,  float* g_nmda,  float* g_gaba_a,  float* g_gaba_b,
    /* basal compartment (used when dendritic_enabled=true) */
    float* g_ampa_basal,    float* g_gaba_a_basal,
    /* apical compartment (used when dendritic_enabled=true) */
    float* g_nmda_apical,   float* g_gaba_b_apical,
    float weight,
    int syn_type,
    bool conductance_mode,
    bool dendritic_enabled)
{
    if (!dendritic_enabled) {
        snn_membrane_deposit_synapse(i_syn,
            g_ampa, g_nmda, g_gaba_a, g_gaba_b,
            weight, syn_type, conductance_mode);
        return;
    }
    /* Dendritic path: receptor → compartment routing per locked table. */
    if (!conductance_mode) {
        /* No conductance buckets in current mode — same fallback as
         * the legacy helper: deposit into i_syn. */
        if (i_syn) *i_syn += weight;
        return;
    }
    switch (syn_type) {
        case SYNAPSE_AMPA:
            if (g_ampa_basal) *g_ampa_basal += weight;
            break;
        case SYNAPSE_NMDA:
            if (g_nmda_apical) *g_nmda_apical += weight;
            break;
        case SYNAPSE_GABA_A:
            if (g_gaba_a_basal)
                *g_gaba_a_basal += (weight < 0.0f) ? -weight : weight;
            break;
        case SYNAPSE_GABA_B:
            if (g_gaba_b_apical)
                *g_gaba_b_apical += (weight < 0.0f) ? -weight : weight;
            break;
        default:
            /* GENERIC / neuromodulators / ELECTRICAL — sign-routed to
             * the basal soma (matches single-compartment helper). */
            if (weight >= 0.0f) {
                if (g_ampa_basal) *g_ampa_basal += weight;
            } else {
                if (g_gaba_a_basal) *g_gaba_a_basal += -weight;
            }
            break;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_MEMBRANE_H */
