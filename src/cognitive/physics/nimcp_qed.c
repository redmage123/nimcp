/**
 * @file nimcp_qed.c
 * @brief Quantum Electrodynamics — Feynman rules, cross-sections, precision QED
 *
 * WHAT: Klein-Nishina, Bhabha, Moller, pair production, running α, anomalous moment
 * WHY:  Most precisely tested theory in physics — foundation for quantum reasoning
 * HOW:  Tree-level Feynman diagram amplitudes, one-loop vacuum polarization
 */

#include "cognitive/physics/nimcp_qed.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "QED"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * Feynman Rule Building Blocks
 * ============================================================================ */

float qed_vertex_coupling(void) {
    /* e = sqrt(4πα) in natural units */
    return sqrtf(4.0f * (float)M_PI * QED_ALPHA);
}

float qed_photon_propagator(float q2_gev2) {
    /* -ig_μν/q² → magnitude 1/|q²| */
    if (fabsf(q2_gev2) < 1e-20f) return 1e20f;  /* IR divergence */
    return 1.0f / fabsf(q2_gev2);
}

float qed_electron_propagator(float p2_gev2, float mass_gev) {
    /* i(p̸+m)/(p²-m²) → magnitude 1/|p²-m²| */
    float denom = p2_gev2 - mass_gev * mass_gev;
    if (fabsf(denom) < 1e-20f) return 1e20f;  /* on-shell pole */
    return 1.0f / fabsf(denom);
}

float qed_phase_space_2to2(float s, float m1, float m2, float m3, float m4) {
    /* dΦ₂ = |p_f|/(16π² √s) where |p_f| = √(λ(s,m3²,m4²))/(2√s) */
    /* λ(a,b,c) = a²+b²+c²-2ab-2ac-2bc (Källén function) */
    float m3sq = m3*m3, m4sq = m4*m4;
    float lambda = s*s + m3sq*m3sq + m4sq*m4sq - 2*s*m3sq - 2*s*m4sq - 2*m3sq*m4sq;
    if (lambda < 0) return 0;  /* below threshold */
    float pf = sqrtf(lambda) / (2.0f * sqrtf(s));
    return pf / (16.0f * (float)(M_PI * M_PI) * sqrtf(s));
}

/* ============================================================================
 * Cross-Sections
 * ============================================================================ */

float qed_thomson_cross_section(void) {
    /* σ_T = 8π/3 · r_e² = 0.6652 barn */
    float re = QED_CLASSICAL_E_RADIUS;
    return 8.0f * (float)M_PI / 3.0f * re * re;
}

float qed_klein_nishina(float photon_energy_gev, float angle_rad) {
    /* Klein-Nishina formula for Compton scattering:
     * dσ/dΩ = (r_e²/2)(ε'/ε)²(ε/ε' + ε'/ε - sin²θ)
     * where ε'/ε = 1/(1 + (E_γ/m_e)(1-cosθ)) */
    float x = photon_energy_gev / QED_ELECTRON_MASS;
    float cos_theta = cosf(angle_rad);
    float ratio = 1.0f / (1.0f + x * (1.0f - cos_theta));
    float re = QED_CLASSICAL_E_RADIUS;
    float sin2 = 1.0f - cos_theta * cos_theta;
    return 0.5f * re * re * ratio * ratio * (ratio + 1.0f/ratio - sin2);
}

float qed_pair_production_threshold(void) {
    return 2.0f * QED_ELECTRON_MASS;  /* 1.022 MeV */
}

