/**
 * @file nimcp_geochemistry.c
 * @brief Geochemistry simulator — weathering, carbon cycle, ocean carbonate, isotopes
 *
 * Eh-pH (Pourbaix) via Nernst, Arrhenius weathering rates, carbon cycle
 * box model with reservoirs and fluxes, ocean carbonate speciation
 * (CO2/H2CO3/HCO3-/CO3^2-), isotope fractionation, Goldschmidt
 * classification, Stokes sedimentation.
 */

#include "cognitive/physics/nimcp_geochemistry.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <math.h>
#include <string.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define LOG_TAG "GEOCHEM"

#define CONC_EPSILON    1e-20f
#define DEFAULT_TEMP    288.15f     /* Earth surface average ~15C */
#define WATER_DENSITY   1025.0f    /* seawater kg/m^3 */
#define WATER_VISCOSITY 1.08e-3f   /* seawater Pa*s at 15C */

/* CO2 ppm to atm: 1 ppm ~ 1e-6 atm */
#define PPM_TO_ATM      1.0e-6f

/* GtC to mol conversion: 1 GtC = 1e15 g / 12 g/mol = 8.33e13 mol */
#define GTC_TO_MOL      8.33e13f

/* ============================================================================
 * Default config
 * ============================================================================ */

geochemistry_config_t geochemistry_default_config(void)
{
    geochemistry_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.dt = 1.0f;                  /* 1 year per step */
    cfg.temperature = DEFAULT_TEMP;
    cfg.atmospheric_co2 = 420.0f;   /* current ~420 ppm */
    cfg.enable_anthropogenic = true;
    cfg.enabled = true;
    return cfg;
}

/* ============================================================================
 * Create / Destroy
 * ============================================================================ */

geochemistry_sim_t* geochemistry_create(const geochemistry_config_t* config)
{
    geochemistry_sim_t* sim =
        (geochemistry_sim_t*)nimcp_calloc(1, sizeof(geochemistry_sim_t));
    if (!sim) {
        LOG_ERROR(LOG_TAG, "Failed to allocate geochemistry_sim_t");
        return NULL;
    }
    sim->config = config ? *config : geochemistry_default_config();
    sim->initialized = true;
    LOG_INFO(LOG_TAG, "Geochemistry sim created (T=%.1f K, CO2=%.0f ppm)",
             sim->config.temperature, sim->config.atmospheric_co2);
    return sim;
}

void geochemistry_destroy(geochemistry_sim_t* sim)
{
    if (!sim) return;
    LOG_INFO(LOG_TAG, "Destroying geochemistry sim (steps=%lu, time=%.1f yr)",
             (unsigned long)sim->stats.step_count, sim->time);
    nimcp_free(sim);
}

/* ============================================================================
 * Entity management
 * ============================================================================ */

uint32_t geochemistry_add_mineral(geochemistry_sim_t* sim, const geochem_mineral_t* m)
{
    if (!sim || !m) return UINT32_MAX;
    if (sim->num_minerals >= GEOCHEM_MAX_MINERALS) return UINT32_MAX;
    uint32_t idx = sim->num_minerals++;
    sim->minerals[idx] = *m;
    sim->minerals[idx].id = idx;
    sim->minerals[idx].active = true;
    return idx;
}

uint32_t geochemistry_add_reservoir(geochemistry_sim_t* sim, const geochem_reservoir_t* r)
{
    if (!sim || !r) return UINT32_MAX;
    if (sim->num_reservoirs >= GEOCHEM_MAX_RESERVOIRS) return UINT32_MAX;
    uint32_t idx = sim->num_reservoirs++;
    sim->reservoirs[idx] = *r;
    sim->reservoirs[idx].id = idx;
    sim->reservoirs[idx].active = true;
    return idx;
}

uint32_t geochemistry_add_isotope(geochemistry_sim_t* sim, const geochem_isotope_t* iso)
{
    if (!sim || !iso) return UINT32_MAX;
    if (sim->num_isotopes >= GEOCHEM_MAX_ISOTOPES) return UINT32_MAX;
    uint32_t idx = sim->num_isotopes++;
    sim->isotopes[idx] = *iso;
    sim->isotopes[idx].id = idx;
    return idx;
}

/* ============================================================================
 * Load Earth's carbon cycle
 * ============================================================================ */

