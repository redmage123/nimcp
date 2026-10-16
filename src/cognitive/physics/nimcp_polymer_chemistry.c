/**
 * @file nimcp_polymer_chemistry.c
 * @brief Polymer Chemistry simulator — polymerization kinetics, MW, Tg, viscosity
 *
 * Chain polymerization: rate = kp*[M]*[M*], kinetic chain length.
 * Step-growth: Carothers equation DP = 1/(1-p).
 * MW distribution: Mn = M0*DP_n, PDI = 1+p.
 * Glass transition: Fox equation.
 * Rubber elasticity: sigma = nkT(lambda - 1/lambda^2).
 * Mark-Houwink: [eta] = K*M^a.
 * Flory-Huggins: dG_mix = RT(n1*ln(phi1) + n2*ln(phi2) + chi*n1*phi2).
 */

#include "cognitive/physics/nimcp_polymer_chemistry.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <math.h>
#include <string.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define LOG_TAG "POLYCHEM"

#define CONC_EPSILON    1e-15f
#define DEFAULT_TEMP    298.15f

/* ============================================================================
 * Default config
 * ============================================================================ */

polymer_chemistry_config_t polymer_chemistry_default_config(void)
{
    polymer_chemistry_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.dt = 0.001f;
    cfg.temperature = DEFAULT_TEMP;
    cfg.enable_crosslinking = false;
    cfg.enabled = true;
    return cfg;
}

/* ============================================================================
 * Create / Destroy
 * ============================================================================ */

polymer_chemistry_sim_t* polymer_chemistry_create(const polymer_chemistry_config_t* config)
{
    polymer_chemistry_sim_t* sim =
        (polymer_chemistry_sim_t*)nimcp_calloc(1, sizeof(polymer_chemistry_sim_t));
    if (!sim) {
        LOG_ERROR(LOG_TAG, "Failed to allocate polymer_chemistry_sim_t");
        return NULL;
    }
    sim->config = config ? *config : polymer_chemistry_default_config();
    sim->initialized = true;
    LOG_INFO(LOG_TAG, "Polymer chemistry sim created (T=%.1f K)", sim->config.temperature);
    return sim;
}

void polymer_chemistry_destroy(polymer_chemistry_sim_t* sim)
{
    if (!sim) return;
    LOG_INFO(LOG_TAG, "Destroying polymer chemistry sim (steps=%lu)",
             (unsigned long)sim->stats.step_count);
    nimcp_free(sim);
}

/* ============================================================================
 * Entity management
 * ============================================================================ */

uint32_t polymer_chemistry_add_monomer(polymer_chemistry_sim_t* sim, const polychem_monomer_t* m)
{
    if (!sim || !m) return UINT32_MAX;
    if (sim->num_monomers >= POLYCHEM_MAX_MONOMERS) {
        LOG_WARN(LOG_TAG, "Max monomers reached (%d)", POLYCHEM_MAX_MONOMERS);
        return UINT32_MAX;
    }
    uint32_t idx = sim->num_monomers++;
    sim->monomers[idx] = *m;
    sim->monomers[idx].id = idx;
    sim->monomers[idx].active = true;
    return idx;
}

uint32_t polymer_chemistry_add_polymer(polymer_chemistry_sim_t* sim, const polychem_polymer_t* p)
{
    if (!sim || !p) return UINT32_MAX;
    if (sim->num_polymers >= POLYCHEM_MAX_POLYMERS) {
        LOG_WARN(LOG_TAG, "Max polymers reached (%d)", POLYCHEM_MAX_POLYMERS);
        return UINT32_MAX;
    }
    uint32_t idx = sim->num_polymers++;
    sim->polymers[idx] = *p;
    sim->polymers[idx].id = idx;
    sim->polymers[idx].active = true;
    return idx;
}

