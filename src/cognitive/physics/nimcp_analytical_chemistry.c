/**
 * @file nimcp_analytical_chemistry.c
 * @brief Analytical Chemistry simulator — titrations, chromatography, spectrophotometry
 *
 * Strong/weak acid-base titrations, Henderson-Hasselbalch, buffer capacity,
 * Van Deemter plate theory, Beer-Lambert law, linear calibration with LOD,
 * Nernst potentiometry.
 */

#include "cognitive/physics/nimcp_analytical_chemistry.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <math.h>
#include <string.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define LOG_TAG "ANACHEM"

#define CONC_EPSILON    1e-15f
#define DEFAULT_TEMP    298.15f

/* ============================================================================
 * Default config
 * ============================================================================ */

analytical_chemistry_config_t analytical_chemistry_default_config(void)
{
    analytical_chemistry_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.dt = 0.1f;          /* mL per step for titrations */
    cfg.temperature = DEFAULT_TEMP;
    cfg.enabled = true;
    return cfg;
}

/* ============================================================================
 * Create / Destroy
 * ============================================================================ */

analytical_chemistry_sim_t* analytical_chemistry_create(const analytical_chemistry_config_t* config)
{
    analytical_chemistry_sim_t* sim =
        (analytical_chemistry_sim_t*)nimcp_calloc(1, sizeof(analytical_chemistry_sim_t));
    if (!sim) {
        LOG_ERROR(LOG_TAG, "Failed to allocate analytical_chemistry_sim_t");
        return NULL;
    }
    sim->config = config ? *config : analytical_chemistry_default_config();
    sim->initialized = true;
    LOG_INFO(LOG_TAG, "Analytical chemistry sim created (T=%.1f K)", sim->config.temperature);
    return sim;
}

void analytical_chemistry_destroy(analytical_chemistry_sim_t* sim)
{
    if (!sim) return;
    LOG_INFO(LOG_TAG, "Destroying analytical chemistry sim (steps=%lu)",
             (unsigned long)sim->stats.step_count);
    nimcp_free(sim);
}

/* ============================================================================
 * Entity management
 * ============================================================================ */

uint32_t analytical_chemistry_add_solution(analytical_chemistry_sim_t* sim,
                                            const anachem_solution_t* s)
{
    if (!sim || !s) return UINT32_MAX;
    if (sim->num_solutions >= ANACHEM_MAX_SOLUTIONS) {
        LOG_WARN(LOG_TAG, "Max solutions reached (%d)", ANACHEM_MAX_SOLUTIONS);
        return UINT32_MAX;
    }
    uint32_t idx = sim->num_solutions++;
    sim->solutions[idx] = *s;
    sim->solutions[idx].id = idx;
    sim->solutions[idx].active = true;
    return idx;
}

uint32_t analytical_chemistry_add_titration(analytical_chemistry_sim_t* sim,
                                             const anachem_titration_t* t)
{
    if (!sim || !t) return UINT32_MAX;
    if (sim->num_titrations >= ANACHEM_MAX_TITRATIONS) {
        LOG_WARN(LOG_TAG, "Max titrations reached (%d)", ANACHEM_MAX_TITRATIONS);
        return UINT32_MAX;
    }
    uint32_t idx = sim->num_titrations++;
    sim->titrations[idx] = *t;
    sim->titrations[idx].id = idx;
    sim->titrations[idx].active = true;
    return idx;
}

uint32_t analytical_chemistry_add_column(analytical_chemistry_sim_t* sim,
                                          const anachem_column_t* c)
{
    if (!sim || !c) return UINT32_MAX;
    if (sim->num_columns >= ANACHEM_MAX_COLUMNS) {
        LOG_WARN(LOG_TAG, "Max columns reached (%d)", ANACHEM_MAX_COLUMNS);
        return UINT32_MAX;
    }
    uint32_t idx = sim->num_columns++;
    sim->columns[idx] = *c;
    sim->columns[idx].id = idx;
    sim->columns[idx].active = true;
    return idx;
}

