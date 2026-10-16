/**
 * @file nimcp_snn_training.c
 * @brief SNN Training Module Implementation
 *
 * WHAT: Training algorithms for spiking neural networks
 * WHY:  Enable learning in SNNs using biologically-plausible rules
 * HOW:  STDP, R-STDP, surrogate gradients, and eProp
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 *
 * @author NIMCP Team
 * @date 2024
 */

#include "snn/nimcp_snn_training.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_synapse.h"  /* for snn_csr_storage_t, snn_csr_synapse_t */
#include "constants/nimcp_constants.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/tensor/nimcp_tensor_internal.h"
#include "utils/validation/nimcp_common.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "core/neuralnet/nimcp_sparse_synapse.h"
#include "core/substrate/nimcp_substrate_effects.h"  /* substrate_apply_lr */

/* Explicit includes for symbols used by the tunable-param section + the
 * intrinsic-reward computation below. Without these we rely on
 * transitive includes from earlier headers — fine today, but fragile. */
#include <string.h>  /* strncmp */
#include <math.h>    /* expf */
#include <stdlib.h>  /* rand_r (on glibc) */

/*============================================================================
 * Runtime-tunable SNN training parameters.
 *
 * Previously these were `const` locals inside functions — unchangeable
 * without a rebuild + brain restart. Promoted to file-scope globals
 * with setters so operators can adjust in real time over the daemon
 * socket (see brain_daemon.py's `tune_snn` command).
 *
 * Defaults match the last-committed values. Changes take effect
 * immediately on the next R-STDP / homeostatic invocation.
 *==========================================================================*/
static float g_rstdp_lr               = 0.0001f;  /* R-STDP learning rate */
static float g_rstdp_baseline_alpha   = 0.001f;   /* reward EMA alpha (slower = less whiplash) */
static float g_homeo_target_rate      = 0.03f;    /* biological 3% firing target */
static float g_homeo_min_scale        = 0.98f;    /* per-apply scale-down floor */
static float g_homeo_max_scale        = 1.02f;    /* per-apply scale-up ceiling (normal) */
static float g_homeo_max_scale_dead   = 1.05f;    /* scale-up ceiling for pops < 10% of target */
static float g_homeo_dead_threshold   = 0.1f;     /* "dead" multiplier: rate < X × target → use max_scale_dead */
static float g_metabolic_cap_factor   = 0.8f;     /* sum(|w|) cap = factor × fan_in */
/* Poisson background noise + intrinsic reward — the real structural
 * fixes for SNN mode collapse. See commit message for rationale. */
/* Noise defaults tuned empirically on pod 2026-04-21: 1.0 × 15.0 was
 * 50-100× too weak to rescue collapsed pops (steady-state contribution
 * ~0.3mV vs 15mV threshold gap). 20 × 30 gives ~3mV per pulse at
 * ~2 pulses/tau, drives ~1-2% baseline firing across all pops. */
static float g_snn_noise_rate_hz      = 20.0f;    /* Poisson baseline firing floor; 0 = disabled */
static float g_snn_noise_pulse_mv     = 30.0f;    /* mV kick injected into I_syn per noise spike */
static float g_snn_intrinsic_alpha    = 0.8f;     /* reward mix: 1.0 = pure intrinsic (rate-match), 0 = pure ANN-loss */
/* Structural issue #4: per-layer firing targets. Input pops are driven
 * by external cortex current at high rate, so 3% is unnaturally low for
 * them — they sit at 10-40% naturally, and forcing them to 3% collapses
 * the cascade. Hidden + output pops should track 3% biological target.
 * Pop name prefix selects the target. */
static float g_target_rate_input      = 0.05f;    /* input_* pops — looser since they're cortex-driven */
/* g_homeo_target_rate (defined above) stays at 0.03 as the default for
 * everything else (L1_feature_*, L2_pattern_*, ..., output_*). */

/* Anti-reward: when a population's firing rate exceeds
 * threshold_ratio × target, the intrinsic reward goes negative so
 * R-STDP can push the offending synapses down. Without this, reward
 * saturates at 0 once rate >> target and R-STDP is powerless. */
static float g_snn_anti_reward_enabled         = 1.0f;  /* 1.0 = on */
static float g_snn_anti_reward_threshold_ratio = 2.0f;  /* trigger at rate > 2×target */
static float g_snn_anti_reward_gain            = 0.5f;  /* (rate-2×target)/target × gain */

/* Short-term synaptic depression dynamics. Per-spike bump adds
 * g_snn_dep_inc; recovery decays exponentially with tau g_snn_dep_tau_ms.
 * Biology's STD acts on ms timescale — our prior hardcoded decay (0.95
 * per step at dt=1ms) corresponded to an effective tau of ~19ms, which
 * is in range but inflexible. Now runtime-tunable. */
static float g_snn_dep_inc    = 0.3f;    /* per-spike depression increment (was ~0.05) */
static float g_snn_dep_tau_ms = 50.0f;   /* exponential recovery time constant */
static float g_snn_dep_cap    = 0.5f;    /* max depression; effective drive = 1 - depression */

/* Biophysical stability mechanisms — each gated by its own runtime
 * enable knob. All default-enabled so out-of-the-box the SNN has
 * biological stability. Disable by setting the enable knob to 0.
 *
 * AHP (M-current)  : fast spike-rate adaptation (~150 ms, ~0.6 mV).
 * Na/K pump        : slow hyperpolarization (~5 s, ~0.05 mV).
 * Basket pool      : fast-spiking inhibitory interneurons (~20%).
 * E/I noise ratio  : fraction of Poisson noise pulses that are
 *                    inhibitory (0.5 = balanced; 0 = legacy exc-only). */
static float g_snn_ahp_enabled      = 1.0f;
static float g_snn_ahp_tau_ms       = 150.0f;
static float g_snn_ahp_gain_mv      = 0.6f;
static float g_snn_pump_enabled     = 1.0f;
static float g_snn_pump_tau_ms      = 5000.0f;
static float g_snn_pump_gain_mv     = 0.05f;
static float g_snn_basket_enabled   = 1.0f;
static float g_snn_basket_fraction  = 0.2f;
static float g_snn_noise_ei_ratio   = 0.5f;

/* Substrate adapter (Phase 1 biological-substrate wiring).
 *
 *   enabled            : master switch; when 0 the SNN ignores its
 *                        attached substrate entirely.
 *   update_period      : how many SNN steps between fresh
 *                        substrate_compute_effects() calls. 10 steps is
 *                        a good default — effects vary on ~100 ms
 *                        timescales, dt is typically 0.1-1 ms.
 *   spike_dropout_on   : 0 disables per-spike survival rolls (keeps all
 *                        spikes), 1 enables (may drop a fraction).
 *   plasticity_mod_on  : 0 disables substrate-modulated R-STDP LR
 *                        (uses static g_rstdp_lr), 1 enables.
 *
 * All flags normalize to exactly 0.0f / 1.0f on assignment. Period is
 * clamped to [1, 10000]. */
static float g_snn_substrate_enabled           = 1.0f;
static float g_snn_substrate_update_period     = 10.0f;
static float g_snn_substrate_spike_dropout_on  = 1.0f;
static float g_snn_substrate_plasticity_mod_on = 1.0f;
/* F8: biological feedback — when 1, AHP + Na/K pump hyperpolarization is
 * scaled by substrate pump_activity (ATP proxy). When 0, AHP/pump gains
 * use a hardcoded 1.0× factor (identity), matching pre-F8 behavior.
 * Biology: real Na/K-ATPase activity is directly ATP-dependent — low
 * ATP slows the pumps and weakens both AHP (pump-driven component) and
 * pump adaptation currents. Clamped inside the hot loop to [0.1, 1.0]
 * since real pumps don't stop entirely, they slow. */
static float g_snn_ahp_pump_substrate_coupling = 1.0f;

/* Conductance-based PSC migration (CB).
 *   conductance_enabled   : master switch. When 1.0, the SNN hot loop
 *                           routes synaptic input through g_exc/g_inh
 *                           with reversal-potential driving force,
 *                           saturating naturally and preventing
 *                           runaway. When 0.0, the legacy current-based
 *                           summation runs unchanged.
 *   cb_weights_rescaled   : sticky flag — set to 1.0 by
 *                           snn_rescale_weights_for_conductance() to
 *                           prevent double-application across restarts.
 *                           Defaults 0.0; persistence is the caller's
 *                           responsibility (see scripts/brain_daemon.py
 *                           snn_tune.json plumbing).
 *   e_exc / e_inh         : reversal potentials (mV). Defaults 0/-80
 *                           (cortical AMPA / GABA-A). Globals (not
 *                           per-config) so checkpoint binary layout is
 *                           unchanged.
 *   tau_exc / tau_inh     : conductance decay time constants (ms).
 *                           Defaults 2/8 (fast AMPA / slower GABA-A
 *                           gives I time to brake E).
 * See docs/claude/cb-phase0-design.md. */
