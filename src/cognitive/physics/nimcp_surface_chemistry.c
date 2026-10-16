/**
 * @file nimcp_surface_chemistry.c
 * @brief Surface Chemistry — adsorption, catalysis, corrosion, electrochemistry
 *
 * WHAT: Langmuir/BET adsorption, Arrhenius kinetics, Butler-Volmer electrochemistry
 * WHY:  Reasoning about rust, batteries, catalysts, soap, cooking, paint
 * HOW:  Rate equations + mass balance at each timestep
 */

#include "cognitive/physics/nimcp_surface_chemistry.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "SURFACE_CHEMISTRY"

/* ============================================================================
 * Public API — Creation / Destruction
 * ============================================================================ */

schem_config_t surface_chemistry_default_config(void) {
    return (schem_config_t){
        .temperature = 298.15f,
        .pressure = 101325.0f,
        .dt = 0.01f,
        .enable_catalysis = true,
        .enable_corrosion = true,
        .enable_electrochemistry = true,
        .enable_diffusion = true,
    };
}

surface_chemistry_sim_t* surface_chemistry_create(const schem_config_t* config) {
    schem_config_t cfg = config ? *config : surface_chemistry_default_config();
    surface_chemistry_sim_t* sim = nimcp_calloc(1, sizeof(*sim));
    if (!sim) return NULL;
    sim->config = cfg;
    sim->initialized = true;
    LOG_INFO(LOG_TAG, "Surface chemistry created: T=%.1fK, catalysis=%s, electrochem=%s",
             cfg.temperature,
             cfg.enable_catalysis ? "yes" : "no",
             cfg.enable_electrochemistry ? "yes" : "no");
    return sim;
}

void surface_chemistry_destroy(surface_chemistry_sim_t* sim) {
    if (!sim) return;
    nimcp_free(sim);
}

/* ============================================================================
 * Entity Management
 * ============================================================================ */

uint32_t surface_chemistry_add_adsorbate(surface_chemistry_sim_t* sim,
                                          const schem_adsorbate_t* ads) {
    if (!sim || !ads || sim->num_adsorbates >= SCHEM_MAX_ADSORBATES) return UINT32_MAX;
    uint32_t id = sim->num_adsorbates;
    sim->adsorbates[id] = *ads;
    sim->adsorbates[id].id = id;
    sim->adsorbates[id].active = true;
    sim->num_adsorbates = id + 1;
    return id;
}

uint32_t surface_chemistry_add_surface(surface_chemistry_sim_t* sim,
                                        const schem_surface_t* surf) {
    if (!sim || !surf || sim->num_surfaces >= SCHEM_MAX_SURFACES) return UINT32_MAX;
    uint32_t id = sim->num_surfaces;
    sim->surfaces[id] = *surf;
    sim->surfaces[id].id = id;
    sim->surfaces[id].active = true;
    sim->surfaces[id].activity = 1.0f;
    sim->num_surfaces = id + 1;
    return id;
}

uint32_t surface_chemistry_add_reaction(surface_chemistry_sim_t* sim,
                                         const schem_reaction_t* rxn) {
    if (!sim || !rxn || sim->num_reactions >= SCHEM_MAX_REACTIONS) return UINT32_MAX;
    uint32_t id = sim->num_reactions;
    sim->reactions[id] = *rxn;
    sim->reactions[id].id = id;
    sim->reactions[id].active = true;
    sim->num_reactions = id + 1;
    return id;
}

uint32_t surface_chemistry_add_electrode(surface_chemistry_sim_t* sim,
                                          const schem_electrode_t* electrode) {
    if (!sim || !electrode || sim->num_electrodes >= SCHEM_MAX_ELECTRODES) return UINT32_MAX;
    uint32_t id = sim->num_electrodes;
    sim->electrodes[id] = *electrode;
    sim->electrodes[id].id = id;
    sim->electrodes[id].active = true;
    sim->num_electrodes = id + 1;
    return id;
}

/* ============================================================================
 * Isotherm Functions
 * ============================================================================ */

float surface_chemistry_langmuir(float K, float pressure) {
    /* θ = KP / (1 + KP) */
    float KP = K * pressure;
    return KP / (1.0f + KP);
}