int geochemistry_load_earth_carbon_cycle(geochemistry_sim_t* sim)
{
    if (!sim) return -1;

    /* Reservoirs */
    static const struct { const char* name; geochem_reservoir_id_t type; float mass; float T; float pH; } res_db[] = {
        { "atmosphere",      GEOCHEM_RES_ATMOSPHERE,     GEOCHEM_C_ATMOSPHERE,     288.15f, 0.0f },
        { "surface ocean",   GEOCHEM_RES_OCEAN_SURFACE,  GEOCHEM_C_OCEAN_SURFACE,  291.15f, 8.1f },
        { "deep ocean",      GEOCHEM_RES_OCEAN_DEEP,     GEOCHEM_C_OCEAN_DEEP,     275.15f, 7.8f },
        { "biosphere",       GEOCHEM_RES_BIOSPHERE,      GEOCHEM_C_BIOSPHERE,      288.15f, 0.0f },
        { "soil",            GEOCHEM_RES_SOIL,           GEOCHEM_C_SOIL,           283.15f, 0.0f },
        { "fossil fuels",    GEOCHEM_RES_FOSSIL,         GEOCHEM_C_FOSSIL,         0.0f,    0.0f },
        { "lithosphere",     GEOCHEM_RES_LITHOSPHERE,    GEOCHEM_C_LITHOSPHERE,    0.0f,    0.0f },
    };

    for (uint32_t i = 0; i < sizeof(res_db)/sizeof(res_db[0]); i++) {
        geochem_reservoir_t r;
        memset(&r, 0, sizeof(r));
        strncpy(r.name, res_db[i].name, GEOCHEM_MAX_NAME - 1);
        r.type = res_db[i].type;
        r.carbon_mass = res_db[i].mass;
        r.initial_mass = res_db[i].mass;
        r.temperature = res_db[i].T;
        r.pH = res_db[i].pH;
        geochemistry_add_reservoir(sim, &r);
    }

    /* Set up fluxes between reservoirs (GtC/yr) */
    /* Atmosphere <-> Biosphere: photosynthesis/respiration */
    if (sim->num_reservoirs > GEOCHEM_RES_BIOSPHERE) {
        sim->reservoirs[GEOCHEM_RES_ATMOSPHERE].flux_to[GEOCHEM_RES_BIOSPHERE] = GEOCHEM_FLUX_PHOTOSYNTHESIS;
        sim->reservoirs[GEOCHEM_RES_BIOSPHERE].flux_to[GEOCHEM_RES_ATMOSPHERE] = GEOCHEM_FLUX_RESPIRATION;
    }
    /* Atmosphere <-> Surface ocean: gas exchange */
    if (sim->num_reservoirs > GEOCHEM_RES_OCEAN_SURFACE) {
        sim->reservoirs[GEOCHEM_RES_ATMOSPHERE].flux_to[GEOCHEM_RES_OCEAN_SURFACE] = GEOCHEM_FLUX_OCEAN_UPTAKE;
        sim->reservoirs[GEOCHEM_RES_OCEAN_SURFACE].flux_to[GEOCHEM_RES_ATMOSPHERE] = GEOCHEM_FLUX_OCEAN_RELEASE;
    }
    /* Surface ocean <-> Deep ocean: thermohaline circulation */
    if (sim->num_reservoirs > GEOCHEM_RES_OCEAN_DEEP) {
        sim->reservoirs[GEOCHEM_RES_OCEAN_SURFACE].flux_to[GEOCHEM_RES_OCEAN_DEEP] = 40.0f;
        sim->reservoirs[GEOCHEM_RES_OCEAN_DEEP].flux_to[GEOCHEM_RES_OCEAN_SURFACE] = 38.0f;
    }
    /* Biosphere -> Soil: litter fall */
    if (sim->num_reservoirs > GEOCHEM_RES_SOIL) {
        sim->reservoirs[GEOCHEM_RES_BIOSPHERE].flux_to[GEOCHEM_RES_SOIL] = 60.0f;
        sim->reservoirs[GEOCHEM_RES_SOIL].flux_to[GEOCHEM_RES_ATMOSPHERE] = 60.0f;
    }

    /* Minerals */
    static const struct { const char* name; geochem_mineral_id_t type; float Ea; float k0; float dens; geochem_goldschmidt_t gs; } min_db[] = {
        { "quartz",      GEOCHEM_MIN_QUARTZ,    GEOCHEM_EA_QUARTZ,    GEOCHEM_K0_QUARTZ,    2650.0f, GEOCHEM_CLASS_LITHOPHILE },
        { "feldspar",    GEOCHEM_MIN_FELDSPAR,  GEOCHEM_EA_FELDSPAR,  GEOCHEM_K0_FELDSPAR,  2630.0f, GEOCHEM_CLASS_LITHOPHILE },
        { "olivine",     GEOCHEM_MIN_OLIVINE,   GEOCHEM_EA_OLIVINE,   GEOCHEM_K0_OLIVINE,   3300.0f, GEOCHEM_CLASS_LITHOPHILE },
        { "calcite",     GEOCHEM_MIN_CALCITE,   GEOCHEM_EA_CALCITE,   GEOCHEM_K0_CALCITE,   2710.0f, GEOCHEM_CLASS_LITHOPHILE },
        { "pyrite",      GEOCHEM_MIN_PYRITE,    GEOCHEM_EA_PYRITE,    GEOCHEM_K0_PYRITE,     5010.0f, GEOCHEM_CLASS_CHALCOPHILE },
    };

    for (uint32_t i = 0; i < sizeof(min_db)/sizeof(min_db[0]); i++) {
        geochem_mineral_t m;
        memset(&m, 0, sizeof(m));
        strncpy(m.name, min_db[i].name, GEOCHEM_MAX_NAME - 1);
        m.type = min_db[i].type;
        m.activation_energy = min_db[i].Ea;
        m.rate_constant_25C = min_db[i].k0;
        m.density = min_db[i].dens;
        m.goldschmidt = min_db[i].gs;
        m.surface_area = 1.0f;
        m.mass = 1000.0f;
        m.particle_radius = 1e-4f;  /* 100 um */
        geochemistry_add_mineral(sim, &m);
    }

    /* Initialize carbonate system */
    sim->carbonate.pCO2 = sim->config.atmospheric_co2 * PPM_TO_ATM;
    sim->carbonate.temperature = sim->config.temperature;
    geochemistry_compute_carbonate(&sim->carbonate);

    LOG_INFO(LOG_TAG, "Loaded Earth carbon cycle: %u reservoirs, %u minerals",
             sim->num_reservoirs, sim->num_minerals);
    return 0;
}