static float g_snn_conductance_enabled = 0.0f;
static float g_snn_cb_weights_rescaled = 0.0f;
/* CB Phase 5 — GPU port runtime gate. Default ON.
 * When BOTH conductance_enabled AND cb_gpu_enabled are nonzero AND a GPU
 * is attached, the SNN step layer routes through the GPU CB hot path
 * (nimcp_gpu_lif_forward_cb). Default ON so when the deposit-pass refactor
 * + bit-identity test (CB-GPU-5) lands, no production config flip is
 * required — the daemon already has the desired routing baked in. Until
 * the GPU CB integration cutover ships, this knob is documentary at the
 * step layer (the gate at line ~970 still falls through to CPU when CB
 * is ON to preserve Phase 0-4 correctness).
 *
 * Set to 0 explicitly only to disable the future cutover, e.g. for
 * debugging or if a GPU regression forces a temporary CPU-only fallback. */
static float g_snn_cb_gpu_enabled = 1.0f;
/* P0 per-receptor reversal potentials (mV).
 * AMPA / NMDA share E_rev ≈ 0 mV (Na+/Ca2+ permeable, depolarizing).
 * GABA_A / GABA_B share E_rev ≈ -75 mV (Cl-/K+ permeable, hyperpolarizing).
 * The legacy g_snn_e_exc_mv / g_snn_e_inh_mv globals are KEPT as aliases
 * — getters return the AMPA/GABA_A value so Python `snn_tune_get_e_exc_mv`
 * still works. Setters update both AMPA and NMDA simultaneously (and both
 * GABA-A / GABA-B for the inh setter) for sane defaults. */
static float g_snn_e_ampa_mv   =   0.0f;
static float g_snn_e_nmda_mv   =   0.0f;
static float g_snn_e_gaba_a_mv = -75.0f;
static float g_snn_e_gaba_b_mv = -75.0f;
/* P0 per-receptor decay τ (ms). Standard cortical kinetics:
 *   AMPA   ≈   2 ms (fast excitatory)
 *   NMDA   ≈ 100 ms (slow excitatory, Mg²⁺-blocked)
 *   GABA_A ≈  10 ms (fast inhibitory)
 *   GABA_B ≈ 150 ms (slow inhibitory, GIRK-coupled K+ current) */
static float g_snn_tau_ampa_ms   =   2.0f;
static float g_snn_tau_nmda_ms   = 100.0f;
static float g_snn_tau_gaba_a_ms =  10.0f;
static float g_snn_tau_gaba_b_ms = 150.0f;
/* NMDA Mg²⁺ block: g_nmda_eff = g_nmda / (1 + [Mg]·exp(-V/16.13)/3.57)
 * Standard physiological [Mg²⁺] is ~1 mM extracellular. Tunable so
 * experiments can scale or zero it (zero → no V-gating, NMDA acts like
 * slow AMPA, useful for ablation tests). */
static float g_snn_nmda_mg_mm = 1.0f;

/* Wave H — dendritic compartment runtime flag (default OFF).
 *
 * When non-zero AT THE TIME OF POPULATION CREATION (in the hierarchical
 * wiring path), tier-pyramidal pops get pop->dendritic_enabled = true and
 * the 8 Wave H arrays allocated via snn_network_enable_dendritic().
 * Interneurons (PV/SOM/VIP/TRN) stay single-compartment regardless of
 * this flag.
 *
 * Like conductance_enabled, this must be set BEFORE brain init for the
 * default-OFF regression contract to flip at production scale. */
static float g_snn_dendritic_enabled = 0.0f;

/* Public setters — called from Python binding via tune_snn RPC. */
void snn_tune_set_rstdp_lr(float v)              { if (v > 0.0f && v < 1.0f)     g_rstdp_lr = v; }
void snn_tune_set_rstdp_baseline_alpha(float v)  { if (v > 0.0f && v <= 1.0f)    g_rstdp_baseline_alpha = v; }
void snn_tune_set_target_rate(float v)           { if (v > 0.0f && v < 0.5f)     g_homeo_target_rate = v; }
void snn_tune_set_homeo_bounds(float min_, float max_) {
    if (min_ > 0.5f && min_ < 1.0f) g_homeo_min_scale = min_;
    if (max_ > 1.0f && max_ < 2.0f) g_homeo_max_scale = max_;
}
void snn_tune_set_max_scale_dead(float v)        { if (v > 1.0f && v < 2.0f)     g_homeo_max_scale_dead = v; }
void snn_tune_set_dead_threshold(float v)        { if (v > 0.0f && v < 1.0f)     g_homeo_dead_threshold = v; }
void snn_tune_set_metabolic_cap(float v)         { if (v > 0.0f && v < 10.0f)    g_metabolic_cap_factor = v; }
void snn_tune_set_noise_rate_hz(float v)         { if (v >= 0.0f && v < 500.0f)  g_snn_noise_rate_hz = v; }
void snn_tune_set_noise_pulse_mv(float v)        { if (v > 0.0f && v < 200.0f)   g_snn_noise_pulse_mv = v; }
void snn_tune_set_intrinsic_alpha(float v)       { if (v >= 0.0f && v <= 1.0f)   g_snn_intrinsic_alpha = v; }
void snn_tune_set_target_rate_input(float v)     { if (v > 0.0f && v < 0.5f)     g_target_rate_input = v; }
/* Anti-reward setters. enabled: any nonzero treated as on.
 * threshold_ratio: must be > 1.0 and < 10.0. gain: must be >= 0.0 and < 10.0. */
void snn_tune_set_anti_reward_enabled(float v)         { g_snn_anti_reward_enabled = (v != 0.0f) ? 1.0f : 0.0f; }
void snn_tune_set_anti_reward_threshold_ratio(float v) { if (v > 1.0f && v < 10.0f)  g_snn_anti_reward_threshold_ratio = v; }
void snn_tune_set_anti_reward_gain(float v)            { if (v >= 0.0f && v < 10.0f) g_snn_anti_reward_gain = v; }
void snn_tune_set_depression_inc(float v)        { if (v >= 0.0f && v <= 1.0f)   g_snn_dep_inc = v; }
void snn_tune_set_depression_tau_ms(float v)     { if (v >= 1.0f && v <= 10000.0f) g_snn_dep_tau_ms = v; }
void snn_tune_set_depression_cap(float v)        { if (v >= 0.0f && v <= 1.0f)   g_snn_dep_cap = v; }
/* Biophysical stability enables/params — bounds chosen to cover the
 * full biologically-plausible range. enable knobs: any nonzero = on. */
void snn_tune_set_ahp_enabled(float v)           { g_snn_ahp_enabled  = (v != 0.0f) ? 1.0f : 0.0f; }
void snn_tune_set_ahp_tau_ms(float v)            { if (v >= 1.0f && v <= 100000.0f) g_snn_ahp_tau_ms = v; }
void snn_tune_set_ahp_gain_mv(float v)           { if (v >= 0.0f && v <= 50.0f)     g_snn_ahp_gain_mv = v; }
void snn_tune_set_pump_enabled(float v)          { g_snn_pump_enabled = (v != 0.0f) ? 1.0f : 0.0f; }
void snn_tune_set_pump_tau_ms(float v)           { if (v >= 1.0f && v <= 100000.0f) g_snn_pump_tau_ms = v; }
void snn_tune_set_pump_gain_mv(float v)          { if (v >= 0.0f && v <= 50.0f)     g_snn_pump_gain_mv = v; }
void snn_tune_set_basket_enabled(float v)        { g_snn_basket_enabled = (v != 0.0f) ? 1.0f : 0.0f; }
void snn_tune_set_basket_fraction(float v)       { if (v >= 0.01f && v <= 0.5f)     g_snn_basket_fraction = v; }
void snn_tune_set_noise_ei_ratio(float v)        { if (v >= 0.0f && v <= 1.0f)      g_snn_noise_ei_ratio = v; }

/* Substrate adapter setters — booleans accept any nonzero as on; period
 * is clamped to [1, 10000] steps. Out-of-range values for the period
 * are ignored (no change). */