float surface_chemistry_bet(float Vm, float c, float P, float Ps) {
    /* BET: V = Vm · c · P / [(Ps - P)(1 + (c-1)·P/Ps)] */
    if (Ps <= 0 || P >= Ps) return Vm;  /* saturated */
    float x = P / Ps;
    float denom = (1.0f - x) * (1.0f + (c - 1.0f) * x);
    if (fabsf(denom) < 1e-12f) return Vm;
    return Vm * c * x / denom;
}

float surface_chemistry_freundlich(float K, float P, float n) {
    /* θ = K · P^(1/n) */
    if (P <= 0 || n <= 0) return 0;
    return K * powf(P, 1.0f / n);
}

/* ============================================================================
 * Kinetics
 * ============================================================================ */

float surface_chemistry_arrhenius(float A, float Ea, float T) {
    /* k = A · exp(-Ea / (R·T)) — Ea in kJ/mol */
    if (T <= 0) return 0;
    return A * expf(-Ea * 1000.0f / (SCHEM_GAS_CONSTANT * T));
}

float surface_chemistry_tof(float rate, float site_density) {
    if (site_density <= 0) return 0;
    return rate / site_density;
}

/* ============================================================================
 * Electrochemistry
 * ============================================================================ */

float surface_chemistry_nernst(float E0, float T, uint32_t n, float Q) {
    /* E = E° - (RT / nF) · ln(Q) */
    if (n == 0 || Q <= 0) return E0;
    return E0 - (SCHEM_GAS_CONSTANT * T / ((float)n * SCHEM_FARADAY)) * logf(Q);
}

float surface_chemistry_butler_volmer(float j0, float alpha, uint32_t n,
                                       float eta, float T) {
    /* j = j₀ [ exp(α·n·F·η / RT) - exp(-(1-α)·n·F·η / RT) ] */
    if (T <= 0) return 0;
    float nF_RT = (float)n * SCHEM_FARADAY / (SCHEM_GAS_CONSTANT * T);
    float anodic = expf(alpha * nF_RT * eta);
    float cathodic = expf(-(1.0f - alpha) * nF_RT * eta);
    /* Clamp to prevent overflow */
    if (anodic > 1e10f) anodic = 1e10f;
    if (cathodic > 1e10f) cathodic = 1e10f;
    return j0 * (anodic - cathodic);
}

float surface_chemistry_tafel_slope(float alpha, uint32_t n, float T) {
    if (alpha <= 0 || n == 0 || T <= 0) return 0;
    return 2.303f * SCHEM_GAS_CONSTANT * T / (alpha * (float)n * SCHEM_FARADAY);
}

/* ============================================================================
 * Corrosion
 * ============================================================================ */

float surface_chemistry_corrosion_rate(float current_density, float molar_mass,
                                        uint32_t electrons, float density) {
    /* CR = j · M / (n · F · ρ) in m/s */
    if (electrons == 0 || density <= 0) return 0;
    return fabsf(current_density) * molar_mass /
           ((float)electrons * SCHEM_FARADAY * density * 1000.0f);  /* g→kg */
}

float surface_chemistry_pilling_bedworth(float oxide_molar_volume,
                                          float metal_molar_volume,
                                          float stoich_ratio) {
    if (metal_molar_volume <= 0) return 0;
    return oxide_molar_volume / (stoich_ratio * metal_molar_volume);
}

/* ============================================================================
 * Simulation Step
 * ============================================================================ */