/* ============================================================================
 * Weathering rate: k = k0 * exp(-Ea / (R * T))
 * Arrhenius equation with mineral-specific Ea
 * ============================================================================ */

float geochemistry_weathering_rate(float k0, float Ea, float temperature)
{
    if (k0 <= 0.0f || temperature <= 0.0f) return 0.0f;
    /* Ea in kJ/mol, convert to J/mol */
    float exp_arg = -(Ea * 1000.0f) / (GEOCHEM_R_GAS * temperature);
    /* Clamp to prevent underflow/overflow */
    if (exp_arg < -80.0f) return 0.0f;
    if (exp_arg > 80.0f) exp_arg = 80.0f;
    return k0 * expf(exp_arg);
}

/* ============================================================================
 * Nernst Eh: E = E0 - (RT/(nF)) * ln(Q)
 * For Pourbaix diagrams, Q often depends on pH via [H+]
 * ============================================================================ */

float geochemistry_nernst_eh(float E0, int n_electrons, float Q, float temperature)
{
    if (n_electrons <= 0 || Q <= 0.0f) return E0;
    if (temperature <= 0.0f) temperature = DEFAULT_TEMP;
    float factor = (GEOCHEM_R_GAS * temperature) / ((float)n_electrons * GEOCHEM_FARADAY);
    return E0 - factor * logf(Q);
}

/* ============================================================================
 * Mineral stability at given Eh, pH (Pourbaix diagram bounds)
 * ============================================================================ */

bool geochemistry_mineral_stable(const geochem_mineral_t* mineral, float Eh, float pH)
{
    if (!mineral) return false;
    /* Check if Eh and pH fall within stability field */
    if (mineral->eh_min != 0.0f || mineral->eh_max != 0.0f) {
        if (Eh < mineral->eh_min || Eh > mineral->eh_max) return false;
    }
    if (mineral->ph_min != 0.0f || mineral->ph_max != 0.0f) {
        if (pH < mineral->ph_min || pH > mineral->ph_max) return false;
    }
    return true;
}