void snn_tune_set_substrate_enabled(float v)           { g_snn_substrate_enabled           = (v != 0.0f) ? 1.0f : 0.0f; }
void snn_tune_set_substrate_update_period(float v)     { if (v >= 1.0f && v <= 10000.0f)  g_snn_substrate_update_period = v; }
void snn_tune_set_substrate_spike_dropout_on(float v)  { g_snn_substrate_spike_dropout_on  = (v != 0.0f) ? 1.0f : 0.0f; }
void snn_tune_set_substrate_plasticity_mod_on(float v) { g_snn_substrate_plasticity_mod_on = (v != 0.0f) ? 1.0f : 0.0f; }
/* F8: AHP/pump <-> substrate coupling. Accepts any nonzero as on, 0 as
 * off. When on, hot path scales AHP+pump hyperpolarization by substrate
 * pump_activity (clamped [0.1, 1.0] inside the loop); when off, scaling
 * factor is 1.0 (identity). */
void snn_tune_set_ahp_pump_substrate_coupling(float v) { g_snn_ahp_pump_substrate_coupling = (v != 0.0f) ? 1.0f : 0.0f; }

/* CB migration setters. Bool-style flags accept any nonzero as on.
 * e_exc/e_inh validated: e_exc must be > e_inh and both finite.
 * tau_exc/tau_inh validated: must be in [0.1, 1000] ms. */
void snn_tune_set_conductance_enabled(float v) { g_snn_conductance_enabled = (v != 0.0f) ? 1.0f : 0.0f; }
void snn_tune_set_cb_weights_rescaled(float v) { g_snn_cb_weights_rescaled = (v != 0.0f) ? 1.0f : 0.0f; }
void snn_tune_set_cb_gpu_enabled(float v)      { g_snn_cb_gpu_enabled      = (v != 0.0f) ? 1.0f : 0.0f; }
/* P0 per-receptor setters — write the new per-receptor globals.
 * Each guards a sane validation range. NMDA inherits AMPA's E_rev by default
 * but can diverge if a future user wants to tune them separately. */
void snn_tune_set_e_ampa_mv(float v)   { if (isfinite(v) && v > g_snn_e_gaba_a_mv) g_snn_e_ampa_mv = v; }
void snn_tune_set_e_nmda_mv(float v)   { if (isfinite(v) && v > g_snn_e_gaba_a_mv) g_snn_e_nmda_mv = v; }
void snn_tune_set_e_gaba_a_mv(float v) { if (isfinite(v) && v < g_snn_e_ampa_mv)   g_snn_e_gaba_a_mv = v; }
void snn_tune_set_e_gaba_b_mv(float v) { if (isfinite(v) && v < g_snn_e_ampa_mv)   g_snn_e_gaba_b_mv = v; }
void snn_tune_set_tau_ampa_ms(float v)   { if (v >= 0.1f && v <= 1000.0f) g_snn_tau_ampa_ms = v; }
void snn_tune_set_tau_nmda_ms(float v)   { if (v >= 0.1f && v <= 1000.0f) g_snn_tau_nmda_ms = v; }
void snn_tune_set_tau_gaba_a_ms(float v) { if (v >= 0.1f && v <= 1000.0f) g_snn_tau_gaba_a_ms = v; }
void snn_tune_set_tau_gaba_b_ms(float v) { if (v >= 0.1f && v <= 1000.0f) g_snn_tau_gaba_b_ms = v; }
void snn_tune_set_nmda_mg_mm(float v)    { if (v >= 0.0f && v <= 10.0f)   g_snn_nmda_mg_mm = v; }

/* Wave H — dendritic compartment flag. Bool-style: any nonzero is on. */
void snn_tune_set_dendritic_enabled(float v) { g_snn_dendritic_enabled = (v != 0.0f) ? 1.0f : 0.0f; }

/* Legacy aliases — keep Python bindings working by routing the old
 * "exc/inh" names to AMPA / GABA_A (the dominant receptors). Setting
 * tau_exc_ms updates only AMPA; users who want NMDA must use the new
 * setter. Same for tau_inh_ms → GABA_A. This means existing tune RPCs
 * keep working but lose access to the slow NMDA / GABA_B knobs unless
 * upgraded. */
void snn_tune_set_e_exc_mv(float v)   { snn_tune_set_e_ampa_mv(v); snn_tune_set_e_nmda_mv(v); }
void snn_tune_set_e_inh_mv(float v)   { snn_tune_set_e_gaba_a_mv(v); snn_tune_set_e_gaba_b_mv(v); }
void snn_tune_set_tau_exc_ms(float v) { snn_tune_set_tau_ampa_ms(v); }
void snn_tune_set_tau_inh_ms(float v) { snn_tune_set_tau_gaba_a_ms(v); }

/* Public getters — expose current values for diagnostic queries. */
float snn_tune_get_rstdp_lr(void)              { return g_rstdp_lr; }
float snn_tune_get_rstdp_baseline_alpha(void)  { return g_rstdp_baseline_alpha; }
float snn_tune_get_target_rate(void)           { return g_homeo_target_rate; }
float snn_tune_get_homeo_min_scale(void)       { return g_homeo_min_scale; }
float snn_tune_get_homeo_max_scale(void)       { return g_homeo_max_scale; }
float snn_tune_get_max_scale_dead(void)        { return g_homeo_max_scale_dead; }
float snn_tune_get_dead_threshold(void)        { return g_homeo_dead_threshold; }
float snn_tune_get_metabolic_cap(void)         { return g_metabolic_cap_factor; }
float snn_tune_get_noise_rate_hz(void)         { return g_snn_noise_rate_hz; }
float snn_tune_get_noise_pulse_mv(void)        { return g_snn_noise_pulse_mv; }
float snn_tune_get_intrinsic_alpha(void)       { return g_snn_intrinsic_alpha; }
float snn_tune_get_target_rate_input(void)     { return g_target_rate_input; }
float snn_tune_get_anti_reward_enabled(void)         { return g_snn_anti_reward_enabled; }
float snn_tune_get_anti_reward_threshold_ratio(void) { return g_snn_anti_reward_threshold_ratio; }
float snn_tune_get_anti_reward_gain(void)            { return g_snn_anti_reward_gain; }
float snn_tune_get_depression_inc(void)        { return g_snn_dep_inc; }
float snn_tune_get_depression_tau_ms(void)     { return g_snn_dep_tau_ms; }
float snn_tune_get_depression_cap(void)        { return g_snn_dep_cap; }
float snn_tune_get_ahp_enabled(void)           { return g_snn_ahp_enabled; }
float snn_tune_get_ahp_tau_ms(void)            { return g_snn_ahp_tau_ms; }
float snn_tune_get_ahp_gain_mv(void)           { return g_snn_ahp_gain_mv; }
float snn_tune_get_pump_enabled(void)          { return g_snn_pump_enabled; }
float snn_tune_get_pump_tau_ms(void)           { return g_snn_pump_tau_ms; }
float snn_tune_get_pump_gain_mv(void)          { return g_snn_pump_gain_mv; }
float snn_tune_get_basket_enabled(void)        { return g_snn_basket_enabled; }
float snn_tune_get_basket_fraction(void)       { return g_snn_basket_fraction; }
float snn_tune_get_noise_ei_ratio(void)        { return g_snn_noise_ei_ratio; }

/* Substrate adapter getters — mirror the setters. */
float snn_tune_get_substrate_enabled(void)           { return g_snn_substrate_enabled; }
float snn_tune_get_substrate_update_period(void)     { return g_snn_substrate_update_period; }
float snn_tune_get_substrate_spike_dropout_on(void)  { return g_snn_substrate_spike_dropout_on; }
float snn_tune_get_substrate_plasticity_mod_on(void) { return g_snn_substrate_plasticity_mod_on; }
float snn_tune_get_ahp_pump_substrate_coupling(void)  { return g_snn_ahp_pump_substrate_coupling; }

/* CB migration getters. */
float snn_tune_get_conductance_enabled(void)  { return g_snn_conductance_enabled; }
float snn_tune_get_cb_weights_rescaled(void)  { return g_snn_cb_weights_rescaled; }
float snn_tune_get_cb_gpu_enabled(void)       { return g_snn_cb_gpu_enabled; }
/* P0 per-receptor getters. */
float snn_tune_get_e_ampa_mv(void)            { return g_snn_e_ampa_mv; }
float snn_tune_get_e_nmda_mv(void)            { return g_snn_e_nmda_mv; }
float snn_tune_get_e_gaba_a_mv(void)          { return g_snn_e_gaba_a_mv; }
float snn_tune_get_e_gaba_b_mv(void)          { return g_snn_e_gaba_b_mv; }
float snn_tune_get_tau_ampa_ms(void)          { return g_snn_tau_ampa_ms; }
float snn_tune_get_tau_nmda_ms(void)          { return g_snn_tau_nmda_ms; }
float snn_tune_get_tau_gaba_a_ms(void)        { return g_snn_tau_gaba_a_ms; }
float snn_tune_get_tau_gaba_b_ms(void)        { return g_snn_tau_gaba_b_ms; }
float snn_tune_get_nmda_mg_mm(void)           { return g_snn_nmda_mg_mm; }
/* Wave H getter. */
float snn_tune_get_dendritic_enabled(void)    { return g_snn_dendritic_enabled; }
/* Legacy aliases — return the AMPA/GABA_A value for backward compat. */
float snn_tune_get_e_exc_mv(void)             { return g_snn_e_ampa_mv; }
float snn_tune_get_e_inh_mv(void)             { return g_snn_e_gaba_a_mv; }
float snn_tune_get_tau_exc_ms(void)           { return g_snn_tau_ampa_ms; }
float snn_tune_get_tau_inh_ms(void)           { return g_snn_tau_gaba_a_ms; }