int surface_chemistry_step(surface_chemistry_sim_t* sim, float dt) {
    if (!sim || !sim->initialized) return -1;
    if (dt <= 0) dt = sim->config.dt;
    float T = sim->config.temperature;

    /* 1. Update adsorption equilibria on all surfaces */
    for (uint32_t s = 0; s < sim->num_surfaces; s++) {
        schem_surface_t* surf = &sim->surfaces[s];
        if (!surf->active) continue;

        for (uint32_t a = 0; a < sim->num_adsorbates; a++) {
            if (!sim->adsorbates[a].active) continue;
            float K = surf->K_langmuir[a];
            if (K <= 0) continue;

            /* Compute equilibrium coverage */
            float P = sim->adsorbates[a].partial_pressure;
            float theta_eq = surface_chemistry_langmuir(K, P);

            /* Approach equilibrium with rate proportional to deviation */
            float k_ads = surface_chemistry_arrhenius(1e6f, sim->adsorbates[a].adsorption_energy, T);
            float d_theta = (theta_eq - surf->coverage[a]) * k_ads * dt;

            /* Clamp change to available sites */
            float total_coverage = 0;
            for (uint32_t j = 0; j < sim->num_adsorbates; j++)
                total_coverage += surf->coverage[j];

            if (d_theta > 0 && total_coverage + d_theta > 1.0f)
                d_theta = (1.0f - total_coverage) * 0.5f;

            surf->coverage[a] += d_theta;
            if (surf->coverage[a] < 0) surf->coverage[a] = 0;
            if (surf->coverage[a] > 1.0f) surf->coverage[a] = 1.0f;

            if (d_theta > 0) sim->stats.total_adsorbed += fabsf(d_theta) * surf->site_density * surf->area;
            else sim->stats.total_desorbed += fabsf(d_theta) * surf->site_density * surf->area;
        }
    }

    /* 2. Surface reactions (catalysis) */
    if (sim->config.enable_catalysis) {
        for (uint32_t r = 0; r < sim->num_reactions; r++) {
            schem_reaction_t* rxn = &sim->reactions[r];
            if (!rxn->active) continue;
            if (rxn->surface_id >= sim->num_surfaces) continue;
            schem_surface_t* surf = &sim->surfaces[rxn->surface_id];
            if (!surf->active) continue;

            /* Compute rate from Arrhenius + coverage dependence */
            float k = surface_chemistry_arrhenius(rxn->pre_exponential,
                                                    rxn->activation_energy, T);
            k *= surf->activity * (1.0f - surf->poison_level);

            /* Rate = k × Π(θᵢ^nᵢ) for Langmuir-Hinshelwood */
            float rate = k;
            for (uint32_t i = 0; i < rxn->num_reactants; i++) {
                uint32_t ads_id = rxn->reactant_ids[i];
                if (ads_id < SCHEM_MAX_ADSORBATES) {
                    float theta = surf->coverage[ads_id];
                    float order = rxn->coverage_order[i];
                    if (order == 0) order = 1.0f;
                    rate *= powf(theta + 1e-10f, order);
                }
            }

            rxn->rate = rate;
            if (rate > sim->stats.max_reaction_rate)
                sim->stats.max_reaction_rate = rate;

            /* Consume reactants, produce products */
            float turnover = rate * surf->area * dt;
            for (uint32_t i = 0; i < rxn->num_reactants; i++) {
                uint32_t ads_id = rxn->reactant_ids[i];
                if (ads_id < SCHEM_MAX_ADSORBATES) {
                    surf->coverage[ads_id] -= rxn->reactant_coeffs[i] * turnover / (surf->site_density * surf->area + 1e-12f);
                    if (surf->coverage[ads_id] < 0) surf->coverage[ads_id] = 0;
                }
            }
            for (uint32_t i = 0; i < rxn->num_products; i++) {
                uint32_t ads_id = rxn->product_ids[i];
                if (ads_id < SCHEM_MAX_ADSORBATES) {
                    /* Products desorb (go back to gas phase) */
                    sim->adsorbates[ads_id].partial_pressure +=
                        rxn->product_coeffs[i] * turnover * SCHEM_GAS_CONSTANT * T /
                        (sim->config.pressure + 1e-10f);
                }
            }
            sim->stats.total_reacted += turnover;
        }
    }

    /* 3. Electrochemistry */
    if (sim->config.enable_electrochemistry) {
        for (uint32_t e = 0; e < sim->num_electrodes; e++) {
            schem_electrode_t* elec = &sim->electrodes[e];
            if (!elec->active) continue;

            /* Butler-Volmer current density */
            elec->current_density = surface_chemistry_butler_volmer(
                elec->exchange_current_density,
                elec->transfer_coefficient,
                elec->electrons_transferred,
                elec->overpotential, T);

            /* Accumulate charge: Q += j × A × dt */
            if (elec->surface_id < sim->num_surfaces) {
                float area = sim->surfaces[elec->surface_id].area;
                float dQ = elec->current_density * area * dt;
                elec->charge_transferred += dQ;
                sim->stats.total_charge += fabsf(dQ);
            }

            /* Update electrode potential via Nernst */
            /* Simplified: assume constant reaction quotient for now */
        }
    }

    /* 4. Corrosion (metal dissolution) */
    if (sim->config.enable_corrosion) {
        for (uint32_t r = 0; r < sim->num_reactions; r++) {
            schem_reaction_t* rxn = &sim->reactions[r];
            if (!rxn->active || rxn->mechanism != SCHEM_RXN_CORROSION) continue;
            /* Corrosion already handled as a reaction, but track stats */
            sim->stats.total_corroded += rxn->rate * dt;
        }
    }

    /* Update stats */
    sim->time += dt;
    sim->stats.step_count++;

    float max_cov = 0;
    for (uint32_t s = 0; s < sim->num_surfaces; s++) {
        if (!sim->surfaces[s].active) continue;
        for (uint32_t a = 0; a < sim->num_adsorbates; a++) {
            if (sim->surfaces[s].coverage[a] > max_cov)
                max_cov = sim->surfaces[s].coverage[a];
        }
    }
    sim->stats.max_coverage = max_cov;

    return 0;
}