/* ============================================================================
 * Load common buffers
 * ============================================================================ */

int analytical_chemistry_load_common_buffers(analytical_chemistry_sim_t* sim)
{
    if (!sim) return -1;

    static const struct { const char* name; anachem_buffer_id_t btype; anachem_solution_type_t stype; float pKa; float conc; } buf_db[] = {
        { "phosphate pH 7.2",  ANACHEM_BUFFER_PHOSPHATE, ANACHEM_BUFFER, ANACHEM_PKA_PHOSPHATE2, 0.1f },
        { "Tris pH 8.0",      ANACHEM_BUFFER_TRIS,      ANACHEM_BUFFER, ANACHEM_PKA_TRIS,       0.05f },
        { "HEPES pH 7.5",     ANACHEM_BUFFER_HEPES,     ANACHEM_BUFFER, ANACHEM_PKA_HEPES,      0.025f },
        { "acetate pH 4.8",   ANACHEM_BUFFER_ACETATE,   ANACHEM_BUFFER, ANACHEM_PKA_ACETIC,     0.1f },
        { "carbonate pH 10",  ANACHEM_BUFFER_CARBONATE, ANACHEM_BUFFER, ANACHEM_PKA_CARBONIC2,  0.05f },
        { "citrate pH 4.8",   ANACHEM_BUFFER_CITRATE,   ANACHEM_BUFFER, ANACHEM_PKA_CITRIC2,    0.1f },
    };

    for (uint32_t i = 0; i < sizeof(buf_db)/sizeof(buf_db[0]); i++) {
        anachem_solution_t s;
        memset(&s, 0, sizeof(s));
        strncpy(s.name, buf_db[i].name, ANACHEM_MAX_NAME - 1);
        s.type = buf_db[i].stype;
        s.pKa = buf_db[i].pKa;
        s.concentration = buf_db[i].conc;
        s.volume = 0.1f; /* 100 mL default */
        s.pH = s.pKa;    /* buffer at pKa => pH = pKa */
        analytical_chemistry_add_solution(sim, &s);
    }

    LOG_INFO(LOG_TAG, "Loaded %u common buffers", sim->num_solutions);
    return 0;
}

/* ============================================================================
 * pH calculations
 * ============================================================================ */

/* Strong acid: pH = -log10([H+]) = -log10(C) */
float analytical_chemistry_strong_acid_pH(float concentration)
{
    if (concentration <= CONC_EPSILON) return 7.0f;
    return -log10f(concentration);
}

/* Strong base: pOH = -log10(C), pH = 14 - pOH */
float analytical_chemistry_strong_base_pH(float concentration)
{
    if (concentration <= CONC_EPSILON) return 7.0f;
    float pOH = -log10f(concentration);
    return 14.0f - pOH;
}

/* Weak acid: pH = 0.5*(pKa - log10(C)) via quadratic from Ka = x^2/(C-x) */
float analytical_chemistry_weak_acid_pH(float concentration, float pKa)
{
    if (concentration <= CONC_EPSILON) return 7.0f;
    float Ka = powf(10.0f, -pKa);
    /* Quadratic: x^2 + Ka*x - Ka*C = 0 => x = (-Ka + sqrt(Ka^2 + 4*Ka*C))/2 */
    float disc = Ka * Ka + 4.0f * Ka * concentration;
    float H = (-Ka + sqrtf(disc)) / 2.0f;
    if (H <= CONC_EPSILON) return 7.0f;
    return -log10f(H);
}

/* Weak base: Kb = Kw/Ka, pOH from Kb, pH = 14 - pOH */
float analytical_chemistry_weak_base_pH(float concentration, float pKb)
{
    if (concentration <= CONC_EPSILON) return 7.0f;
    float Kb = powf(10.0f, -pKb);
    float disc = Kb * Kb + 4.0f * Kb * concentration;
    float OH = (-Kb + sqrtf(disc)) / 2.0f;
    if (OH <= CONC_EPSILON) return 7.0f;
    float pOH = -log10f(OH);
    return 14.0f - pOH;
}