/*============================================================================
 * Pop-class target rate selector. Pops with names starting with "input"
 * get the looser input target; everything else gets the default biological
 * 3% target.
 *==========================================================================*/
static inline float _target_rate_for_pop(const snn_population_t* pop) {
    /* Use strncmp for a proper bounded name-prefix match. pop->name is
     * char[64] so read is always defined, but strncmp is clearer and
     * robust against future struct changes. */
    if (pop && strncmp(pop->name, "input", 5) == 0) {
        return g_target_rate_input;
    }
    return g_homeo_target_rate;
}

/*============================================================================
 * Adaptive noise factor per population. Returns a multiplier in [0, 1]
 * for the base Poisson noise probability: 1.0 when pop is dead (full
 * noise to rescue the absorbing state), 0.0 when pop is at or above
 * target (noise only helps the dead). Linear falloff between.
 *
 * Without this, fixed noise both rescues dead pops AND saturates healthy
 * ones, which the oscillation we hit on pod 2026-04-21 (HEALTHY →
 * SATURATION → FULL_COLLAPSE loop) proved catastrophic.
 *==========================================================================*/
float snn_noise_factor_for_pop(const snn_population_t* pop) {
    if (!pop) return 0.0f;
    /* Warmup: firing_rate_ema is not reliable until we have samples.
     * Default to full noise during warmup — pops need it most then. */
    if (pop->rate_samples < 10) return 1.0f;
    const float target = _target_rate_for_pop(pop);
    if (target <= 0.0f) return 0.0f;
    /* Linear ramp: rate=0 → factor=1.0, rate=target → factor=0.0.
     * Above target we clamp to 0 — noise is only a rescue mechanism,
     * not a drive. Synaptic input is what carries signal. */
    const float factor = 1.0f - (pop->firing_rate_ema / target);
    if (factor < 0.0f) return 0.0f;
    if (factor > 1.0f) return 1.0f;
    return factor;
}

/*============================================================================
 * C: Intrinsic SNN reward. Firing-rate-target gaussian per population,
 * averaged. Returns reward in [0, 1]; 1.0 = every active pop at target.
 * Pops in warmup (rate_samples < 10) are excluded from the computation.
 * Called from brain_learn_vector to replace the ANN-loss-derived reward.
 *==========================================================================*/
float snn_compute_intrinsic_reward(snn_network_t* network) {
    if (!network) return 0.0f;
    float sum = 0.0f;
    int n = 0;
    const int anti_on = (g_snn_anti_reward_enabled != 0.0f);
    const float thr_ratio = g_snn_anti_reward_threshold_ratio;
    const float gain      = g_snn_anti_reward_gain;
    for (uint32_t p = 0; p < network->n_populations; p++) {
        snn_population_t* pop = network->populations[p];
        if (!pop) continue;
        if (pop->exclude_from_plasticity) continue;  /* Phase-2: language pops don't
                                                       * poison the global reward */
        if (pop->rate_samples < 10) continue;  /* warmup — rate EMA not trustworthy */
        /* Each pop gauged against its own per-class target (see #4
         * structural fix: input pops at 5%, others at 3%). */
        const float target = _target_rate_for_pop(pop);
        const float sigma  = target * 0.5f;
        const float two_sigma_sq = 2.0f * sigma * sigma;
        const float rate = pop->firing_rate_ema;
        const float err  = rate - target;
        float r = expf(-(err * err) / two_sigma_sq);
        /* Anti-reward: subtract a penalty when this pop is far above
         * target. Penalty is linear in (rate - thr×target)/target so it
         * stays bounded; gain controls magnitude. r can go negative. */
        if (anti_on && rate > thr_ratio * target) {
            r -= gain * (rate - thr_ratio * target) / target;
        }
        sum += r;
        n++;
    }
    return n > 0 ? sum / (float)n : 0.0f;
}

/*============================================================================
 * Population-level accessors for live diagnostics. Thin wrappers so the
 * Python binding doesn't need to chase struct layouts across builds.
 *==========================================================================*/
const char* snn_population_get_name(const void* p) {
    if (!p) return NULL;
    return ((const snn_population_t*)p)->name;
}
uint32_t snn_population_get_n_neurons(const void* p) {
    if (!p) return 0;
    return ((const snn_population_t*)p)->n_neurons;
}
float snn_population_get_firing_rate_ema(const void* p) {
    if (!p) return 0.0f;
    return ((const snn_population_t*)p)->firing_rate_ema;
}
uint32_t snn_population_get_rate_samples(const void* p) {
    if (!p) return 0;
    return (uint32_t)((const snn_population_t*)p)->rate_samples;
}
#include "core/neuralnet/nimcp_neuralnet.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

/*=============================================================================
 * Health Agent Forward Declarations (Phase 8: Heartbeat for Long Operations)
 *============================================================================*/
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_training)

#include <stddef.h>  /* for NULL */
//=============================================================================
// Default Configurations
//=============================================================================

void snn_stdp_config_default(snn_stdp_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_stdp_config_default: null config pointer");
        return;
    }

    /* Bi & Poo 1998 parameters */
    config->a_plus = NIMCP_DEFAULT_LEARNING_RATE;         /* LTP amplitude */
    config->a_minus = 0.0105f;      /* LTD slightly stronger (asymmetric) */
    config->tau_plus = NIMCP_STDP_TAU_PLUS_MS;       /* 20 ms LTP window */
    config->tau_minus = NIMCP_STDP_TAU_MINUS_MS;      /* 20 ms LTD window */
    config->w_min = NIMCP_SYNAPSE_STRENGTH_MIN;
    config->w_max = NIMCP_SYNAPSE_STRENGTH_MAX;
    config->soft_bounds = true;     /* Multiplicative bounds */
    config->symmetric = false;
}

void snn_rstdp_config_default(snn_rstdp_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_rstdp_config_default: null config pointer");
        return;
    }

    snn_stdp_config_default(&config->stdp);
    config->eligibility_tau = 100.0f;   /* 100 ms eligibility window */
    config->reward_tau = 50.0f;          /* 50 ms reward trace */
    config->baseline_reward = 0.0f;
    config->use_td_error = false;
}

void snn_surrogate_config_default(snn_surrogate_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_surrogate_config_default: null config pointer");
        return;
    }

    config->type = SNN_SURROGATE_FAST_SIGMOID;
    config->beta = 10.0f;           /* Steepness */
    config->threshold = 1.0f;       /* Normalized threshold */
    config->learning_rate = NIMCP_LEARNING_RATE_FINE;
    config->momentum = NIMCP_MOMENTUM_DEFAULT;
    config->weight_decay = 1e-5f;  /* Module-specific: lighter than default */
}

void snn_eprop_config_default(snn_eprop_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_eprop_config_default: null config pointer");
        return;
    }

    config->learning_rate = NIMCP_LEARNING_RATE_FINE;
    config->eligibility_tau = 100.0f;
    config->kappa = NIMCP_EXPLORATION_RATE_DEFAULT;           /* Dampening factor */
    config->use_adam = true;
    config->adam_beta1 = NIMCP_ADAM_BETA1_DEFAULT;
    config->adam_beta2 = NIMCP_ADAM_BETA2_DEFAULT;
}

void snn_homeostatic_config_default(snn_homeostatic_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_homeostatic_config_default: null config pointer");
        return;
    }

    config->target_rate = 5.0f;         /* 5 Hz target (cortical) */
    config->rate_tau = 1000.0f;         /* 1 second rate estimation */
    config->adaptation_rate = NIMCP_DEFAULT_LEARNING_RATE;    /* Slow adaptation */
    config->adjust_threshold = true;
    config->adjust_weights = false;
}

//=============================================================================
// Training Context Creation
//=============================================================================

snn_training_ctx_t* snn_training_create_stdp(const snn_stdp_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "NULL config in snn_training_create_stdp");
        return NULL;
    }

    snn_training_ctx_t* ctx = nimcp_malloc(sizeof(snn_training_ctx_t));
    if (!ctx) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(snn_training_ctx_t),
                          "Failed to allocate STDP training context");
        return NULL;
    }

    memset(ctx, 0, sizeof(snn_training_ctx_t));
    ctx->mode = SNN_TRAIN_STDP;
    ctx->eligibility_decay = 1.0f / config->tau_plus;

    NIMCP_LOGGING_DEBUG("Created STDP training context");
    return ctx;
}