/* ============================================================================
 * Preloaded Systems
 * ============================================================================ */

void surface_chemistry_load_common_adsorbates(surface_chemistry_sim_t* sim) {
    if (!sim) return;
    struct { const char* name; float mm; float pp; float D; float Eads; } common[] = {
        {"O2",   32.0f,  21278.0f,  2.1e-5f,  -40.0f},
        {"CO",   28.0f,  0.04f,     2.0e-5f,  -110.0f},
        {"H2O",  18.0f,  2338.0f,   2.4e-5f,  -50.0f},
        {"N2",   28.0f,  79111.0f,  1.9e-5f,  -20.0f},
        {"CO2",  44.0f,  40.0f,     1.6e-5f,  -30.0f},
        {"H2",   2.0f,   0.05f,     6.1e-5f,  -45.0f},
        {"CH4",  16.0f,  0.2f,      2.2e-5f,  -15.0f},
        {"NH3",  17.0f,  0.01f,     2.2e-5f,  -80.0f},
    };
    for (uint32_t i = 0; i < sizeof(common) / sizeof(common[0]); i++) {
        schem_adsorbate_t ads = {0};
        strncpy(ads.name, common[i].name, sizeof(ads.name) - 1);
        ads.molar_mass = common[i].mm;
        ads.partial_pressure = common[i].pp;
        ads.diffusion_coefficient = common[i].D;
        ads.adsorption_energy = common[i].Eads;
        surface_chemistry_add_adsorbate(sim, &ads);
    }
}

void surface_chemistry_load_pt_catalyst(surface_chemistry_sim_t* sim) {
    if (!sim) return;
    surface_chemistry_load_common_adsorbates(sim);

    schem_surface_t pt = {0};
    strncpy(pt.name, "Pt(111)", sizeof(pt.name) - 1);
    pt.type = SCHEM_SURF_METAL;
    pt.area = 1e-4f;               /* 1 cm² */
    pt.site_density = 1.5e19f;     /* sites/m² for Pt(111) */
    pt.temperature = 500.0f;       /* typical catalysis temperature */
    pt.K_langmuir[0] = 1e-3f;     /* O2 */
    pt.K_langmuir[1] = 5e-2f;     /* CO (binds strongly to Pt) */
    pt.K_langmuir[5] = 2e-3f;     /* H2 */
    uint32_t pt_id = surface_chemistry_add_surface(sim, &pt);

    /* CO oxidation: CO(ads) + O(ads) → CO2(gas) */
    schem_reaction_t co_ox = {0};
    strncpy(co_ox.name, "CO_oxidation", sizeof(co_ox.name) - 1);
    co_ox.mechanism = SCHEM_RXN_LANGMUIR_HINSHELWOOD;
    co_ox.surface_id = pt_id;
    co_ox.reactant_ids[0] = 1;     /* CO */
    co_ox.reactant_coeffs[0] = 1;
    co_ox.reactant_ids[1] = 0;     /* O2 (→ 2O) */
    co_ox.reactant_coeffs[1] = 0.5f;
    co_ox.num_reactants = 2;
    co_ox.product_ids[0] = 4;      /* CO2 */
    co_ox.product_coeffs[0] = 1;
    co_ox.num_products = 1;
    co_ox.pre_exponential = 1e13f;
    co_ox.activation_energy = 60.0f; /* kJ/mol */
    co_ox.coverage_order[0] = 1.0f;
    co_ox.coverage_order[1] = 0.5f;
    surface_chemistry_add_reaction(sim, &co_ox);
}