/* ============================================================================
 * Henderson-Hasselbalch: pH = pKa + log10([A-]/[HA])
 * ============================================================================ */

float analytical_chemistry_henderson_hasselbalch(float pKa, float conc_base, float conc_acid)
{
    if (conc_acid <= CONC_EPSILON || conc_base <= CONC_EPSILON) return pKa;
    return pKa + log10f(conc_base / conc_acid);
}

/* ============================================================================
 * Buffer capacity: beta = 2.303 * C * Ka * [H+] / (Ka + [H+])^2
 * Maximum at pH = pKa (where [H+] = Ka)
 * ============================================================================ */

float analytical_chemistry_buffer_capacity(float total_conc, float Ka, float H_conc)
{
    if (total_conc <= 0.0f || Ka <= 0.0f || H_conc <= 0.0f) return 0.0f;
    float sum = Ka + H_conc;
    if (sum <= CONC_EPSILON) return 0.0f;
    return ANACHEM_LN10 * total_conc * Ka * H_conc / (sum * sum);
}

/* ============================================================================
 * Titration pH computation
 *
 * For strong acid (analyte) + strong base (titrant):
 * - Before equivalence: excess H+ => pH = -log([H+]_excess)
 * - At equivalence: pH = 7
 * - After equivalence: excess OH- => pH = 14 + log([OH-]_excess)
 *
 * For weak acid + strong base: Henderson-Hasselbalch in buffer region
 * ============================================================================ */

float analytical_chemistry_titration_pH(const anachem_titration_t* tit,
                                         const anachem_solution_t* analyte,
                                         const anachem_solution_t* titrant,
                                         float vol_added_mL)
{
    if (!tit || !analyte || !titrant) return 7.0f;

    float Ca = analyte->concentration;
    float Va = analyte->volume * 1000.0f;  /* L to mL */
    float Cb = titrant->concentration;
    float Vb = vol_added_mL;

    float mol_acid = Ca * Va / 1000.0f;
    float mol_base = Cb * Vb / 1000.0f;
    float total_vol_L = (Va + Vb) / 1000.0f;

    if (total_vol_L <= 0.0f) return 7.0f;

    if (tit->type == ANACHEM_TITRATION_SA_SB) {
        /* Strong acid + strong base */
        if (mol_base < mol_acid - CONC_EPSILON) {
            /* Before equivalence: excess H+ */
            float H_excess = (mol_acid - mol_base) / total_vol_L;
            if (H_excess < 1e-14f) H_excess = 1e-14f;  /* floor to avoid log(<=0) */
            return -log10f(H_excess);
        } else if (mol_base > mol_acid + CONC_EPSILON) {
            /* After equivalence: excess OH- */
            float OH_excess = (mol_base - mol_acid) / total_vol_L;
            if (OH_excess < 1e-14f) OH_excess = 1e-14f;
            float pOH = -log10f(OH_excess);
            return 14.0f - pOH;
        } else {
            return 7.0f;  /* at equivalence */
        }
    } else if (tit->type == ANACHEM_TITRATION_WA_SB) {
        /* Weak acid + strong base */
        float Ka = powf(10.0f, -analyte->pKa);
        if (mol_base < CONC_EPSILON) {
            /* Initial: weak acid alone */
            return analytical_chemistry_weak_acid_pH(Ca, analyte->pKa);
        } else if (mol_base < mol_acid - CONC_EPSILON) {
            /* Buffer region: Henderson-Hasselbalch */
            float HA = mol_acid - mol_base;
            float A_minus = mol_base;
            return analytical_chemistry_henderson_hasselbalch(
                analyte->pKa, A_minus, HA);
        } else if (fabsf(mol_base - mol_acid) < CONC_EPSILON) {
            /* Equivalence: hydrolysis of conjugate base A- */
            float conc_A = mol_acid / total_vol_L;
            float Kb = ANACHEM_KW / Ka;
            float disc = Kb * Kb + 4.0f * Kb * conc_A;
            float OH = (-Kb + sqrtf(disc)) / 2.0f;
            if (OH > CONC_EPSILON) return 14.0f + log10f(OH);
            return 7.0f;
        } else {
            /* After equivalence: excess strong base */
            float OH_excess = (mol_base - mol_acid) / total_vol_L;
            return 14.0f - (-log10f(OH_excess));
        }
    }

    return 7.0f;  /* fallback */
}