snn_training_ctx_t* snn_training_create_rstdp(const snn_rstdp_config_t* config,
                                               uint32_t n_pre,
                                               uint32_t n_post) {
    if (!config || n_pre == 0 || n_post == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID, "snn_training_create_rstdp: invalid args (null config or zero dimensions)");
        return NULL;
    }

    snn_training_ctx_t* ctx = nimcp_malloc(sizeof(snn_training_ctx_t));
    if (!ctx) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(snn_training_ctx_t), "snn_training_create_rstdp: failed to allocate context");
        return NULL;
    }

    memset(ctx, 0, sizeof(snn_training_ctx_t));
    ctx->mode = SNN_TRAIN_R_STDP;
    ctx->eligibility_decay = 1.0f / config->eligibility_tau;
    ctx->reward_baseline = config->baseline_reward;

    /* Create eligibility tensor */
    uint32_t dims[] = {n_pre, n_post};
    ctx->eligibility = nimcp_tensor_zeros(dims, 2, NIMCP_DTYPE_F32);

    if (!ctx->eligibility) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, n_pre * n_post * sizeof(float), "snn_training_create_rstdp: failed to allocate eligibility tensor");
        nimcp_free(ctx);
        return NULL;
    }

    NIMCP_LOGGING_DEBUG("Created R-STDP training context: %u x %u", n_pre, n_post);
    return ctx;
}

snn_training_ctx_t* snn_training_create_surrogate(const snn_surrogate_config_t* config,
                                                   uint32_t n_pre,
                                                   uint32_t n_post) {
    if (!config || n_pre == 0 || n_post == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID, "snn_training_create_surrogate: invalid args (null config or zero dimensions)");
        return NULL;
    }

    snn_training_ctx_t* ctx = nimcp_malloc(sizeof(snn_training_ctx_t));
    if (!ctx) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(snn_training_ctx_t), "snn_training_create_surrogate: failed to allocate context");
        return NULL;
    }

    memset(ctx, 0, sizeof(snn_training_ctx_t));
    ctx->mode = SNN_TRAIN_SURROGATE;  /* Surrogate gradient backprop */
    ctx->surrogate = config->type;
    ctx->surrogate_beta = config->beta;

    /* Create gradient tensors */
    uint32_t dims[] = {n_pre, n_post};
    ctx->grad_weights = nimcp_tensor_zeros(dims, 2, NIMCP_DTYPE_F32);

    if (!ctx->grad_weights) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, n_pre * n_post * sizeof(float), "snn_training_create_surrogate: failed to allocate gradient tensor");
        nimcp_free(ctx);
        return NULL;
    }

    NIMCP_LOGGING_DEBUG("Created surrogate gradient training context");
    return ctx;
}

snn_training_ctx_t* snn_training_create_eprop(const snn_eprop_config_t* config,
                                               uint32_t n_pre,
                                               uint32_t n_post) {
    if (!config || n_pre == 0 || n_post == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID, "snn_training_create_eprop: invalid args (null config or zero dimensions)");
        return NULL;
    }

    snn_training_ctx_t* ctx = nimcp_malloc(sizeof(snn_training_ctx_t));
    if (!ctx) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(snn_training_ctx_t), "snn_training_create_eprop: failed to allocate context");
        return NULL;
    }

    memset(ctx, 0, sizeof(snn_training_ctx_t));
    ctx->mode = SNN_TRAIN_EPROP;
    ctx->eligibility_decay = 1.0f / config->eligibility_tau;

    /* Create eligibility and gradient tensors */
    uint32_t dims[] = {n_pre, n_post};
    ctx->eligibility = nimcp_tensor_zeros(dims, 2, NIMCP_DTYPE_F32);
    ctx->grad_weights = nimcp_tensor_zeros(dims, 2, NIMCP_DTYPE_F32);

    if (!ctx->eligibility || !ctx->grad_weights) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, n_pre * n_post * sizeof(float), "snn_training_create_eprop: failed to allocate tensors");
        snn_training_destroy(ctx);
        return NULL;
    }

    NIMCP_LOGGING_DEBUG("Created eProp training context: %u x %u", n_pre, n_post);
    return ctx;
}

void snn_training_destroy(snn_training_ctx_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_destroy: null context pointer");
        return;
    }

    if (ctx->eligibility) nimcp_tensor_destroy(ctx->eligibility);
    if (ctx->grad_membrane) nimcp_tensor_destroy(ctx->grad_membrane);
    if (ctx->grad_weights) nimcp_tensor_destroy(ctx->grad_weights);

    nimcp_free(ctx);
}

//=============================================================================
// STDP Functions
//=============================================================================

float snn_stdp_compute_delta_w(const snn_training_ctx_t* ctx,
                                float dt_pre_post,
                                float current_weight) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_stdp_compute_delta_w: null context pointer");
        return 0.0f;
    }

    /* Clamp non-finite weight to bounds */
    if (!isfinite(current_weight)) {
        return 0.0f;
    }

    /* Use default STDP parameters from common.h */
    const float a_plus = NIMCP_DEFAULT_LEARNING_RATE;
    const float a_minus = 0.0105f;
    const float tau_plus = 20.0f;
    const float tau_minus = 20.0f;
    const float w_min = NIMCP_SYNAPSE_STRENGTH_MIN;
    const float w_max = NIMCP_SYNAPSE_STRENGTH_MAX;

    float delta_w = 0.0f;

    /* P0 fix: Bound exponential arguments to prevent Inf/NaN
     * WHY:  expf(x) overflows for x > ~88, underflows for x < ~-88
     * HOW:  Clamp exponent argument to [-20, 0] range (covers biological timescales)
     */
    if (dt_pre_post > 0.0f) {
        /* Post after pre: LTP */
        float exp_arg = -dt_pre_post / tau_plus;
        /* Clamp to prevent underflow (exp(-20) ≈ 2e-9, negligible contribution) */
        if (exp_arg < -20.0f) exp_arg = -20.0f;
        float exp_result = expf(exp_arg);
        /* Validate exponential result */
        if (isnan(exp_result) || isinf(exp_result)) {
            exp_result = 0.0f;
        }
        delta_w = a_plus * exp_result;
    } else {
        /* Pre after post: LTD */
        float exp_arg = dt_pre_post / tau_minus;
        /* Clamp to prevent underflow (exp(-20) ≈ 2e-9, negligible contribution) */
        if (exp_arg < -20.0f) exp_arg = -20.0f;
        float exp_result = expf(exp_arg);
        /* Validate exponential result */
        if (isnan(exp_result) || isinf(exp_result)) {
            exp_result = 0.0f;
        }
        delta_w = -a_minus * exp_result;
    }

    /* P0 fix: Validate delta_w before applying soft bounds */
    if (isnan(delta_w) || isinf(delta_w)) {
        return 0.0f;
    }

    /* Apply soft bounds */
    if (delta_w > 0.0f) {
        delta_w *= (w_max - current_weight);
    } else {
        delta_w *= (current_weight - w_min);
    }

    return delta_w;
}

float snn_stdp_update(snn_training_ctx_t* ctx,
                      synapse_t* synapse,
                      float t_pre,
                      float t_post) {
    if (!ctx || !synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_stdp_update: null context or synapse pointer");
        return 0.0f;
    }

    float dt = t_post - t_pre;
    float delta_w = snn_stdp_compute_delta_w(ctx, dt, synapse->weight);

    /* Apply weight change with bounds */
    float new_weight = synapse->weight + delta_w;
    if (new_weight < 0.0f) new_weight = 0.0f;
    if (new_weight > 1.0f) new_weight = 1.0f;

    synapse->weight = new_weight;

    return new_weight;
}

uint32_t snn_stdp_apply_network(snn_training_ctx_t* ctx,
                                 snn_network_t* network,
                                 float t_current) {
    if (!ctx || !network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_stdp_apply_network: null context or network pointer");
        return 0;
    }

    (void)t_current;

    int result = snn_network_apply_stdp(network);
    return (result == SNN_SUCCESS) ? 1 : 0;
}

//=============================================================================
// R-STDP Functions
//=============================================================================

void snn_rstdp_update_eligibility(snn_training_ctx_t* ctx, float dt) {
    if (!ctx || !ctx->eligibility) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_rstdp_update_eligibility: null context or eligibility pointer");
        return;
    }

    float decay = expf(-dt * ctx->eligibility_decay);
    nimcp_tensor_mul_scalar_(ctx->eligibility, (double)decay);
}