/* ============================================================================
 * Ocean carbonate system
 *
 * CO2(g) -> CO2(aq):  [CO2] = KH * pCO2
 * CO2(aq) + H2O -> H2CO3 -> HCO3- + H+:  Ka1 = [HCO3-][H+]/[H2CO3]
 * HCO3- -> CO3^2- + H+:  Ka2 = [CO3^2-][H+]/[HCO3-]
 *
 * Given pCO2, solve for speciation at given pH or self-consistently
 * ============================================================================ */

int geochemistry_compute_carbonate(geochem_carbonate_state_t* state)
{
    if (!state) return -1;

    float T = state->temperature;
    if (T <= 0.0f) T = DEFAULT_TEMP;

    /* Dissolved CO2 from Henry's law */
    state->H2CO3 = GEOCHEM_KH_CO2 * state->pCO2;

    /* If pH is given, compute speciation from that */
    if (state->pH > 0.0f) {
        float H = powf(10.0f, -state->pH);

        /* From Ka1: [HCO3-] = Ka1 * [H2CO3] / [H+] */
        state->HCO3 = GEOCHEM_KA1_CARBONIC * state->H2CO3 / (H + CONC_EPSILON);

        /* From Ka2: [CO3^2-] = Ka2 * [HCO3-] / [H+] */
        state->CO3 = GEOCHEM_KA2_CARBONIC * state->HCO3 / (H + CONC_EPSILON);
    } else {
        /* Self-consistent: assume charge balance for pure CO2 system */
        /* [H+] = sqrt(Ka1 * [H2CO3]) as first approximation */
        float H_approx = sqrtf(GEOCHEM_KA1_CARBONIC * state->H2CO3);
        if (H_approx < CONC_EPSILON) H_approx = 1e-7f;

        /* Iterative refinement with relaxation to prevent divergence */
        float H = H_approx;
        for (int iter = 0; iter < 5; iter++) {
            float hco3 = GEOCHEM_KA1_CARBONIC * state->H2CO3 / H;
            float co3 = GEOCHEM_KA2_CARBONIC * hco3 / H;
            float OH = GEOCHEM_KW / H;
            float H_new = hco3 + 2.0f * co3 + OH;
            if (H_new < CONC_EPSILON) H_new = CONC_EPSILON;
            /* Relaxation: blend old and new to stabilize convergence */
            H = 0.5f * H + 0.5f * H_new;
            /* Early exit if converged */
            if (fabsf(H_new - H) < 1e-12f) break;
        }

        state->HCO3 = GEOCHEM_KA1_CARBONIC * state->H2CO3 / H;
        state->CO3 = GEOCHEM_KA2_CARBONIC * state->HCO3 / H;
        state->pH = -log10f(H);
    }

    /* DIC = [CO2] + [HCO3-] + [CO3^2-] */
    state->DIC = state->H2CO3 + state->HCO3 + state->CO3;

    /* Alkalinity ~ [HCO3-] + 2*[CO3^2-] */
    state->alkalinity = state->HCO3 + 2.0f * state->CO3;

    /* Calcite saturation: Omega = [Ca2+][CO3^2-] / Ksp */
    /* Assume [Ca2+] ~ 0.010 mol/L for seawater */
    float Ca_conc = 0.010f;
    state->omega_calcite = Ca_conc * state->CO3 / GEOCHEM_KSP_CALCITE;

    return 0;
}

/* ============================================================================
 * Isotope fractionation
 * delta_product = delta_reactant + 1000 * (alpha - 1)
 * alpha = R_product / R_reactant (fractionation factor)
 * delta notation: delta = (R_sample/R_standard - 1) * 1000 (permil)
 * ============================================================================ */

float geochemistry_isotope_fractionate(float delta_reactant, float alpha)
{
    return delta_reactant + 1000.0f * (alpha - 1.0f);
}

/* ============================================================================
 * Goldschmidt classification
 * ============================================================================ */