/* ============================================================================
 * Van Deemter equation: H = A + B/u + C*u
 * A = eddy diffusion (multiple paths through packing)
 * B = longitudinal diffusion (axial diffusion in mobile phase)
 * C = mass transfer resistance (equilibration between phases)
 * ============================================================================ */

float analytical_chemistry_van_deemter(float A, float B, float C, float u)
{
    if (u <= CONC_EPSILON) return A + 1e6f;  /* effectively infinite at u=0 */
    return A + B / u + C * u;
}

/* Optimal flow rate: u_opt = sqrt(B/C), gives minimum H */
float analytical_chemistry_optimal_flow(float B, float C)
{
    if (B <= 0.0f || C <= 0.0f) return 1.0f;
    return sqrtf(B / C);
}

/* ============================================================================
 * Resolution: Rs = sqrt(N)/4 * (alpha-1)/alpha * k'/(1+k')
 * N = theoretical plates, alpha = selectivity factor, k' = retention factor
 * ============================================================================ */

float analytical_chemistry_resolution(uint32_t N, float alpha, float k_prime)
{
    if (N == 0 || alpha <= 1.0f || k_prime <= 0.0f) return 0.0f;
    float sqrtN = sqrtf((float)N);
    return (sqrtN / 4.0f) * ((alpha - 1.0f) / alpha) * (k_prime / (1.0f + k_prime));
}

/* ============================================================================
 * Beer-Lambert law: A = epsilon * l * c
 * Transmittance: T = 10^(-A) = I/I0
 * ============================================================================ */

anachem_beer_lambert_t analytical_chemistry_beer_lambert(float epsilon, float path_length,
                                                         float concentration)
{
    anachem_beer_lambert_t res;
    memset(&res, 0, sizeof(res));
    res.epsilon = epsilon;
    res.path_length = path_length;
    res.concentration = concentration;
    res.absorbance = epsilon * path_length * concentration;
    res.transmittance = powf(10.0f, -res.absorbance);
    return res;
}

/* ============================================================================
 * Linear calibration: y = slope*x + intercept via least-squares regression
 * LOD = 3*sigma/slope, LOQ = 10*sigma/slope
 * ============================================================================ */

int analytical_chemistry_calibrate(anachem_calibration_t* cal)
{
    if (!cal || cal->n_points < 2) return -1;

    uint32_t n = cal->n_points;
    float sum_x = 0.0f, sum_y = 0.0f, sum_xy = 0.0f, sum_x2 = 0.0f;

    for (uint32_t i = 0; i < n; i++) {
        sum_x  += cal->x[i];
        sum_y  += cal->y[i];
        sum_xy += cal->x[i] * cal->y[i];
        sum_x2 += cal->x[i] * cal->x[i];
    }

    float fn = (float)n;
    float denom = fn * sum_x2 - sum_x * sum_x;
    if (fabsf(denom) < CONC_EPSILON) return -1;

    cal->slope = (fn * sum_xy - sum_x * sum_y) / denom;
    cal->intercept = (sum_y - cal->slope * sum_x) / fn;

    /* R^2 and standard error */
    float y_mean = sum_y / fn;
    float ss_tot = 0.0f, ss_res = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        float y_pred = cal->slope * cal->x[i] + cal->intercept;
        float residual = cal->y[i] - y_pred;
        ss_res += residual * residual;
        float diff = cal->y[i] - y_mean;
        ss_tot += diff * diff;
    }

    cal->r_squared = (ss_tot > CONC_EPSILON) ? 1.0f - (ss_res / ss_tot) : 0.0f;
    cal->std_error = (n > 2) ? sqrtf(ss_res / (fn - 2.0f)) : 0.0f;

    /* LOD = 3*sigma/slope, LOQ = 10*sigma/slope */
    if (fabsf(cal->slope) > CONC_EPSILON) {
        cal->lod = 3.0f * cal->std_error / fabsf(cal->slope);
        cal->loq = 10.0f * cal->std_error / fabsf(cal->slope);
    }

    return 0;
}