void snn_rstdp_set_reward(snn_training_ctx_t* ctx, float reward) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_rstdp_set_reward: null context pointer");
        return;
    }
    ctx->reward = reward;
}

uint32_t snn_rstdp_apply(snn_training_ctx_t* ctx, snn_network_t* network) {
    if (!ctx || !network || !ctx->eligibility) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_rstdp_apply: null context, network, or eligibility pointer");
        return 0;
    }

    /* R-STDP: weight update = lr * (reward - baseline) * eligibility_trace
     * Reward modulates accumulated STDP eligibility traces.
     * Positive reward strengthens recently co-active synapses. */
    float reward_modulation = ctx->reward - ctx->reward_baseline;

    /* Diagnostic: log every call so we can see if reward signal is reaching us */
    static uint64_t _rstdp_call_count = 0;
    _rstdp_call_count++;
    if ((_rstdp_call_count % 50) == 0) {
        NIMCP_LOGGING_INFO(
            "[R-STDP] call=%llu reward=%.4f baseline=%.4f mod=%.4f%s",
            (unsigned long long)_rstdp_call_count,
            ctx->reward, ctx->reward_baseline, reward_modulation,
            (fabsf(reward_modulation) < 1e-8f) ? " — SKIP (no signal)" : "");
    }

    if (fabsf(reward_modulation) < 1e-8f) return 0;

    /* R-STDP learning rate — now runtime-tunable via snn_tune_set_rstdp_lr().
     * Default 0.0001 after iteration history: 0.001 → 0.0005 → 0.0001. */
    float lr = g_rstdp_lr;
    /* Substrate-modulated plasticity. When the network has a substrate
     * attached AND both the master enabled + plasticity_mod_on knobs are
     * set, scale the effective LR by the cached dendrite plasticity
     * multiplier. At baseline (atp=1) the multiplier is 1.0 and this is
     * a no-op. Low ATP (depleted substrate) shrinks the multiplier and
     * slows learning — biological fidelity: tired circuits learn less. */
    if (network && network->substrate &&
        snn_tune_get_substrate_enabled() != 0.0f &&
        snn_tune_get_substrate_plasticity_mod_on() != 0.0f) {
        lr = substrate_apply_lr(lr, &network->cached_dend_effects);
    }
    float scale = lr * reward_modulation;

    /* Apply eligibility-modulated update to network synapses */
    uint32_t updates = 0;
    if (network->neural_net) {
        uint32_t n_neurons = neural_network_get_num_neurons(network->neural_net);
        float* elig_data = (float*)ctx->eligibility->data;
        uint32_t elig_rows = ctx->eligibility->shape.dims[0];
        uint32_t elig_cols = ctx->eligibility->shape.dims[ctx->eligibility->shape.rank - 1];

        for (uint32_t i = 0; i < n_neurons; i++) {
            neuron_t* n = neural_network_get_neuron(network->neural_net, i);
            if (!n) continue;
            uint32_t syn_count = sparse_synapse_count(&n->outgoing);
            for (uint32_t s = 0; s < syn_count; s++) {
                synapse_handle_t* h = sparse_synapse_get(&n->outgoing, s);
                if (!h) continue;
                /* Look up eligibility for this synapse (i, target).
                 * Only synapses within the trace dimensions get reward-modulated
                 * updates; the rest rely on local STDP in the SNN step. */
                uint32_t j = h->target_neuron_id;
                if (i < elig_rows && j < elig_cols && elig_data) {
                    float e = elig_data[i * elig_cols + j];
                    if (fabsf(e) > 1e-10f) {
                        h->weight += scale * e;
                        /* Clamp weight to [-2, 2] */
                        if (h->weight > 2.0f) h->weight = 2.0f;
                        if (h->weight < -2.0f) h->weight = -2.0f;
                        updates++;
                    }
                }
            }
        }
    }

    /* === LIGHTWEIGHT CSR R-STDP ===
     * The hierarchical SNN uses lightweight CSR populations (n_hidden=0 in
     * neural_net), so the loop above updates zero synapses. For CSR pops,
     * apply a direct Hebbian rule modulated by reward using current spike
     * outputs. For each incoming CSR entry on each post-synaptic neuron:
     *   pre_spike  = source pop's spike_output[src_neuron]
     *   post_spike = this pop's spike_output[j]
     *   Δw = scale × (pre × post - decay × weight)
     * Only updates synapses where both pre and post fired recently —
     * effectively Hebbian + reward + light weight decay. */
    for (uint32_t p = 0; p < network->n_populations; p++) {
        snn_population_t* dst_pop = network->populations[p];
        if (!dst_pop || !dst_pop->lightweight || !dst_pop->incoming_csr) continue;
        if (dst_pop->exclude_from_plasticity) continue;  /* Phase-2: keep the
                                          * language projection out of global R-STDP */
        if (!dst_pop->spike_output) continue;
        /* Warm-up gate: skip R-STDP on populations that haven't collected
         * enough firing-rate samples for homeostasis to engage. During the
         * first 100 SNN steps, R-STDP with no homeostatic brake can amplify
         * transient hyperactivity into runaway — classic Hebbian compounding.
         * Matches the same rate_samples < 100 gate in snn_homeostatic_apply. */
        if (dst_pop->rate_samples < 100) continue;
        snn_csr_storage_t* csr = dst_pop->incoming_csr;
        if (!csr->entries || csr->n_synapses == 0) continue;

        const float* post_spikes = (const float*)nimcp_tensor_data_const(dst_pop->spike_output);
        if (!post_spikes) continue;

        /* For each neuron j in this population, iterate its incoming synapses */
        for (uint32_t j = 0; j < csr->n_neurons; j++) {
            float post = post_spikes[j];
            if (post < 0.5f) continue;  /* Post didn't spike — no Hebbian update */

            uint32_t row_start = csr->row_ptr[j];
            uint32_t row_end = csr->row_ptr[j + 1];
            for (uint32_t e = row_start; e < row_end; e++) {
                snn_csr_synapse_t* entry = &csr->entries[e];
                /* Look up pre-synaptic spike from source population */
                if (entry->src_pop >= network->n_populations) continue;
                snn_population_t* src_pop = network->populations[entry->src_pop];
                if (!src_pop || !src_pop->spike_output) continue;
                if (entry->src_neuron >= src_pop->n_neurons) continue;

                const float* src_spikes = (const float*)nimcp_tensor_data_const(src_pop->spike_output);
                if (!src_spikes) continue;
                float pre = src_spikes[entry->src_neuron];
                if (pre < 0.5f) continue;  /* Pre didn't spike */

                /* Reward-modulated Hebbian + standard L2 weight decay.
                 *
                 * EXCITATORY (weight > 0): standard rule
                 *   Δw = scale - decay × w
                 *   + reward → weight grows, strengthening excitation
                 *
                 * INHIBITORY (weight < 0): SIGN-FLIPPED rule (inhibitory plasticity)
                 *   Δw = -scale - decay × w
                 *   + reward → weight becomes more negative, strengthening inhibition
                 *   This implements separate E/I plasticity: inhibitory synapses
                 *   that "helped" (by suppressing wrong output when reward came)
                 *   also strengthen, maintaining E/I balance.
                 *
                 * RATE-DEPENDENT INHIBITORY STRENGTHENING:
                 * If postsynaptic neuron is over-firing (rate_ema > 0.10),
                 * inhibitory synapses onto it strengthen proportionally —
                 * exactly what's needed to clamp runaway excitation at the
                 * local circuit level, not just via slow synaptic scaling. */
                float decay_rate = 1e-5f;
                float delta;
                if (entry->weight >= 0.0f) {
                    delta = scale - decay_rate * entry->weight;
                } else {
                    delta = -scale - decay_rate * entry->weight;
                    /* Extra inhibitory strengthening proportional to post rate */
                    if (dst_pop->neuron_rate_ema) {
                        float rate = dst_pop->neuron_rate_ema[j];
                        if (rate > 0.10f) {
                            /* Push weight more negative: additional 0.1 × (rate - 0.10)
                             * per apply. At rate=0.5, adds -0.04 per call. */
                            delta -= 0.1f * (rate - 0.10f);
                        }
                    }
                }
                entry->weight += delta;
                if (entry->weight > 2.0f) entry->weight = 2.0f;
                if (entry->weight < -4.0f) entry->weight = -4.0f;  /* inhib cap −4 */
                updates++;
            }
        }
    }

    /* Decay reward baseline toward recent rewards (EMA) — tunable. */
    float a = g_rstdp_baseline_alpha;
    ctx->reward_baseline = (1.0f - a) * ctx->reward_baseline + a * ctx->reward;

    if ((_rstdp_call_count % 50) == 0 && updates > 0) {
        NIMCP_LOGGING_INFO("[R-STDP] updated %u synapses (delta sign=%s)",
                           updates, (scale > 0) ? "+" : "-");
    }

    return updates;
}