geochem_goldschmidt_t geochemistry_classify_element(const char* symbol)
{
    if (!symbol) return GEOCHEM_CLASS_LITHOPHILE;

    /* Siderophile: Fe, Co, Ni, Pt, Au, Os, Ir, Ru, Rh, Pd, Re, Mo, W */
    if (strcmp(symbol, "Fe") == 0 || strcmp(symbol, "Co") == 0 ||
        strcmp(symbol, "Ni") == 0 || strcmp(symbol, "Pt") == 0 ||
        strcmp(symbol, "Au") == 0 || strcmp(symbol, "Os") == 0 ||
        strcmp(symbol, "Ir") == 0 || strcmp(symbol, "Mo") == 0 ||
        strcmp(symbol, "W") == 0)
        return GEOCHEM_CLASS_SIDEROPHILE;

    /* Chalcophile: Cu, Zn, Pb, Ag, Hg, As, Sb, Bi, Se, Te, Cd, Tl, S */
    if (strcmp(symbol, "Cu") == 0 || strcmp(symbol, "Zn") == 0 ||
        strcmp(symbol, "Pb") == 0 || strcmp(symbol, "Ag") == 0 ||
        strcmp(symbol, "Hg") == 0 || strcmp(symbol, "As") == 0 ||
        strcmp(symbol, "Sb") == 0 || strcmp(symbol, "Se") == 0 ||
        strcmp(symbol, "Cd") == 0 || strcmp(symbol, "S") == 0)
        return GEOCHEM_CLASS_CHALCOPHILE;

    /* Atmophile: H, He, C, N, O, Ne, Ar, Kr, Xe */
    if (strcmp(symbol, "H") == 0 || strcmp(symbol, "He") == 0 ||
        strcmp(symbol, "C") == 0 || strcmp(symbol, "N") == 0 ||
        strcmp(symbol, "Ne") == 0 || strcmp(symbol, "Ar") == 0 ||
        strcmp(symbol, "Kr") == 0 || strcmp(symbol, "Xe") == 0)
        return GEOCHEM_CLASS_ATMOPHILE;

    /* Default: lithophile (Si, Al, Ca, Na, K, Mg, Ti, Cr, Mn, etc.) */
    return GEOCHEM_CLASS_LITHOPHILE;
}

/* ============================================================================
 * Stokes settling: v = 2/9 * (rho_p - rho_f) * g * r^2 / mu
 * Valid for Re < 1 (laminar flow around particle)
 * ============================================================================ */

geochem_stokes_result_t geochemistry_stokes_settling(float particle_radius,
                                                      float particle_density,
                                                      float fluid_density,
                                                      float fluid_viscosity)
{
    geochem_stokes_result_t res;
    memset(&res, 0, sizeof(res));

    res.particle_radius = particle_radius;
    res.particle_density = particle_density;
    res.fluid_density = fluid_density;
    res.fluid_viscosity = fluid_viscosity;

    if (particle_radius <= 0.0f || fluid_viscosity <= 0.0f) return res;

    float drho = particle_density - fluid_density;
    if (drho <= 0.0f) {
        res.velocity = 0.0f;  /* particle floats */
        return res;
    }

    /* v = 2/9 * delta_rho * g * r^2 / mu */
    res.velocity = (2.0f / 9.0f) * drho * GEOCHEM_G * particle_radius * particle_radius
                   / fluid_viscosity;

    /* Reynolds number: Re = 2*r*v*rho_f / mu */
    res.reynolds = 2.0f * particle_radius * res.velocity * fluid_density / fluid_viscosity;

    return res;
}

/* ============================================================================
 * Carbon cycle step: update reservoir masses from fluxes
 * Simple Euler integration of box model
 * ============================================================================ */