/* ============================================================================
 * Nernst equation: E = E0 - (RT/(nF)) * ln(Q)
 * At 25C: E = E0 - (0.02569/n) * ln(Q) = E0 - (0.05916/n) * log10(Q)
 * ============================================================================ */

float analytical_chemistry_nernst(float E0, int n, float Q, float temperature)
{
    if (n <= 0 || Q <= 0.0f) return E0;
    if (temperature <= 0.0f) temperature = DEFAULT_TEMP;
    float factor = (ANACHEM_R_GAS * temperature) / ((float)n * ANACHEM_FARADAY);
    return E0 - factor * logf(Q);
}

/* ============================================================================
 * Step — advance titrations and update column metrics
 * ============================================================================ */

int analytical_chemistry_step(analytical_chemistry_sim_t* sim, float dt)
{
    if (!sim || !sim->initialized) return -1;
    if (dt <= 0.0f) dt = sim->config.dt;
    if (dt <= 0.0f) dt = 0.1f;

    /* Advance each active titration by dt mL */
    for (uint32_t i = 0; i < sim->num_titrations; i++) {
        anachem_titration_t* tit = &sim->titrations[i];
        if (!tit->active) continue;
        if (tit->analyte_idx >= sim->num_solutions) continue;
        if (tit->titrant_idx >= sim->num_solutions) continue;

        anachem_solution_t* analyte = &sim->solutions[tit->analyte_idx];
        anachem_solution_t* titrant = &sim->solutions[tit->titrant_idx];

        tit->volume_added += dt;

        /* Compute equivalence volume: V_eq = Ca*Va / Cb */
        if (titrant->concentration > CONC_EPSILON) {
            tit->equivalence_vol = (analyte->concentration * analyte->volume * 1000.0f)
                                   / titrant->concentration;
        }

        tit->current_pH = analytical_chemistry_titration_pH(
            tit, analyte, titrant, tit->volume_added);

        tit->past_equivalence = (tit->volume_added > tit->equivalence_vol);
        if (tit->past_equivalence && tit->volume_added > 2.0f * tit->equivalence_vol) {
            tit->active = false;
            sim->stats.titrations_completed++;
        }
    }

    /* Update chromatography columns */
    float total_res = 0.0f;
    uint32_t active_cols = 0;
    for (uint32_t i = 0; i < sim->num_columns; i++) {
        anachem_column_t* col = &sim->columns[i];
        if (!col->active) continue;

        col->plate_height = analytical_chemistry_van_deemter(
            col->A, col->B, col->C, col->flow_rate);
        if (col->plate_height > CONC_EPSILON) {
            col->theoretical_plates = (uint32_t)(col->length / col->plate_height);
        }
        col->resolution = analytical_chemistry_resolution(
            col->theoretical_plates, col->selectivity, col->retention_factor);
        total_res += col->resolution;
        active_cols++;
    }
    if (active_cols > 0) {
        sim->stats.avg_resolution = total_res / (float)active_cols;
    }

    sim->stats.step_count++;
    sim->time += dt;
    return 0;
}

/* ============================================================================
 * Stats
 * ============================================================================ */

analytical_chemistry_stats_t analytical_chemistry_get_stats(const analytical_chemistry_sim_t* sim)
{
    analytical_chemistry_stats_t zero;
    memset(&zero, 0, sizeof(zero));
    if (!sim) return zero;
    return sim->stats;
}