float qed_pair_annihilation_cross_section(float gamma_cm) {
    /* e⁺e⁻ → γγ cross-section (Dirac, 1930):
     * σ = (πr_e²/γ) · [(γ²+4γ+1)/(γ²-1)·ln(γ+√(γ²-1)) - (γ+3)/√(γ²-1)]
     * where γ = E_cm/(2m_e) is the Lorentz factor */
    if (gamma_cm <= 1.0f) return 0;  /* below threshold */
    float re = QED_CLASSICAL_E_RADIUS;
    double g = (double)gamma_cm;
    double g2 = g * g;
    double sqrt_g2m1 = sqrt(g2 - 1.0);
    double term1 = (g2 + 4.0*g + 1.0) / (g2 - 1.0) * log(g + sqrt_g2m1);
    double term2 = (g + 3.0) / sqrt_g2m1;
    double sigma = M_PI * (double)re * re / g * (term1 - term2);
    return (float)sigma;
}

float qed_moller_cross_section(float cm_energy_gev, float angle_rad) {
    /* Møller scattering: e⁻e⁻→e⁻e⁻ (t-channel + u-channel exchange)
     * dσ/dΩ = (α²/4s) · [4/sin⁴θ - 3/sin²θ + (1+4/sin²θ)(sin⁴(θ/2)+cos⁴(θ/2))]
     * Simplified Rutherford-like at tree level */
    float s = cm_energy_gev * cm_energy_gev;
    if (s < 4.0f * QED_ELECTRON_MASS * QED_ELECTRON_MASS) return 0;
    float sin_theta = sinf(angle_rad);
    if (fabsf(sin_theta) < 1e-6f) return 1e10f;  /* forward divergence */
    float sin2 = sin_theta * sin_theta;
    float sin4 = sin2 * sin2;
    float cos_half = cosf(angle_rad * 0.5f);
    float sin_half = sinf(angle_rad * 0.5f);
    float term = 4.0f/sin4 - 3.0f/sin2 +
                 (1.0f + 4.0f/sin2) * (sin_half*sin_half*sin_half*sin_half +
                                         cos_half*cos_half*cos_half*cos_half);
    return QED_ALPHA * QED_ALPHA / (4.0f * s) * term * QED_HBAR_C2;
}

float qed_bhabha_cross_section(float cm_energy_gev, float angle_rad) {
    /* Bhabha scattering: e⁺e⁻→e⁺e⁻ (s-channel + t-channel)
     * dσ/dΩ = (α²/2s) · [(1+cos²θ)/(1-cosθ)² + ... + interference]
     * Simplified to leading order */
    float s = cm_energy_gev * cm_energy_gev;
    if (s < 4.0f * QED_ELECTRON_MASS * QED_ELECTRON_MASS) return 0;
    float cos_theta = cosf(angle_rad);
    float t_denom = 1.0f - cos_theta;
    if (fabsf(t_denom) < 1e-6f) return 1e10f;
    float cos2 = cos_theta * cos_theta;
    /* t-channel dominates at small angles */
    float dsigma = QED_ALPHA * QED_ALPHA / (2.0f * s) *
                   (1.0f + cos2) / (t_denom * t_denom) * QED_HBAR_C2;
    return dsigma;
}

float qed_breit_wheeler_cross_section(float cm_energy_gev) {
    /* γγ → e⁺e⁻ (Breit-Wheeler, 1934)
     * σ = (πr_e²/2)(1-β²)[2β(β²-2)+(3-β⁴)ln((1+β)/(1-β))]
     * where β = √(1 - 4m²/s) */
    float s = cm_energy_gev * cm_energy_gev;
    float beta2 = 1.0f - 4.0f * QED_ELECTRON_MASS * QED_ELECTRON_MASS / s;
    if (beta2 <= 0) return 0;  /* below threshold */
    float beta = sqrtf(beta2);
    float re = QED_CLASSICAL_E_RADIUS;
    double b = (double)beta;
    double b2 = b*b;
    double b4 = b2*b2;
    double term1 = 2.0*b*(b2 - 2.0);
    double term2 = (3.0 - b4) * log((1.0+b)/(1.0-b));
    return (float)(M_PI * (double)re * re * 0.5 * (1.0 - b2) * (term1 + term2));
}

/* ============================================================================
 * Running Coupling & Vacuum Polarization
 * ============================================================================ */