uint32_t polymer_chemistry_add_reaction(polymer_chemistry_sim_t* sim, const polychem_reaction_t* r)
{
    if (!sim || !r) return UINT32_MAX;
    if (sim->num_reactions >= POLYCHEM_MAX_REACTIONS) {
        LOG_WARN(LOG_TAG, "Max reactions reached (%d)", POLYCHEM_MAX_REACTIONS);
        return UINT32_MAX;
    }
    uint32_t idx = sim->num_reactions++;
    sim->reactions[idx] = *r;
    sim->reactions[idx].id = idx;
    sim->reactions[idx].active = true;
    return idx;
}

/* ============================================================================
 * Load common polymers database
 * ============================================================================ */

int polymer_chemistry_load_common_polymers(polymer_chemistry_sim_t* sim)
{
    if (!sim) return -1;

    /* Common monomers */
    static const struct { const char* name; polychem_monomer_id_t type; float mw; float func; float tg; } mono_db[] = {
        { "ethylene",          POLYCHEM_MONO_ETHYLENE,    POLYCHEM_MW_ETHYLENE,   2.0f, POLYCHEM_TG_PE },
        { "propylene",         POLYCHEM_MONO_PROPYLENE,   POLYCHEM_MW_PROPYLENE,  2.0f, POLYCHEM_TG_PP },
        { "styrene",           POLYCHEM_MONO_STYRENE,     POLYCHEM_MW_STYRENE,    2.0f, POLYCHEM_TG_PS },
        { "isoprene",          POLYCHEM_MONO_ISOPRENE,    POLYCHEM_MW_ISOPRENE,   2.0f, POLYCHEM_TG_RUBBER },
    };

    for (uint32_t i = 0; i < sizeof(mono_db)/sizeof(mono_db[0]); i++) {
        polychem_monomer_t m;
        memset(&m, 0, sizeof(m));
        strncpy(m.name, mono_db[i].name, POLYCHEM_MAX_NAME - 1);
        m.type = mono_db[i].type;
        m.mw = mono_db[i].mw;
        m.functionality = mono_db[i].func;
        m.tg = mono_db[i].tg;
        m.concentration = 1.0f;
        polymer_chemistry_add_monomer(sim, &m);
    }

    /* Common polymers with Mark-Houwink parameters */
    static const struct { const char* name; polychem_polymer_id_t type; float mono_mw; float mh_K; float mh_a; float tg; } poly_db[] = {
        { "polyethylene",      POLYCHEM_POLYMER_PE,       POLYCHEM_MW_ETHYLENE,   POLYCHEM_MH_PE_K, POLYCHEM_MH_PE_A, POLYCHEM_TG_PE },
        { "polypropylene",     POLYCHEM_POLYMER_PP,       POLYCHEM_MW_PROPYLENE,  0.011f, 0.80f, POLYCHEM_TG_PP },
        { "polystyrene",       POLYCHEM_POLYMER_PS,       POLYCHEM_MW_STYRENE,    POLYCHEM_MH_PS_K, POLYCHEM_MH_PS_A, POLYCHEM_TG_PS },
        { "PET",               POLYCHEM_POLYMER_PET,      POLYCHEM_MW_PET_REPEAT, 0.0047f, 0.658f, POLYCHEM_TG_PET },
        { "nylon-6,6",         POLYCHEM_POLYMER_NYLON66,  POLYCHEM_MW_NYLON66,    0.0110f, 0.72f, POLYCHEM_TG_NYLON66 },
        { "natural rubber",    POLYCHEM_POLYMER_NATURAL_RUBBER, POLYCHEM_MW_ISOPRENE, 0.0505f, 0.667f, POLYCHEM_TG_RUBBER },
    };

    for (uint32_t i = 0; i < sizeof(poly_db)/sizeof(poly_db[0]); i++) {
        polychem_polymer_t p;
        memset(&p, 0, sizeof(p));
        strncpy(p.name, poly_db[i].name, POLYCHEM_MAX_NAME - 1);
        p.type = poly_db[i].type;
        p.monomer_mw = poly_db[i].mono_mw;
        p.mh_K = poly_db[i].mh_K;
        p.mh_a = poly_db[i].mh_a;
        p.tg = poly_db[i].tg;
        p.dp_n = 100.0f;
        p.conversion = 0.0f;
        polymer_chemistry_add_polymer(sim, &p);
    }

    LOG_INFO(LOG_TAG, "Loaded %u monomers and %u polymers", sim->num_monomers, sim->num_polymers);
    return 0;
}