void surface_chemistry_load_iron_corrosion(surface_chemistry_sim_t* sim) {
    if (!sim) return;
    surface_chemistry_load_common_adsorbates(sim);

    schem_surface_t fe = {0};
    strncpy(fe.name, "Fe_surface", sizeof(fe.name) - 1);
    fe.type = SCHEM_SURF_METAL;
    fe.area = 1e-3f;               /* 10 cm² */
    fe.site_density = 1.2e19f;
    fe.temperature = 298.15f;
    fe.K_langmuir[0] = 1e-4f;     /* O2 */
    fe.K_langmuir[2] = 5e-4f;     /* H2O */
    uint32_t fe_id = surface_chemistry_add_surface(sim, &fe);

    /* Fe corrosion: 4Fe + 3O2 + 6H2O → 4Fe(OH)3 (rust) */
    schem_reaction_t corr = {0};
    strncpy(corr.name, "Fe_rusting", sizeof(corr.name) - 1);
    corr.mechanism = SCHEM_RXN_CORROSION;
    corr.surface_id = fe_id;
    corr.reactant_ids[0] = 0;     /* O2 */
    corr.reactant_coeffs[0] = 0.75f;
    corr.reactant_ids[1] = 2;     /* H2O */
    corr.reactant_coeffs[1] = 1.5f;
    corr.num_reactants = 2;
    corr.num_products = 0;         /* rust stays on surface */
    corr.pre_exponential = 1e8f;
    corr.activation_energy = 40.0f;
    corr.coverage_order[0] = 1.0f;
    corr.coverage_order[1] = 1.0f;
    surface_chemistry_add_reaction(sim, &corr);
}

void surface_chemistry_load_daniell_cell(surface_chemistry_sim_t* sim) {
    if (!sim) return;

    /* Zinc anode surface */
    schem_surface_t zn = {0};
    strncpy(zn.name, "Zn_anode", sizeof(zn.name) - 1);
    zn.type = SCHEM_SURF_METAL;
    zn.area = 1e-3f;
    zn.site_density = 1.5e19f;
    zn.temperature = 298.15f;
    uint32_t zn_id = surface_chemistry_add_surface(sim, &zn);

    /* Copper cathode surface */
    schem_surface_t cu = {0};
    strncpy(cu.name, "Cu_cathode", sizeof(cu.name) - 1);
    cu.type = SCHEM_SURF_METAL;
    cu.area = 1e-3f;
    cu.site_density = 1.5e19f;
    cu.temperature = 298.15f;
    uint32_t cu_id = surface_chemistry_add_surface(sim, &cu);

    /* Zn anode: Zn → Zn²⁺ + 2e⁻ (E° = -0.76V) */
    schem_electrode_t anode = {0};
    strncpy(anode.name, "Zn_anode", sizeof(anode.name) - 1);
    anode.surface_id = zn_id;
    anode.standard_potential = -0.76f;
    anode.current_potential = -0.76f;
    anode.exchange_current_density = 0.1f;
    anode.transfer_coefficient = 0.5f;
    anode.electrons_transferred = 2;
    anode.overpotential = 0.05f;   /* slight driving force */
    surface_chemistry_add_electrode(sim, &anode);

    /* Cu cathode: Cu²⁺ + 2e⁻ → Cu (E° = +0.34V) */
    schem_electrode_t cathode = {0};
    strncpy(cathode.name, "Cu_cathode", sizeof(cathode.name) - 1);
    cathode.surface_id = cu_id;
    cathode.standard_potential = 0.34f;
    cathode.current_potential = 0.34f;
    cathode.exchange_current_density = 0.05f;
    cathode.transfer_coefficient = 0.5f;
    cathode.electrons_transferred = 2;
    cathode.overpotential = -0.05f;
    surface_chemistry_add_electrode(sim, &cathode);
}

schem_stats_t surface_chemistry_get_stats(const surface_chemistry_sim_t* sim) {
    if (!sim) return (schem_stats_t){0};
    return sim->stats;
}