float qed_vacuum_polarization(float q2_gev2) {
    /* One-loop vacuum polarization (electron loop):
     * Π(q²) = -(α/3π)·[ln(q²/m_e²) - 5/3 + O(m_e²/q²)]
     * Valid for |q²| >> m_e² */
    float me2 = QED_ELECTRON_MASS * QED_ELECTRON_MASS;
    if (fabsf(q2_gev2) < me2) return 0;  /* below electron threshold */
    double ratio = fabs((double)q2_gev2 / (double)me2);
    double Pi = -(double)QED_ALPHA / (3.0 * M_PI) * (log(ratio) - 5.0/3.0);
    return (float)Pi;
}

float qed_running_alpha(float q2_gev2) {
    /* α(q²) = α / (1 - Π(q²))
     * At q² = M_Z² ≈ (91.2 GeV)², α ≈ 1/128 (vs 1/137 at q²=0) */
    float Pi = qed_vacuum_polarization(q2_gev2);
    float denom = 1.0f - Pi;
    if (fabsf(denom) < 1e-10f) denom = 1e-10f;  /* Landau pole protection */
    return QED_ALPHA / denom;
}

/* ============================================================================
 * Precision QED
 * ============================================================================ */

float qed_anomalous_moment_first_order(void) {
    /* Schwinger (1948): a_e = α/(2π) */
    return QED_ALPHA / (2.0f * (float)M_PI);
}

float qed_anomalous_moment_third_order(void) {
    /* Through O(α³):
     * a_e = α/(2π) - 0.32848·(α/π)² + 1.1812·(α/π)³
     * = 0.001159652... (agrees with experiment to 12 digits) */
    float a_pi = QED_ALPHA / (float)M_PI;
    float a_pi2 = a_pi * a_pi;
    float a_pi3 = a_pi2 * a_pi;
    return 0.5f * a_pi - 0.32848f * a_pi2 + 1.1812f * a_pi3;
}

float qed_lamb_shift_hydrogen(void) {
    /* Lamb shift 2S₁/₂ - 2P₁/₂ in hydrogen:
     * ΔE ≈ (α⁵m_e c²/π) · [ln(1/α²) - Bethe_log + ...]
     * Experimental: 1057.845 MHz
     * Simplified one-loop: ~1040 MHz */
    double alpha = (double)QED_ALPHA;
    double alpha5 = alpha*alpha*alpha*alpha*alpha;
    double me_c2_mhz = (double)QED_ELECTRON_MASS * 1e6 * 241.8;  /* GeV → MHz */
    double shift = alpha5 * me_c2_mhz / M_PI * (log(1.0/(alpha*alpha)) + 2.81);
    return (float)shift;
}

float qed_bethe_logarithm(uint32_t n, uint32_t l) {
    /* Bethe logarithm ln(k₀) for hydrogen states
     * Values: 1S: 2.984, 2S: 2.812, 2P: -0.030, 3S: 2.768 */
    if (n == 1 && l == 0) return 2.984f;
    if (n == 2 && l == 0) return 2.812f;
    if (n == 2 && l == 1) return -0.030f;
    if (n == 3 && l == 0) return 2.768f;
    /* Generic approximation for high n */
    return 2.8f - 0.18f * (float)n;
}

/* ============================================================================
 * Process Dispatch
 * ============================================================================ */