/* ============================================================================
 * Chain polymerization: rate = kp * [M] * [M*]
 *   [M*] = radical concentration = (f*kd*[I]/kt)^0.5 (steady-state)
 * ============================================================================ */

float polymer_chemistry_chain_rate(float kp, float monomer_conc, float radical_conc)
{
    if (kp <= 0.0f || monomer_conc <= CONC_EPSILON || radical_conc <= CONC_EPSILON)
        return 0.0f;
    return kp * monomer_conc * radical_conc;
}

/* ============================================================================
 * Kinetic chain length: v = kp*[M] / (2*kt*f*[I])^0.5
 * With termination by combination: DP_n = 2*v
 * With termination by disproportionation: DP_n = v
 * ============================================================================ */

float polymer_chemistry_kinetic_chain_length(float kp, float monomer_conc,
                                              float kt, float f, float initiator_conc)
{
    if (kp <= 0.0f || monomer_conc <= CONC_EPSILON) return 0.0f;
    float denom = 2.0f * kt * f * initiator_conc;
    if (denom <= CONC_EPSILON) return 0.0f;
    return kp * monomer_conc / sqrtf(denom);
}

/* ============================================================================
 * Carothers equation for step-growth: DP_n = 1 / (1 - p)
 * where p = conversion (extent of reaction)
 * ============================================================================ */

float polymer_chemistry_carothers_dp(float conversion)
{
    if (conversion < 0.0f) conversion = 0.0f;
    if (conversion >= 1.0f) conversion = 0.9999f;
    return 1.0f / (1.0f - conversion);
}

/* ============================================================================
 * MW distribution computation
 * Mn = M0 * DP_n
 * For most probable distribution (step-growth): PDI = 1 + p
 * For free-radical (combination): PDI = 1.5
 * For free-radical (disproportionation): PDI = 2.0
 * Mw = Mn * PDI
 * ============================================================================ */

void polymer_chemistry_compute_mw(polychem_polymer_t* polymer)
{
    if (!polymer) return;
    if (polymer->dp_n < 1.0f) polymer->dp_n = 1.0f;

    polymer->mn = polymer->monomer_mw * polymer->dp_n;

    /* PDI for step-growth: 1+p, for chain-growth use 2.0 as default */
    if (polymer->conversion > 0.0f && polymer->conversion < 1.0f) {
        polymer->pdi = 1.0f + polymer->conversion;
    } else {
        polymer->pdi = 2.0f;  /* most probable */
    }

    polymer->mw = polymer->mn * polymer->pdi;

    /* Update intrinsic viscosity via Mark-Houwink */
    if (polymer->mh_K > 0.0f && polymer->mw > 0.0f) {
        polymer->intrinsic_visc = polymer_chemistry_intrinsic_viscosity(
            polymer->mh_K, polymer->mh_a, polymer->mw);
    }
}

/* ============================================================================
 * Fox equation: 1/Tg = sum(wi / Tg_i)
 * For copolymers and polymer blends
 * ============================================================================ */

float polymer_chemistry_fox_tg(const float* weight_fracs, const float* tg_values, uint32_t n)
{
    if (!weight_fracs || !tg_values || n == 0) return 0.0f;

    float inv_tg = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        if (tg_values[i] > 0.0f) {
            inv_tg += weight_fracs[i] / tg_values[i];
        }
    }
    if (inv_tg <= 0.0f) return 0.0f;
    return 1.0f / inv_tg;
}

/* ============================================================================
 * Rubber elasticity (neo-Hookean / affine network model)
 * sigma = n * k * T * (lambda - 1/lambda^2)
 * where n = crosslink density (mol/m^3), k = Boltzmann
 * Modulus E = 3*n*k*T (small strain)
 * ============================================================================ */