//=============================================================================
// Surrogate Gradient Functions
//=============================================================================

static float snn_surrogate_gradient_local(const snn_training_ctx_t* ctx, float membrane_v) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_surrogate_gradient: null context pointer");
        return 0.0f;
    }

    float beta = ctx->surrogate_beta > 0.0f ? ctx->surrogate_beta : 10.0f;
    float x = beta * (membrane_v - 1.0f);  /* Threshold normalized to 1 */
    float grad = 0.0f;

    switch (ctx->surrogate) {
        case SNN_SURROGATE_SIGMOID: {
            float sig = 1.0f / (1.0f + expf(-x));
            grad = sig * (1.0f - sig) * beta;
            break;
        }

        case SNN_SURROGATE_FAST_SIGMOID: {
            float denom = 1.0f + fabsf(x);
            grad = beta / (2.0f * denom * denom);
            break;
        }

        case SNN_SURROGATE_ARCTAN: {
            grad = beta / (1.0f + x * x);
            break;
        }

        case SNN_SURROGATE_SUPERSPIKE: {
            float denom = 1.0f + fabsf(x);
            grad = 1.0f / (denom * denom);
            break;
        }

        case SNN_SURROGATE_TRIANGULAR:
            if (fabsf(x) < 1.0f) {
                grad = beta * (1.0f - fabsf(x));
            }
            break;

        case SNN_SURROGATE_RECTANGULAR:
            if (fabsf(x) < 0.5f) {
                grad = beta;
            }
            break;

        default:
            grad = 0.0f;
    }

    return grad;
}

/**
 * @brief Public wrapper for snn_surrogate_gradient_local
 *
 * WHAT: Public API for computing surrogate gradient with snn_training_ctx_t
 * WHY:  Exposes functionality renamed from snn_surrogate_gradient() to avoid
 *       ODR conflict with snn_backprop's snn_surrogate_gradient()
 * HOW:  Delegates to static snn_surrogate_gradient_local()
 */
float snn_training_surrogate_gradient(const snn_training_ctx_t* ctx, float membrane_v) {
    return snn_surrogate_gradient_local(ctx, membrane_v);
}

/** Gradient clipping bounds for numerical stability */
#define SNN_GRADIENT_CLIP_MIN -5.0f
#define SNN_GRADIENT_CLIP_MAX 5.0f

/**
 * @brief Clip gradient to prevent numerical instability
 *
 * WHAT: Bound gradient magnitude to prevent explosion
 * WHY:  Surrogate gradients can grow unbounded, causing NaN/Inf
 * HOW:  Clamp to [-5, 5] range (configurable via defines)
 *
 * @param grad Input gradient value
 * @return Clipped gradient value
 */
static inline float snn_clip_gradient(float grad) {
    if (grad < SNN_GRADIENT_CLIP_MIN) return SNN_GRADIENT_CLIP_MIN;
    if (grad > SNN_GRADIENT_CLIP_MAX) return SNN_GRADIENT_CLIP_MAX;
    return grad;
}

int snn_surrogate_backward(snn_training_ctx_t* ctx,
                           const float* output_grad,
                           const float* membrane_v,
                           uint32_t n_neurons,
                           float* input_grad) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surrogate_backward: training context is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!output_grad) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surrogate_backward: gradient buffer is NULL (no forward pass?)");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!membrane_v || !input_grad) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surrogate_backward: membrane_v or input_grad is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    /* Phase 8: Send heartbeat at start of backward pass */
    snn_training_heartbeat("snn_backward", 0.0f);

    for (uint32_t i = 0; i < n_neurons; i++) {
        float surrogate = snn_surrogate_gradient_local(ctx, membrane_v[i]);
        float grad = output_grad[i] * surrogate;

        /* Apply gradient clipping to prevent exploding gradients
         * WHAT: Bound gradient magnitude
         * WHY:  Surrogate × output_grad can explode during training
         */
        input_grad[i] = snn_clip_gradient(grad);
    }

    return SNN_SUCCESS;
}

int snn_surrogate_apply_gradients(snn_training_ctx_t* ctx,
                                   float** weights,
                                   float** gradients) {
    if (!ctx || !weights || !gradients) {
        return SNN_ERROR_NULL_POINTER;
    }

    /* Simplified gradient application */
    return SNN_SUCCESS;
}

//=============================================================================
// eProp Functions
//=============================================================================

void snn_eprop_update_eligibility(snn_training_ctx_t* ctx,
                                   const uint8_t* pre_spikes,
                                   const uint8_t* post_spikes,
                                   float dt) {
    if (!ctx || !ctx->eligibility || !pre_spikes || !post_spikes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_eprop_update_eligibility: null context, eligibility, pre_spikes, or post_spikes pointer");
        return;
    }

    float decay = expf(-dt * ctx->eligibility_decay);
    nimcp_tensor_mul_scalar_(ctx->eligibility, (double)decay);
}

uint32_t snn_eprop_apply(snn_training_ctx_t* ctx,
                          snn_network_t* network,
                          float learning_signal) {
    if (!ctx || !network || !ctx->eligibility) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_eprop_apply: null context, network, or eligibility pointer");
        return 0;
    }

    /* eProp: weight update = lr * learning_signal * eligibility_trace
     * learning_signal is the error signal broadcast to all neurons.
     * eProp uses only local information (eligibility) + global signal. */
    float lr = 0.001f;  /* eProp learning rate (ctx has no lr field) */
    float scale = lr * learning_signal;
    if (fabsf(scale) < 1e-10f) return 0;

    uint32_t updates = 0;
    if (network->neural_net) {
        uint32_t n_neurons = neural_network_get_num_neurons(network->neural_net);
        float* elig_data = (float*)ctx->eligibility->data;
        uint32_t elig_rows = ctx->eligibility->shape.dims[0];
        uint32_t elig_cols = ctx->eligibility->shape.dims[ctx->eligibility->shape.rank - 1];

        for (uint32_t i = 0; i < n_neurons; i++) {
            neuron_t* n = neural_network_get_neuron(network->neural_net, i);
            if (!n) continue;
            uint32_t syn_count = sparse_synapse_count(&n->outgoing);
            for (uint32_t s = 0; s < syn_count; s++) {
                synapse_handle_t* h = sparse_synapse_get(&n->outgoing, s);
                if (!h) continue;
                uint32_t j = h->target_neuron_id;
                if (i < elig_rows && j < elig_cols && elig_data) {
                    float e = elig_data[i * elig_cols + j];
                    if (fabsf(e) > 1e-10f) {
                        h->weight -= scale * e;
                        if (h->weight > 2.0f) h->weight = 2.0f;
                        if (h->weight < -2.0f) h->weight = -2.0f;
                        updates++;
                    }
                }
            }
        }
    }

    return updates;
}

//=============================================================================
// Homeostatic Functions
//=============================================================================

void snn_homeostatic_update_rates(snn_training_ctx_t* ctx,
                                   const uint8_t* spikes,
                                   uint32_t n_neurons,
                                   float dt) {
    if (!ctx || !spikes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_homeostatic_update_rates: null context or spikes pointer");
        return;
    }
    (void)n_neurons;
    (void)dt;
}

/* Synaptic scaling (Turrigiano 2008, biological homeostatic plasticity).
 * For each lightweight population, compare its EMA firing rate to the
 * biological target and multiplicatively scale all incoming CSR weights
 * to pull the rate back toward target. Prevents R-STDP runaway into
 * either silence or saturation.
 *
 * Called from the training loop every N learn_vector calls (not every
 * SNN step — too expensive to iterate 1.45B synapses that often). */