float qed_compute_cross_section(qed_sim_t* sim, qed_process_type_t type,
                                  float cm_energy_gev, float angle_rad) {
    if (!sim) return 0;

    float sigma = 0;
    switch (type) {
    case QED_PROC_COMPTON:
        sigma = qed_klein_nishina(cm_energy_gev, angle_rad);
        break;
    case QED_PROC_PAIR_ANNIHILATION: {
        float gamma = cm_energy_gev / (2.0f * QED_ELECTRON_MASS);
        sigma = qed_pair_annihilation_cross_section(gamma);
        break;
    }
    case QED_PROC_MOLLER:
        sigma = qed_moller_cross_section(cm_energy_gev, angle_rad);
        break;
    case QED_PROC_BHABHA:
        sigma = qed_bhabha_cross_section(cm_energy_gev, angle_rad);
        break;
    case QED_PROC_PHOTON_PHOTON:
        sigma = qed_breit_wheeler_cross_section(cm_energy_gev);
        break;
    case QED_PROC_THOMSON:
        sigma = qed_thomson_cross_section();
        break;
    default:
        break;
    }

    sim->stats.total_processes_computed++;
    if (sigma > sim->stats.max_cross_section)
        sim->stats.max_cross_section = sigma;

    /* Update process record if it exists */
    for (uint32_t i = 0; i < sim->num_processes; i++) {
        if (sim->processes[i].type == type) {
            sim->processes[i].sigma_total = sigma;
            sim->processes[i].cm_energy = cm_energy_gev;
            sim->processes[i].scattering_angle = angle_rad;
            break;
        }
    }

    return sigma;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

qed_config_t qed_default_config(void) {
    return (qed_config_t){
        .enable_radiative_corrections = true,
        .enable_vacuum_polarization = true,
        .perturbation_order = 1,
        .ir_cutoff = 1e-6f,
    };
}

qed_sim_t* qed_create(const qed_config_t* config) {
    qed_config_t cfg = config ? *config : qed_default_config();
    qed_sim_t* sim = nimcp_calloc(1, sizeof(*sim));
    if (!sim) return NULL;
    sim->config = cfg;

    /* Pre-compute precision QED values */
    sim->stats.anomalous_magnetic_moment = qed_anomalous_moment_third_order();
    sim->stats.lamb_shift_2s_2p = qed_lamb_shift_hydrogen();

    sim->initialized = true;
    LOG_INFO(LOG_TAG, "QED engine created: order=%u, a_e=%.12f, Lamb=%.1f MHz",
             cfg.perturbation_order,
             sim->stats.anomalous_magnetic_moment,
             sim->stats.lamb_shift_2s_2p);
    return sim;
}

void qed_destroy(qed_sim_t* sim) {
    if (!sim) return;
    nimcp_free(sim);
}

void qed_load_standard_processes(qed_sim_t* sim) {
    if (!sim) return;
    struct { const char* name; qed_process_type_t type; float threshold; } procs[] = {
        {"Compton (Klein-Nishina)",     QED_PROC_COMPTON, 0},
        {"Pair production",             QED_PROC_PAIR_PRODUCTION, 2*QED_ELECTRON_MASS},
        {"Pair annihilation",           QED_PROC_PAIR_ANNIHILATION, 2*QED_ELECTRON_MASS},
        {"Bremsstrahlung",              QED_PROC_BREMSSTRAHLUNG, 0},
        {"Moller (e-e-)",               QED_PROC_MOLLER, 2*QED_ELECTRON_MASS},
        {"Bhabha (e+e-)",               QED_PROC_BHABHA, 2*QED_ELECTRON_MASS},
        {"Breit-Wheeler (gamma-gamma)", QED_PROC_PHOTON_PHOTON, 2*QED_ELECTRON_MASS},
        {"Thomson (low-E Compton)",     QED_PROC_THOMSON, 0},
    };
    for (uint32_t i = 0; i < sizeof(procs)/sizeof(procs[0]) && sim->num_processes < QED_MAX_PROCESSES; i++) {
        qed_process_t p = {0};
        p.id = sim->num_processes;
        strncpy(p.name, procs[i].name, QED_MAX_NAME - 1);
        p.type = procs[i].type;
        p.threshold_energy = procs[i].threshold;
        p.active = true;
        sim->processes[sim->num_processes++] = p;
    }
}

qed_stats_t qed_get_stats(const qed_sim_t* sim) {
    if (!sim) return (qed_stats_t){0};
    return sim->stats;
}