polychem_rubber_result_t polymer_chemistry_rubber_stress(float crosslink_density,
                                                          float temperature, float lambda)
{
    polychem_rubber_result_t res;
    memset(&res, 0, sizeof(res));

    res.crosslink_density = crosslink_density;
    res.lambda = lambda;

    if (lambda <= 0.0f) lambda = 1.0f;
    if (crosslink_density <= 0.0f || temperature <= 0.0f) return res;

    /* n*k*T where n is in mol/m^3, use R instead of k for molar quantities */
    float nRT = crosslink_density * POLYCHEM_R_GAS * temperature;

    /* sigma = nRT * (lambda - 1/lambda^2) */
    float inv_lam2 = 1.0f / (lambda * lambda);
    res.stress = nRT * (lambda - inv_lam2);

    /* Young's modulus at small strain: E = 3*nRT */
    res.modulus = 3.0f * nRT;

    return res;
}

/* ============================================================================
 * Mark-Houwink equation: [eta] = K * M^a
 * Intrinsic viscosity in mL/g
 * K and a are polymer-solvent-temperature specific
 * a = 0.5 (theta solvent), 0.6-0.8 (good solvent), 1.0 (rod-like)
 * ============================================================================ */

float polymer_chemistry_intrinsic_viscosity(float K, float a, float mw)
{
    if (K <= 0.0f || mw <= 0.0f) return 0.0f;
    return K * powf(mw, a);
}

/* ============================================================================
 * Flory-Huggins mixing free energy
 * dG_mix = RT * (n1*ln(phi1) + n2*ln(phi2) + chi*n1*phi2)
 * dS_mix = -R * (n1*ln(phi1) + n2*ln(phi2))   (combinatorial entropy)
 * dH_mix = R * T * chi * n1 * phi2             (enthalpy from interactions)
 * Miscible if dG_mix < 0 AND d2G/dphi2 > 0
 * ============================================================================ */

polychem_mixing_result_t polymer_chemistry_flory_huggins(float n1, float phi1,
                                                          float n2, float phi2,
                                                          float chi, float temperature)
{
    polychem_mixing_result_t res;
    memset(&res, 0, sizeof(res));
    res.chi = chi;

    if (temperature <= 0.0f) temperature = DEFAULT_TEMP;

    /* Clamp volume fractions to avoid log(0) */
    if (phi1 < CONC_EPSILON) phi1 = CONC_EPSILON;
    if (phi2 < CONC_EPSILON) phi2 = CONC_EPSILON;

    float RT = POLYCHEM_R_GAS * temperature;

    /* Combinatorial entropy of mixing (Flory-Huggins lattice model) */
    float entropy_term = n1 * logf(phi1) + n2 * logf(phi2);
    res.dS_mix = -POLYCHEM_R_GAS * entropy_term;

    /* Enthalpy of mixing (chi parameter) */
    float enthalpy_term = chi * n1 * phi2;
    res.dH_mix = RT * enthalpy_term;

    /* Gibbs free energy of mixing */
    res.dG_mix = RT * (entropy_term + enthalpy_term);

    res.miscible = (res.dG_mix < 0.0f);

    return res;
}

/* ============================================================================
 * Step — advance polymerization reactions
 * ============================================================================ */