uint32_t snn_homeostatic_apply(snn_training_ctx_t* ctx, snn_network_t* network) {
    if (!ctx || !network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_homeostatic_apply: null context or network pointer");
        return 0;
    }

    /* Target rate is now per-pop (see _target_rate_for_pop). Declared
     * inside the per-pop loop below. */
    /* Scale bounds: tight [0.98, 1.02] always. The earlier emergency band
     * [0.90, 1.10] caused bang-bang oscillation — repeated 1.10× pushes
     * compounded the weights (1.10^20 ≈ 6.7×) so the SNN swung from silent
     * collapse (<1K spikes) to hyperactive (>1M spikes) within hundreds
     * of steps, degrading training. Tight bounds recover slower
     * (~30 applies from extreme) but don't overshoot. */
    const float rate_floor    = 1e-4f;   /* avoid divide-by-near-zero */
    const float w_cap         = 10.0f;   /* same cap as init path */

    uint32_t n_scaled = 0;
    uint32_t n_skipped_warmup = 0;

    for (uint32_t p = 0; p < network->n_populations; p++) {
        snn_population_t* pop = network->populations[p];
        if (!pop || !pop->lightweight || !pop->incoming_csr) continue;
        if (pop->exclude_from_plasticity) continue;  /* Phase-2: don't homeostatically
                                  * rescale the warm-started language projection weights */

        /* Warm-up gate: don't scale until EMA has enough samples to be
         * trustworthy. Prevents scaling on transient startup activity. */
        if (pop->rate_samples < 100) {
            n_skipped_warmup++;
            continue;
        }

        /* Per-pop target rate — input pops get a looser target since
         * they're externally driven; hidden + output pops use the
         * standard biological 3%. Prevents the case where forcing input
         * pops from natural 30%+ firing down to 3% crashes cascade. */
        const float target_rate = _target_rate_for_pop(pop);

        float cur_rate = pop->firing_rate_ema;
        if (cur_rate < rate_floor) cur_rate = rate_floor;

        /* Scale bounds: tight [0.98, 1.02] in the normal regime to prevent
         * hot-collapse oscillation (compound 1.10^N caused bang-bang in
         * prior runs — see commit 776511957 for history). BUT when a pop
         * is DEAD (firing < 10% of target, i.e. <0.3%), the slow 1.02 cap
         * can take 150+ applies to escape — meanwhile R-STDP has nothing
         * to reinforce because nothing is firing. Result: trapped at zero.
         *
         * Asymmetric fix: when a pop is far below target, widen only the
         * UPPER bound to 1.05. This halves recovery time (1.05^85 ≈ 66
         * vs 1.02^85 ≈ 5.4) without re-introducing the overshoot risk,
         * because:
         *   - only scale-UP is widened; scale-DOWN stays at 0.98
         *   - the widened bound only engages for quiet pops, which by
         *     definition cannot overshoot into hot-collapse territory
         *     (they're scaling from nothing; going 66× nothing is still
         *     small relative to the threshold-triggering drive)
         *   - once rate > 0.3% of target, we drop back to the tight cap
         *     so the approach to target is smooth
         */
        float min_scale = g_homeo_min_scale;
        float max_scale = g_homeo_max_scale;
        if (cur_rate < g_homeo_dead_threshold * target_rate) {
            max_scale = g_homeo_max_scale_dead;  /* escape velocity for dead pops */
        }
        /* Reward-coupled aggressiveness: when R-STDP reward is near zero
         * (saturation collapses the gaussian intrinsic reward to 0), R-STDP
         * weights don't update and only homeostatic can pull firing back
         * down. The default 0.98 floor takes ~70 applies to halve weights,
         * which is far too slow vs. the timescale at which saturation
         * cascades. Widen the scale-DOWN bound to 0.90 for over-target pops
         * specifically when R-STDP is stuck. Upper bound is untouched so
         * dead pops still get their normal escape velocity. */
        if (ctx->reward < 0.1f && cur_rate > target_rate * 1.5f) {
            min_scale = 0.90f;
        }

        /* Target / current gives the pull direction. */
        float scale = target_rate / cur_rate;
        if (scale < min_scale) scale = min_scale;
        if (scale > max_scale) scale = max_scale;

        /* Skip near-unity scaling — saves cycles when population is on
         * target. 0.5% deadband avoids thrashing around the setpoint. */
        if (scale > 0.995f && scale < 1.005f) continue;

        snn_csr_storage_t* csr = pop->incoming_csr;

        /* Phase: saturation-recovery perf fix. Operate on the flat
         * weights[] array when it exists (GPU path). The flat array is
         * contiguous float32 — SIMD-vectorizable by the compiler at -O3
         * vs. the strided entries[] struct (12-byte stride, 4-byte
         * field). On saturated checkpoints where every pop needs a full
         * weight walk, this drops per-pop cost ~3-4× by eliminating
         * redundant passes. Metabolic cap skip when scale <= 1.0
         * (scaling down can't newly violate cap) cuts another ~50%. */
        const bool use_flat = (csr->weights != NULL);

        if (use_flat) {
            /* 1. Scale in place on flat weights[] — vectorizable. */
            float* __restrict__ w = csr->weights;
            const uint32_t n = csr->n_synapses;
            for (uint32_t e = 0; e < n; e++) {
                float v = w[e] * scale;
                if (v >  w_cap) v =  w_cap;
                if (v < -w_cap) v = -w_cap;
                w[e] = v;
            }

            /* 2. Metabolic cap — skip when scale<=1.0 since shrinking
             *    can't newly exceed cap. (Pre-existing violations from
             *    R-STDP LTP since last apply are still handled on the
             *    scale>1.0 path, and also by the next apply when the
             *    pop's rate triggers scaling in the other direction.) */
            if (scale > 1.0f) {
                for (uint32_t j = 0; j < csr->n_neurons; j++) {
                    uint32_t rs = csr->row_ptr[j];
                    uint32_t re = csr->row_ptr[j + 1];
                    uint32_t fan_in = re - rs;
                    if (fan_in == 0) continue;
                    float cap = g_metabolic_cap_factor * (float)fan_in;
                    float sum_abs = 0.0f;
                    for (uint32_t e = rs; e < re; e++) sum_abs += fabsf(w[e]);
                    if (sum_abs > cap) {
                        float rescale = cap / sum_abs;
                        for (uint32_t e = rs; e < re; e++) w[e] *= rescale;
                    }
                }
            }

            /* 3. Sync flat weights[] -> entries[] in a single strided
             *    pass. CPU-path code (R-STDP, Louvain, introspection)
             *    reads .weight from entries[], so we must keep both in
             *    sync — but only one pass, not three. */
            for (uint32_t e = 0; e < n; e++) {
                csr->entries[e].weight = w[e];
            }

            /* 4. Push to GPU if resident. */
            if (csr->gpu_resident) {
                snn_csr_sync_weights_to_gpu(csr);
            }
        } else {
            /* CPU-only fallback: original strided path on entries[]. */
            for (uint32_t e = 0; e < csr->n_synapses; e++) {
                float v = csr->entries[e].weight * scale;
                if (v >  w_cap) v =  w_cap;
                if (v < -w_cap) v = -w_cap;
                csr->entries[e].weight = v;
            }

            if (scale > 1.0f) {
                for (uint32_t j = 0; j < csr->n_neurons; j++) {
                    uint32_t rs = csr->row_ptr[j];
                    uint32_t re = csr->row_ptr[j + 1];
                    uint32_t fan_in = re - rs;
                    if (fan_in == 0) continue;
                    float cap = g_metabolic_cap_factor * (float)fan_in;
                    float sum_abs = 0.0f;
                    for (uint32_t e = rs; e < re; e++) sum_abs += fabsf(csr->entries[e].weight);
                    if (sum_abs > cap) {
                        float rescale = cap / sum_abs;
                        for (uint32_t e = rs; e < re; e++) csr->entries[e].weight *= rescale;
                    }
                }
            }
        }

        NIMCP_LOGGING_INFO("homeostatic: pop %u '%s' rate=%.4f target=%.4f "
                           "scale=%.4f syns=%u",
                           pop->id, pop->name, pop->firing_rate_ema,
                           target_rate, scale, csr->n_synapses);
        n_scaled++;
    }

    if (n_skipped_warmup > 0) {
        NIMCP_LOGGING_DEBUG("homeostatic: %u pops still in warm-up",
                            n_skipped_warmup);
    }
    return n_scaled;
}

//=============================================================================
// Statistics Functions
//=============================================================================

void snn_training_get_stats(const snn_training_ctx_t* ctx,
                            uint64_t* weight_updates,
                            uint64_t* training_steps,
                            float* total_delta_w) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_get_stats: null context pointer");
        return;
    }

    if (weight_updates) *weight_updates = 0;
    if (training_steps) *training_steps = 0;
    if (total_delta_w) *total_delta_w = 0.0f;
}

void snn_training_reset_stats(snn_training_ctx_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_reset_stats: null context pointer");
        return;
    }
    /* No stats to reset in current struct */
}

void snn_training_reset(snn_training_ctx_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_reset: null context pointer");
        return;
    }

    /* Zero out tensors by multiplying by 0 */
    if (ctx->eligibility) {
        nimcp_tensor_mul_scalar_(ctx->eligibility, 0.0);
    }
    if (ctx->grad_weights) {
        nimcp_tensor_mul_scalar_(ctx->grad_weights, 0.0);
    }
    if (ctx->grad_membrane) {
        nimcp_tensor_mul_scalar_(ctx->grad_membrane, 0.0);
    }

    ctx->reward = 0.0f;
    ctx->current_loss = 0.0f;
    ctx->smoothed_loss = 0.0f;
}