int geochemistry_carbon_cycle_step(geochemistry_sim_t* sim, float dt_years)
{
    if (!sim || sim->num_reservoirs == 0) return -1;

    /* Compute net flux for each reservoir */
    float net_flux[GEOCHEM_MAX_RESERVOIRS];
    memset(net_flux, 0, sizeof(net_flux));

    for (uint32_t i = 0; i < sim->num_reservoirs; i++) {
        if (!sim->reservoirs[i].active) continue;

        for (uint32_t j = 0; j < sim->num_reservoirs; j++) {
            if (i == j) continue;
            float flux_out = sim->reservoirs[i].flux_to[j];
            net_flux[i] -= flux_out * dt_years;
            net_flux[j] += flux_out * dt_years;
        }
    }

    /* Add anthropogenic emissions to atmosphere */
    if (sim->config.enable_anthropogenic) {
        if (sim->num_reservoirs > GEOCHEM_RES_FOSSIL &&
            sim->num_reservoirs > GEOCHEM_RES_ATMOSPHERE) {
            float fossil_emit = GEOCHEM_FLUX_FOSSIL_BURN * dt_years;
            float land_use = GEOCHEM_FLUX_LAND_USE * dt_years;
            net_flux[GEOCHEM_RES_ATMOSPHERE] += fossil_emit + land_use;
            net_flux[GEOCHEM_RES_FOSSIL] -= fossil_emit;
            net_flux[GEOCHEM_RES_BIOSPHERE] -= land_use;
        }
    }

    /* Volcanism: lithosphere -> atmosphere */
    if (sim->num_reservoirs > GEOCHEM_RES_LITHOSPHERE) {
        float volc = GEOCHEM_FLUX_VOLCANISM * dt_years;
        net_flux[GEOCHEM_RES_ATMOSPHERE] += volc;
        net_flux[GEOCHEM_RES_LITHOSPHERE] -= volc;
    }

    /* Apply fluxes */
    for (uint32_t i = 0; i < sim->num_reservoirs; i++) {
        sim->reservoirs[i].carbon_mass += net_flux[i];
        if (sim->reservoirs[i].carbon_mass < 0.0f)
            sim->reservoirs[i].carbon_mass = 0.0f;
    }

    /* Update atmospheric CO2 ppm from reservoir mass */
    /* ~2.12 GtC per ppm (current relationship) */
    if (sim->num_reservoirs > GEOCHEM_RES_ATMOSPHERE) {
        sim->config.atmospheric_co2 = sim->reservoirs[GEOCHEM_RES_ATMOSPHERE].carbon_mass / 2.12f;
    }

    /* Update carbonate system */
    sim->carbonate.pCO2 = sim->config.atmospheric_co2 * PPM_TO_ATM;
    if (sim->num_reservoirs > GEOCHEM_RES_OCEAN_SURFACE) {
        sim->carbonate.pH = sim->reservoirs[GEOCHEM_RES_OCEAN_SURFACE].pH;
        sim->carbonate.temperature = sim->reservoirs[GEOCHEM_RES_OCEAN_SURFACE].temperature;
    }
    geochemistry_compute_carbonate(&sim->carbonate);

    /* Ocean pH feedback: more CO2 -> lower pH (ocean acidification) */
    if (sim->num_reservoirs > GEOCHEM_RES_OCEAN_SURFACE) {
        sim->reservoirs[GEOCHEM_RES_OCEAN_SURFACE].pH = sim->carbonate.pH;
    }

    return 0;
}

/* ============================================================================
 * Step — advance weathering, carbon cycle, mineral rates
 * ============================================================================ */

int geochemistry_step(geochemistry_sim_t* sim, float dt)
{
    if (!sim || !sim->initialized) return -1;
    if (dt <= 0.0f) dt = sim->config.dt;
    if (dt <= 0.0f) dt = 1.0f;

    float T = sim->config.temperature;
    if (T <= 0.0f) T = DEFAULT_TEMP;

    /* Update mineral weathering rates */
    for (uint32_t i = 0; i < sim->num_minerals; i++) {
        geochem_mineral_t* min = &sim->minerals[i];
        if (!min->active) continue;

        min->current_rate = geochemistry_weathering_rate(
            min->rate_constant_25C, min->activation_energy, T);

        /* Weathering consumes mineral mass: dm = -rate * surface_area * dt_seconds */
        float dt_seconds = dt * 365.25f * 24.0f * 3600.0f;
        float mass_loss = min->current_rate * min->surface_area * dt_seconds * 100.0f;
        /* molecular weight ~ 100 g/mol for silicates */
        min->mass -= mass_loss;
        if (min->mass < 0.0f) min->mass = 0.0f;
    }

    /* Carbon cycle box model */
    geochemistry_carbon_cycle_step(sim, dt);

    /* Update stats */
    float total_c = 0.0f;
    for (uint32_t i = 0; i < sim->num_reservoirs; i++) {
        total_c += sim->reservoirs[i].carbon_mass;
    }
    sim->stats.total_carbon = total_c;
    sim->stats.atmospheric_co2 = sim->config.atmospheric_co2;
    sim->stats.ocean_pH = sim->carbonate.pH;

    sim->stats.step_count++;
    sim->time += dt;
    return 0;
}

/* ============================================================================
 * Stats
 * ============================================================================ */

geochemistry_stats_t geochemistry_get_stats(const geochemistry_sim_t* sim)
{
    geochemistry_stats_t zero;
    memset(&zero, 0, sizeof(zero));
    if (!sim) return zero;
    return sim->stats;
}