int polymer_chemistry_step(polymer_chemistry_sim_t* sim, float dt)
{
    if (!sim || !sim->initialized) return -1;
    if (dt <= 0.0f) dt = sim->config.dt;
    if (dt <= 0.0f) dt = 0.001f;

    float T = sim->config.temperature;
    if (T <= 0.0f) T = DEFAULT_TEMP;

    for (uint32_t i = 0; i < sim->num_reactions; i++) {
        polychem_reaction_t* rxn = &sim->reactions[i];
        if (!rxn->active) continue;
        if (rxn->monomer_idx >= sim->num_monomers) continue;
        if (rxn->polymer_idx >= sim->num_polymers) continue;

        polychem_monomer_t* mono = &sim->monomers[rxn->monomer_idx];
        polychem_polymer_t* poly = &sim->polymers[rxn->polymer_idx];

        /* Temperature-adjusted rate via Arrhenius */
        float kp = rxn->kp;
        if (rxn->activation_energy > 0.0f) {
            kp *= expf(-rxn->activation_energy * 1000.0f / (POLYCHEM_R_GAS * T));
        }

        if (rxn->type == POLYCHEM_CHAIN_FREE_RADICAL ||
            rxn->type == POLYCHEM_CHAIN_CATIONIC ||
            rxn->type == POLYCHEM_CHAIN_ANIONIC ||
            rxn->type == POLYCHEM_CHAIN_COORDINATION) {
            /* Chain-growth polymerization */
            /* Steady-state radical concentration: [M*] = (f*ki*[I]/kt)^0.5 */
            float ki = rxn->ki > 0.0f ? rxn->ki : 1e-5f;
            float kt = rxn->kt > CONC_EPSILON ? rxn->kt : 1e7f;
            float f = rxn->f > 0.0f ? rxn->f : 0.5f;
            float I_conc = rxn->initiator_conc > 0.0f ? rxn->initiator_conc : 0.01f;

            float radical_conc = sqrtf(f * ki * I_conc / kt);
            float rate = polymer_chemistry_chain_rate(kp, mono->concentration, radical_conc);
            float consumed = rate * dt;
            if (consumed > mono->concentration) consumed = mono->concentration;
            mono->concentration -= consumed;

            /* Update DP via kinetic chain length */
            float v = polymer_chemistry_kinetic_chain_length(kp, mono->concentration, kt, f, I_conc);
            poly->dp_n = 2.0f * v;  /* combination termination */
            poly->conversion = 1.0f - (mono->concentration /
                (mono->concentration + consumed + CONC_EPSILON));
        } else {
            /* Step-growth (condensation) */
            /* dp/dt = k * (1-p)^2 * [functional groups] */
            float p = poly->conversion;
            float dp = kp * (1.0f - p) * (1.0f - p) * mono->concentration * dt;
            p += dp;
            if (p > 0.9999f) p = 0.9999f;
            if (p < 0.0f) p = 0.0f;
            poly->conversion = p;
            poly->dp_n = polymer_chemistry_carothers_dp(p);
        }

        /* Recompute MW distribution */
        polymer_chemistry_compute_mw(poly);

        /* Update Tg for copolymer if applicable */
        if (poly->num_components > 1) {
            poly->tg = polymer_chemistry_fox_tg(poly->weight_fractions,
                                                 poly->tg_components,
                                                 poly->num_components);
        }

        /* Cross-linking increases modulus */
        if (sim->config.enable_crosslinking && poly->crosslink_density > 0.0f) {
            polychem_rubber_result_t rub = polymer_chemistry_rubber_stress(
                poly->crosslink_density, T, 1.01f);
            /* Track as max_value for stats */
            if (rub.modulus > sim->stats.max_value) {
                sim->stats.max_value = rub.modulus;
            }
        }
    }

    /* Update aggregate stats */
    float sum_mn = 0.0f, sum_mw = 0.0f, sum_pdi = 0.0f, sum_conv = 0.0f;
    uint32_t active = 0;
    for (uint32_t i = 0; i < sim->num_polymers; i++) {
        if (!sim->polymers[i].active) continue;
        sum_mn  += sim->polymers[i].mn;
        sum_mw  += sim->polymers[i].mw;
        sum_pdi += sim->polymers[i].pdi;
        sum_conv += sim->polymers[i].conversion;
        active++;
    }
    if (active > 0) {
        sim->stats.avg_mn  = sum_mn  / (float)active;
        sim->stats.avg_mw  = sum_mw  / (float)active;
        sim->stats.avg_pdi = sum_pdi / (float)active;
        sim->stats.avg_conversion = sum_conv / (float)active;
    }

    sim->stats.step_count++;
    sim->time += dt;
    return 0;
}

/* ============================================================================
 * Stats
 * ============================================================================ */

polymer_chemistry_stats_t polymer_chemistry_get_stats(const polymer_chemistry_sim_t* sim)
{
    polymer_chemistry_stats_t zero;
    memset(&zero, 0, sizeof(zero));
    if (!sim) return zero;
    return sim->stats;
}
