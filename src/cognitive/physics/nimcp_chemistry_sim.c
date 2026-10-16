/**
 * @file nimcp_chemistry_sim.c
 * @brief Chemistry Simulator implementation
 *
 * Implements mass-action kinetics, conservation checking, phase transitions,
 * pH computation, and common element/substance/reaction loading.
 */

#include "cognitive/physics/nimcp_chemistry_sim.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>
#include <math.h>
#include <float.h>

/* ============================================================================
 * Internal constants
 * ============================================================================ */

/** Gas constant in kJ/(mol*K) */
#define R_GAS_CONSTANT      0.008314f

/** Minimum concentration threshold (below this, treat as zero) */
#define CONC_EPSILON         1e-15f

/** Conservation tolerance (relative) */
#define CONSERVATION_TOL     1e-4f

/** Maximum dt to prevent instability */
#define MAX_DT               1.0f

/** Absolute zero */
#define ABS_ZERO             0.0f

/** Standard temperature (25 C) */
#define STANDARD_TEMP        298.15f

/* ============================================================================
 * Internal helpers — forward declarations
 * ============================================================================ */

static float compute_reaction_rate(const chemistry_sim_t* sim,
                                   const chem_reaction_t* rxn,
                                   bool forward);

static void  update_phases(chemistry_sim_t* sim);
static void  update_ph(chemistry_sim_t* sim);
static void  snapshot_initial_atoms(chemistry_sim_t* sim);
static float compute_total_mass(const chemistry_sim_t* sim);
static void  compute_atom_counts_internal(const chemistry_sim_t* sim,
                                          float* counts, uint32_t max_elem);

static uint32_t find_element_by_symbol(const chemistry_sim_t* sim,
                                       const char* symbol);
static uint32_t find_substance_by_name(const chemistry_sim_t* sim,
                                       const char* name);

/* ============================================================================
 * Default config
 * ============================================================================ */

chem_config_t chemistry_sim_default_config(void)
{
    chem_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.default_temperature = STANDARD_TEMP;
    cfg.default_pressure    = 1.0f;
    cfg.default_volume      = 1.0f;
    cfg.dt                  = 0.001f;
    return cfg;
}

/* ============================================================================
 * Create / Destroy
 * ============================================================================ */

chemistry_sim_t* chemistry_sim_create(const chem_config_t* config)
{
    chemistry_sim_t* sim = (chemistry_sim_t*)nimcp_calloc(1, sizeof(chemistry_sim_t));
    if (!sim) {
        LOG_ERROR("chemistry_sim_create: allocation failed");
        return NULL;
    }

    if (config) {
        sim->config = *config;
    } else {
        sim->config = chemistry_sim_default_config();
    }

    /* Initialize state with defaults from config */
    sim->state.temperature = sim->config.default_temperature;
    sim->state.pressure    = sim->config.default_pressure;
    sim->state.volume      = sim->config.default_volume;
    sim->state.pH          = 7.0f;  /* neutral */
    sim->state.total_energy = 0.0f;

    sim->num_elements   = 0;
    sim->num_substances = 0;
    sim->num_reactions  = 0;
    sim->step_count     = 0;
    sim->reactions_fired = 0;
    sim->mass_drift     = 0.0f;
    sim->initialized    = true;

    LOG_INFO("Chemistry simulator created (T=%.1fK, P=%.2fatm, V=%.2fL)",
             sim->state.temperature, sim->state.pressure, sim->state.volume);

    return sim;
}

void chemistry_sim_destroy(chemistry_sim_t* sim)
{
    if (!sim) return;

    LOG_INFO("Chemistry simulator destroyed after %lu steps, %lu reactions fired",
             (unsigned long)sim->step_count, (unsigned long)sim->reactions_fired);

    sim->initialized = false;
    nimcp_free(sim);
}

/* ============================================================================
 * Add element / substance / reaction
 * ============================================================================ */

uint32_t chemistry_sim_add_element(chemistry_sim_t* sim, const char* symbol,
                                   float atomic_mass, uint32_t atomic_number)
{
    if (!sim || !symbol) return UINT32_MAX;
    if (sim->num_elements >= CHEM_MAX_ELEMENTS) {
        LOG_WARN("chemistry_sim: element table full (%u/%u)",
                 sim->num_elements, (uint32_t)CHEM_MAX_ELEMENTS);
        return UINT32_MAX;
    }

    /* Check for duplicate */
    uint32_t existing = find_element_by_symbol(sim, symbol);
    if (existing != UINT32_MAX) {
        return existing;
    }

    uint32_t id = sim->num_elements;
    chem_element_t* e = &sim->elements[id];
    memset(e, 0, sizeof(*e));
    strncpy(e->symbol, symbol, sizeof(e->symbol) - 1);
    e->atomic_mass   = atomic_mass;
    e->atomic_number = atomic_number;
    e->active        = true;
    sim->num_elements++;

    return id;
}

uint32_t chemistry_sim_add_substance(chemistry_sim_t* sim,
                                     const chem_substance_t* sub)
{
    if (!sim || !sub) return UINT32_MAX;
    if (sim->num_substances >= CHEM_MAX_SUBSTANCES) {
        LOG_WARN("chemistry_sim: substance table full (%u/%u)",
                 sim->num_substances, (uint32_t)CHEM_MAX_SUBSTANCES);
        return UINT32_MAX;
    }

    uint32_t id = sim->num_substances;
    sim->substances[id] = *sub;
    sim->substances[id].id = id;
    sim->substances[id].active = true;
    sim->num_substances++;

    /* Initialize concentration and phase */
    sim->state.concentrations[id] = 0.0f;
    sim->state.amounts[id]        = 0.0f;
    sim->state.phases[id]         = sub->default_phase;

    return id;
}

uint32_t chemistry_sim_add_reaction(chemistry_sim_t* sim,
                                    const chem_reaction_t* rxn)
{
    if (!sim || !rxn) return UINT32_MAX;
    if (sim->num_reactions >= CHEM_MAX_REACTIONS) {
        LOG_WARN("chemistry_sim: reaction table full (%u/%u)",
                 sim->num_reactions, (uint32_t)CHEM_MAX_REACTIONS);
        return UINT32_MAX;
    }

    uint32_t id = sim->num_reactions;
    sim->reactions[id] = *rxn;
    sim->reactions[id].id = id;
    sim->reactions[id].active = true;
    sim->num_reactions++;

    return id;
}

/* ============================================================================
 * Set concentration
 * ============================================================================ */

void chemistry_sim_set_concentration(chemistry_sim_t* sim,
                                     uint32_t substance_id, float conc)
{
    if (!sim || substance_id >= sim->num_substances) return;
    if (conc < 0.0f) conc = 0.0f;

    sim->state.concentrations[substance_id] = conc;
    sim->state.amounts[substance_id] = conc * sim->state.volume;

    /* Update phases based on current temperature */
    update_phases(sim);
    update_ph(sim);

    /* Re-snapshot initial atom counts for conservation tracking */
    snapshot_initial_atoms(sim);
}

/* ============================================================================
 * Step
 * ============================================================================ */

int chemistry_sim_step(chemistry_sim_t* sim, float dt)
{
    if (!sim || !sim->initialized) return -1;
    if (dt <= 0.0f) dt = sim->config.dt;
    if (dt > MAX_DT) dt = MAX_DT;

    /* Snapshot initial atoms on first step */
    if (sim->step_count == 0) {
        snapshot_initial_atoms(sim);
    }

    /* Apply each active reaction using forward Euler */
    float delta_conc[CHEM_MAX_SUBSTANCES];
    memset(delta_conc, 0, sizeof(delta_conc));

    for (uint32_t r = 0; r < sim->num_reactions; r++) {
        chem_reaction_t* rxn = &sim->reactions[r];
        if (!rxn->active) continue;

        /* Forward rate: rate = k_fwd * product([reactant]^coeff) */
        float fwd_rate = compute_reaction_rate(sim, rxn, true);

        /* Reverse rate (if reversible) */
        float rev_rate = 0.0f;
        if (rxn->reversible && rxn->reverse_rate > 0.0f) {
            rev_rate = compute_reaction_rate(sim, rxn, false);
        }

        float net_rate = fwd_rate - rev_rate;
        if (fabsf(net_rate) < CONC_EPSILON) continue;

        /* Check that all reactants have sufficient concentration for forward */
        bool can_proceed = true;
        if (net_rate > 0.0f) {
            for (uint32_t i = 0; i < rxn->num_reactants; i++) {
                uint32_t sid = rxn->reactant_ids[i];
                if (sid >= sim->num_substances) { can_proceed = false; break; }
                if (sim->state.concentrations[sid] < CONC_EPSILON) {
                    can_proceed = false;
                    break;
                }
            }
        } else {
            /* Net reverse: check products have sufficient concentration */
            for (uint32_t i = 0; i < rxn->num_products; i++) {
                uint32_t sid = rxn->product_ids[i];
                if (sid >= sim->num_substances) { can_proceed = false; break; }
                if (sim->state.concentrations[sid] < CONC_EPSILON) {
                    can_proceed = false;
                    break;
                }
            }
        }
        if (!can_proceed) continue;

        float extent = net_rate * dt;

        /* Limit extent so no concentration goes negative */
        if (extent > 0.0f) {
            for (uint32_t i = 0; i < rxn->num_reactants; i++) {
                uint32_t sid = rxn->reactant_ids[i];
                if (sid >= sim->num_substances) continue;
                float coeff = (float)rxn->reactant_coeffs[i];
                if (coeff > 0.0f) {
                    float max_extent = sim->state.concentrations[sid] / coeff;
                    if (extent > max_extent) extent = max_extent;
                }
            }
        } else {
            float abs_ext = -extent;
            for (uint32_t i = 0; i < rxn->num_products; i++) {
                uint32_t sid = rxn->product_ids[i];
                if (sid >= sim->num_substances) continue;
                float coeff = (float)rxn->product_coeffs[i];
                if (coeff > 0.0f) {
                    float max_ext = sim->state.concentrations[sid] / coeff;
                    if (abs_ext > max_ext) abs_ext = max_ext;
                }
            }
            extent = -abs_ext;
        }

        /* Apply stoichiometry: reactants consumed, products formed */
        for (uint32_t i = 0; i < rxn->num_reactants; i++) {
            uint32_t sid = rxn->reactant_ids[i];
            if (sid >= sim->num_substances) continue;
            delta_conc[sid] -= (float)rxn->reactant_coeffs[i] * extent;
        }
        for (uint32_t i = 0; i < rxn->num_products; i++) {
            uint32_t sid = rxn->product_ids[i];
            if (sid >= sim->num_substances) continue;
            delta_conc[sid] += (float)rxn->product_coeffs[i] * extent;
        }

        /* Energy bookkeeping */
        sim->state.total_energy += rxn->enthalpy_change * extent * sim->state.volume;

        if (fabsf(extent) > CONC_EPSILON) {
            sim->reactions_fired++;
        }
    }

    /* Apply concentration deltas */
    for (uint32_t s = 0; s < sim->num_substances; s++) {
        sim->state.concentrations[s] += delta_conc[s];
        /* Clamp to non-negative */
        if (sim->state.concentrations[s] < 0.0f) {
            sim->state.concentrations[s] = 0.0f;
        }
        sim->state.amounts[s] = sim->state.concentrations[s] * sim->state.volume;
    }

    /* Update phases based on temperature */
    update_phases(sim);

    /* Update pH */
    update_ph(sim);

    /* Conservation check — compute mass drift */
    float current_mass = compute_total_mass(sim);
    if (sim->initial_total_mass > CONC_EPSILON) {
        sim->mass_drift = fabsf(current_mass - sim->initial_total_mass)
                          / sim->initial_total_mass;
    }
    if (sim->mass_drift > CONSERVATION_TOL) {
        LOG_WARN("chemistry_sim: mass drift %.6f exceeds tolerance (step %lu)",
                 sim->mass_drift, (unsigned long)sim->step_count);
    }

    sim->step_count++;
    return 0;
}

/* ============================================================================
 * Violation check
 * ============================================================================ */

chem_violation_t chemistry_sim_check_violations(const chemistry_sim_t* sim,
                                                const chem_state_t* predicted)
{
    if (!sim || !predicted) return CHEM_VIOLATION_NONE;

    uint32_t violations = CHEM_VIOLATION_NONE;

    /* Check negative concentrations */
    for (uint32_t s = 0; s < sim->num_substances; s++) {
        if (predicted->concentrations[s] < -CONC_EPSILON) {
            violations |= CHEM_VIOLATION_NEGATIVE_CONC;
            break;
        }
    }

    /* Check mass conservation */
    float pred_mass = 0.0f;
    for (uint32_t s = 0; s < sim->num_substances; s++) {
        if (!sim->substances[s].active) continue;
        pred_mass += predicted->concentrations[s] * predicted->volume
                     * sim->substances[s].molar_mass;
    }
    if (sim->initial_total_mass > CONC_EPSILON) {
        float drift = fabsf(pred_mass - sim->initial_total_mass)
                      / sim->initial_total_mass;
        if (drift > CONSERVATION_TOL) {
            violations |= CHEM_VIOLATION_MASS_NOT_CONSERVED;
        }
    }

    /* Check atom conservation */
    float current_atoms[CHEM_MAX_ELEMENTS];
    memset(current_atoms, 0, sizeof(current_atoms));
    for (uint32_t s = 0; s < sim->num_substances; s++) {
        if (!sim->substances[s].active) continue;
        float moles = predicted->concentrations[s] * predicted->volume;
        for (uint32_t e = 0; e < sim->substances[s].num_elements; e++) {
            uint32_t eid = sim->substances[s].element_ids[e];
            if (eid < CHEM_MAX_ELEMENTS) {
                current_atoms[eid] += moles * (float)sim->substances[s].element_counts[e];
            }
        }
    }
    for (uint32_t e = 0; e < sim->num_elements; e++) {
        if (sim->initial_atom_counts[e] > CONC_EPSILON) {
            float drift = fabsf(current_atoms[e] - sim->initial_atom_counts[e])
                          / sim->initial_atom_counts[e];
            if (drift > CONSERVATION_TOL) {
                violations |= CHEM_VIOLATION_ATOMS_NOT_BALANCED;
                break;
            }
        }
    }

    /* Check phase vs temperature consistency */
    for (uint32_t s = 0; s < sim->num_substances; s++) {
        if (!sim->substances[s].active) continue;
        if (predicted->concentrations[s] < CONC_EPSILON) continue;

        const chem_substance_t* sub = &sim->substances[s];
        chem_phase_t expected_phase;
        float T = predicted->temperature;

        if (sub->melting_point > 0.0f && sub->boiling_point > 0.0f) {
            if (T < sub->melting_point) {
                expected_phase = CHEM_PHASE_SOLID;
            } else if (T < sub->boiling_point) {
                expected_phase = CHEM_PHASE_LIQUID;
            } else {
                expected_phase = CHEM_PHASE_GAS;
            }

            /* Aqueous is always OK (dissolved) */
            if (predicted->phases[s] != expected_phase &&
                predicted->phases[s] != CHEM_PHASE_AQUEOUS) {
                violations |= CHEM_VIOLATION_IMPOSSIBLE_PHASE;
                break;
            }
        }
    }

    return (chem_violation_t)violations;
}

/* ============================================================================
 * Total mass
 * ============================================================================ */

float chemistry_sim_total_mass(const chemistry_sim_t* sim)
{
    if (!sim) return 0.0f;
    return compute_total_mass(sim);
}

/* ============================================================================
 * Atom counts
 * ============================================================================ */

void chemistry_sim_atom_counts(const chemistry_sim_t* sim,
                               float* counts, uint32_t max_elements)
{
    if (!sim || !counts) return;
    compute_atom_counts_internal(sim, counts, max_elements);
}

/* ============================================================================
 * pH
 * ============================================================================ */

float chemistry_sim_get_ph(const chemistry_sim_t* sim)
{
    if (!sim) return 7.0f;
    return sim->state.pH;
}

/* ============================================================================
 * Temperature / phase transitions
 * ============================================================================ */

void chemistry_sim_set_temperature(chemistry_sim_t* sim, float kelvin)
{
    if (!sim) return;
    if (kelvin < ABS_ZERO) kelvin = ABS_ZERO;

    sim->state.temperature = kelvin;
    update_phases(sim);
    update_ph(sim);

    LOG_INFO("Chemistry sim temperature set to %.1fK (%.1f°C)",
             kelvin, kelvin - 273.15f);
}

/* ============================================================================
 * Load common elements
 * ============================================================================ */

void chemistry_sim_load_common_elements(chemistry_sim_t* sim)
{
    if (!sim) return;

    /* symbol, atomic_mass, atomic_number */
    chemistry_sim_add_element(sim, "H",  1.008f,  1);
    chemistry_sim_add_element(sim, "C",  12.011f, 6);
    chemistry_sim_add_element(sim, "N",  14.007f, 7);
    chemistry_sim_add_element(sim, "O",  15.999f, 8);
    chemistry_sim_add_element(sim, "Na", 22.990f, 11);
    chemistry_sim_add_element(sim, "Cl", 35.453f, 17);
    chemistry_sim_add_element(sim, "Fe", 55.845f, 26);
    chemistry_sim_add_element(sim, "Ca", 40.078f, 20);
    chemistry_sim_add_element(sim, "K",  39.098f, 19);
    chemistry_sim_add_element(sim, "P",  30.974f, 15);
    chemistry_sim_add_element(sim, "S",  32.065f, 16);

    LOG_INFO("Loaded %u common elements", sim->num_elements);
}

/* ============================================================================
 * Helper: build a substance struct
 * ============================================================================ */

/**
 * @brief Build a substance with elemental composition.
 *
 * @param sim           Simulator (for element lookup)
 * @param name          Human name
 * @param formula       Chemical formula string
 * @param phase         Default phase
 * @param molar_mass    g/mol
 * @param mp            Melting point (K)
 * @param bp            Boiling point (K)
 * @param enthalpy      Standard formation enthalpy (kJ/mol)
 * @param elem_symbols  Array of element symbols (NULL-terminated)
 * @param elem_counts   Array of per-element atom counts
 * @param n_elems       Number of distinct elements
 * @param[out] out      Output substance struct
 */
static void build_substance(const chemistry_sim_t* sim,
                            const char* name, const char* formula,
                            chem_phase_t phase, float molar_mass,
                            float mp, float bp, float enthalpy,
                            const char* elem_symbols[], const uint32_t elem_counts[],
                            uint32_t n_elems, chem_substance_t* out)
{
    memset(out, 0, sizeof(*out));
    strncpy(out->name, name, CHEM_MAX_NAME_LEN - 1);
    strncpy(out->formula, formula, CHEM_MAX_NAME_LEN - 1);
    out->default_phase     = phase;
    out->molar_mass        = molar_mass;
    out->melting_point     = mp;
    out->boiling_point     = bp;
    out->standard_enthalpy = enthalpy;
    out->num_elements      = n_elems;

    for (uint32_t i = 0; i < n_elems && i < CHEM_MAX_ELEMENTS; i++) {
        uint32_t eid = find_element_by_symbol(sim, elem_symbols[i]);
        out->element_ids[i]    = eid;
        out->element_counts[i] = elem_counts[i];
    }
}

/* ============================================================================
 * Load common substances
 * ============================================================================ */

void chemistry_sim_load_common_substances(chemistry_sim_t* sim)
{
    if (!sim) return;

    chem_substance_t sub;

    /* Water: H2O  (mp=273.15K, bp=373.15K, dHf=-285.8 kJ/mol) */
    {
        const char* elems[] = { "H", "O" };
        const uint32_t counts[] = { 2, 1 };
        build_substance(sim, "water", "H2O", CHEM_PHASE_LIQUID,
                        18.015f, 273.15f, 373.15f, -285.8f,
                        elems, counts, 2, &sub);
        chemistry_sim_add_substance(sim, &sub);
    }

    /* Carbon dioxide: CO2 (sublimes at 194.65K at 1atm, dHf=-393.5) */
    {
        const char* elems[] = { "C", "O" };
        const uint32_t counts[] = { 1, 2 };
        build_substance(sim, "carbon_dioxide", "CO2", CHEM_PHASE_GAS,
                        44.01f, 194.65f, 194.65f, -393.5f,
                        elems, counts, 2, &sub);
        chemistry_sim_add_substance(sim, &sub);
    }

    /* Oxygen: O2 (mp=54.36K, bp=90.19K) */
    {
        const char* elems[] = { "O" };
        const uint32_t counts[] = { 2 };
        build_substance(sim, "oxygen", "O2", CHEM_PHASE_GAS,
                        32.0f, 54.36f, 90.19f, 0.0f,
                        elems, counts, 1, &sub);
        chemistry_sim_add_substance(sim, &sub);
    }

    /* Glucose: C6H12O6 (mp=419K, decomposes before boiling, dHf=-1274) */
    {
        const char* elems[] = { "C", "H", "O" };
        const uint32_t counts[] = { 6, 12, 6 };
        build_substance(sim, "glucose", "C6H12O6", CHEM_PHASE_SOLID,
                        180.16f, 419.0f, 560.0f, -1274.0f,
                        elems, counts, 3, &sub);
        chemistry_sim_add_substance(sim, &sub);
    }

    /* Sodium chloride: NaCl (mp=1074K, bp=1686K, dHf=-411.2) */
    {
        const char* elems[] = { "Na", "Cl" };
        const uint32_t counts[] = { 1, 1 };
        build_substance(sim, "sodium_chloride", "NaCl", CHEM_PHASE_SOLID,
                        58.44f, 1074.0f, 1686.0f, -411.2f,
                        elems, counts, 2, &sub);
        chemistry_sim_add_substance(sim, &sub);
    }

    /* Hydrochloric acid: HCl (mp=159K, bp=188K, dHf=-92.3) */
    {
        const char* elems[] = { "H", "Cl" };
        const uint32_t counts[] = { 1, 1 };
        build_substance(sim, "hydrochloric_acid", "HCl", CHEM_PHASE_AQUEOUS,
                        36.46f, 159.0f, 188.0f, -92.3f,
                        elems, counts, 2, &sub);
        chemistry_sim_add_substance(sim, &sub);
    }

    /* Sodium hydroxide: NaOH (mp=596K, bp=1661K, dHf=-425.6) */
    {
        const char* elems[] = { "Na", "O", "H" };
        const uint32_t counts[] = { 1, 1, 1 };
        build_substance(sim, "sodium_hydroxide", "NaOH", CHEM_PHASE_AQUEOUS,
                        40.0f, 596.0f, 1661.0f, -425.6f,
                        elems, counts, 3, &sub);
        chemistry_sim_add_substance(sim, &sub);
    }

    /* Ammonia: NH3 (mp=195.4K, bp=239.8K, dHf=-45.9) */
    {
        const char* elems[] = { "N", "H" };
        const uint32_t counts[] = { 1, 3 };
        build_substance(sim, "ammonia", "NH3", CHEM_PHASE_GAS,
                        17.03f, 195.4f, 239.8f, -45.9f,
                        elems, counts, 2, &sub);
        chemistry_sim_add_substance(sim, &sub);
    }

    /* Methane: CH4 (mp=90.7K, bp=111.7K, dHf=-74.8) */
    {
        const char* elems[] = { "C", "H" };
        const uint32_t counts[] = { 1, 4 };
        build_substance(sim, "methane", "CH4", CHEM_PHASE_GAS,
                        16.04f, 90.7f, 111.7f, -74.8f,
                        elems, counts, 2, &sub);
        chemistry_sim_add_substance(sim, &sub);
    }

    /* Ethanol: C2H5OH (mp=159.0K, bp=351.4K, dHf=-277.7) */
    {
        const char* elems[] = { "C", "H", "O" };
        const uint32_t counts[] = { 2, 6, 1 };
        build_substance(sim, "ethanol", "C2H5OH", CHEM_PHASE_LIQUID,
                        46.07f, 159.0f, 351.4f, -277.7f,
                        elems, counts, 3, &sub);
        chemistry_sim_add_substance(sim, &sub);
    }

    /* Additional substances needed as reaction products/reactants */

    /* Hydrogen: H2 (mp=14.0K, bp=20.3K) */
    {
        const char* elems[] = { "H" };
        const uint32_t counts[] = { 2 };
        build_substance(sim, "hydrogen", "H2", CHEM_PHASE_GAS,
                        2.016f, 14.0f, 20.3f, 0.0f,
                        elems, counts, 1, &sub);
        chemistry_sim_add_substance(sim, &sub);
    }

    /* Iron: Fe (mp=1811K, bp=3134K) */
    {
        const char* elems[] = { "Fe" };
        const uint32_t counts[] = { 1 };
        build_substance(sim, "iron", "Fe", CHEM_PHASE_SOLID,
                        55.845f, 1811.0f, 3134.0f, 0.0f,
                        elems, counts, 1, &sub);
        chemistry_sim_add_substance(sim, &sub);
    }

    /* Iron(III) oxide (rust): Fe2O3 (mp=1839K, dHf=-824.2) */
    {
        const char* elems[] = { "Fe", "O" };
        const uint32_t counts[] = { 2, 3 };
        build_substance(sim, "iron_oxide", "Fe2O3", CHEM_PHASE_SOLID,
                        159.69f, 1839.0f, 3414.0f, -824.2f,
                        elems, counts, 2, &sub);
        chemistry_sim_add_substance(sim, &sub);
    }

    LOG_INFO("Loaded %u common substances", sim->num_substances);
}

/* ============================================================================
 * Load common reactions
 * ============================================================================ */

void chemistry_sim_load_common_reactions(chemistry_sim_t* sim)
{
    if (!sim) return;

    chem_reaction_t rxn;

    /*
     * Substance index lookup (depends on load order in load_common_substances):
     *  0 = water (H2O)
     *  1 = carbon_dioxide (CO2)
     *  2 = oxygen (O2)
     *  3 = glucose (C6H12O6)
     *  4 = sodium_chloride (NaCl)
     *  5 = hydrochloric_acid (HCl)
     *  6 = sodium_hydroxide (NaOH)
     *  7 = ammonia (NH3)
     *  8 = methane (CH4)
     *  9 = ethanol (C2H5OH)
     * 10 = hydrogen (H2)
     * 11 = iron (Fe)
     * 12 = iron_oxide (Fe2O3)
     */

    uint32_t ID_WATER   = find_substance_by_name(sim, "water");
    uint32_t ID_CO2     = find_substance_by_name(sim, "carbon_dioxide");
    uint32_t ID_O2      = find_substance_by_name(sim, "oxygen");
    uint32_t ID_GLUCOSE = find_substance_by_name(sim, "glucose");
    uint32_t ID_NACL    = find_substance_by_name(sim, "sodium_chloride");
    uint32_t ID_HCL     = find_substance_by_name(sim, "hydrochloric_acid");
    uint32_t ID_NAOH    = find_substance_by_name(sim, "sodium_hydroxide");
    uint32_t ID_CH4     = find_substance_by_name(sim, "methane");
    uint32_t ID_H2      = find_substance_by_name(sim, "hydrogen");
    uint32_t ID_FE      = find_substance_by_name(sim, "iron");
    uint32_t ID_FE2O3   = find_substance_by_name(sim, "iron_oxide");

    /* --- Reaction 1: Combustion of methane ---
     * CH4 + 2O2 -> CO2 + 2H2O
     * dH = -890.4 kJ/mol
     */
    memset(&rxn, 0, sizeof(rxn));
    rxn.num_reactants    = 2;
    rxn.reactant_ids[0]  = ID_CH4;   rxn.reactant_coeffs[0] = 1;
    rxn.reactant_ids[1]  = ID_O2;    rxn.reactant_coeffs[1] = 2;
    rxn.num_products     = 2;
    rxn.product_ids[0]   = ID_CO2;   rxn.product_coeffs[0]  = 1;
    rxn.product_ids[1]   = ID_WATER; rxn.product_coeffs[1]  = 2;
    rxn.rate_constant    = 0.1f;
    rxn.activation_energy = 150.0f;   /* high Ea — needs ignition */
    rxn.enthalpy_change  = -890.4f;
    rxn.reversible       = false;
    rxn.reverse_rate     = 0.0f;
    chemistry_sim_add_reaction(sim, &rxn);

    /* --- Reaction 2: Simplified photosynthesis ---
     * 6CO2 + 6H2O -> C6H12O6 + 6O2
     * dH = +2803 kJ/mol (endothermic, driven by light)
     * We use a small rate constant since it requires light energy.
     */
    memset(&rxn, 0, sizeof(rxn));
    rxn.num_reactants    = 2;
    rxn.reactant_ids[0]  = ID_CO2;   rxn.reactant_coeffs[0] = 6;
    rxn.reactant_ids[1]  = ID_WATER; rxn.reactant_coeffs[1] = 6;
    rxn.num_products     = 2;
    rxn.product_ids[0]   = ID_GLUCOSE; rxn.product_coeffs[0] = 1;
    rxn.product_ids[1]   = ID_O2;      rxn.product_coeffs[1] = 6;
    rxn.rate_constant    = 0.001f;      /* slow without light */
    rxn.activation_energy = 0.0f;
    rxn.enthalpy_change  = 2803.0f;
    rxn.reversible       = false;
    rxn.reverse_rate     = 0.0f;
    chemistry_sim_add_reaction(sim, &rxn);

    /* --- Reaction 3: Acid-base neutralization ---
     * HCl + NaOH -> NaCl + H2O
     * dH = -57.1 kJ/mol
     */
    memset(&rxn, 0, sizeof(rxn));
    rxn.num_reactants    = 2;
    rxn.reactant_ids[0]  = ID_HCL;  rxn.reactant_coeffs[0] = 1;
    rxn.reactant_ids[1]  = ID_NAOH; rxn.reactant_coeffs[1] = 1;
    rxn.num_products     = 2;
    rxn.product_ids[0]   = ID_NACL;  rxn.product_coeffs[0] = 1;
    rxn.product_ids[1]   = ID_WATER; rxn.product_coeffs[1] = 1;
    rxn.rate_constant    = 10.0f;     /* very fast in solution */
    rxn.activation_energy = 0.0f;
    rxn.enthalpy_change  = -57.1f;
    rxn.reversible       = false;
    rxn.reverse_rate     = 0.0f;
    chemistry_sim_add_reaction(sim, &rxn);

    /* --- Reaction 4: Water formation ---
     * 2H2 + O2 -> 2H2O
     * dH = -571.6 kJ/mol (per mol of O2)
     */
    memset(&rxn, 0, sizeof(rxn));
    rxn.num_reactants    = 2;
    rxn.reactant_ids[0]  = ID_H2;   rxn.reactant_coeffs[0] = 2;
    rxn.reactant_ids[1]  = ID_O2;   rxn.reactant_coeffs[1] = 1;
    rxn.num_products     = 1;
    rxn.product_ids[0]   = ID_WATER; rxn.product_coeffs[0] = 2;
    rxn.rate_constant    = 0.05f;
    rxn.activation_energy = 75.0f;
    rxn.enthalpy_change  = -571.6f;
    rxn.reversible       = false;
    rxn.reverse_rate     = 0.0f;
    chemistry_sim_add_reaction(sim, &rxn);

    /* --- Reaction 5: Cellular respiration (simplified) ---
     * C6H12O6 + 6O2 -> 6CO2 + 6H2O
     * dH = -2803 kJ/mol
     */
    memset(&rxn, 0, sizeof(rxn));
    rxn.num_reactants    = 2;
    rxn.reactant_ids[0]  = ID_GLUCOSE; rxn.reactant_coeffs[0] = 1;
    rxn.reactant_ids[1]  = ID_O2;      rxn.reactant_coeffs[1] = 6;
    rxn.num_products     = 2;
    rxn.product_ids[0]   = ID_CO2;     rxn.product_coeffs[0] = 6;
    rxn.product_ids[1]   = ID_WATER;   rxn.product_coeffs[1] = 6;
    rxn.rate_constant    = 0.01f;
    rxn.activation_energy = 50.0f;
    rxn.enthalpy_change  = -2803.0f;
    rxn.reversible       = false;
    rxn.reverse_rate     = 0.0f;
    chemistry_sim_add_reaction(sim, &rxn);

    /* --- Reaction 6: Rusting (simplified) ---
     * 4Fe + 3O2 + 6H2O -> 4Fe(OH)3 ... simplified as:
     * 4Fe + 3O2 -> 2Fe2O3
     * dH = -1648.4 kJ (for 4 mol Fe)
     * Note: Real rusting involves water as catalyst; we simplify.
     */
    memset(&rxn, 0, sizeof(rxn));
    rxn.num_reactants    = 2;
    rxn.reactant_ids[0]  = ID_FE;   rxn.reactant_coeffs[0] = 4;
    rxn.reactant_ids[1]  = ID_O2;   rxn.reactant_coeffs[1] = 3;
    rxn.num_products     = 1;
    rxn.product_ids[0]   = ID_FE2O3; rxn.product_coeffs[0] = 2;
    rxn.rate_constant    = 0.0001f;   /* very slow at room temp */
    rxn.activation_energy = 40.0f;
    rxn.enthalpy_change  = -1648.4f;
    rxn.reversible       = false;
    rxn.reverse_rate     = 0.0f;
    chemistry_sim_add_reaction(sim, &rxn);

    LOG_INFO("Loaded %u common reactions", sim->num_reactions);
}

/* ============================================================================
 * Internal: compute reaction rate (mass-action kinetics)
 * ============================================================================ */

/**
 * Forward: rate = k * product([reactant_i]^coeff_i)
 * If activation_energy > 0, apply Arrhenius factor: k_eff = k * exp(-Ea / (R*T))
 *
 * Reverse: rate = k_rev * product([product_i]^coeff_i)
 */
static float compute_reaction_rate(const chemistry_sim_t* sim,
                                   const chem_reaction_t* rxn,
                                   bool forward)
{
    float k;
    uint32_t n_species;
    const uint32_t* species_ids;
    const uint32_t* species_coeffs;

    if (forward) {
        k = rxn->rate_constant;
        n_species      = rxn->num_reactants;
        species_ids    = rxn->reactant_ids;
        species_coeffs = rxn->reactant_coeffs;
    } else {
        k = rxn->reverse_rate;
        n_species      = rxn->num_products;
        species_ids    = rxn->product_ids;
        species_coeffs = rxn->product_coeffs;
    }

    if (k <= 0.0f) return 0.0f;

    /* Arrhenius temperature dependence (forward only) */
    if (forward && rxn->activation_energy > 0.0f &&
        sim->state.temperature > 0.0f) {
        float exponent = -rxn->activation_energy
                         / (R_GAS_CONSTANT * sim->state.temperature);
        /* Clamp exponent to prevent underflow/overflow */
        if (exponent < -80.0f) return 0.0f;
        if (exponent > 80.0f) exponent = 80.0f;
        k *= expf(exponent);
    }

    /* Mass-action: rate = k * product(conc^coeff) */
    float rate = k;
    for (uint32_t i = 0; i < n_species; i++) {
        uint32_t sid = species_ids[i];
        if (sid >= sim->num_substances) return 0.0f;

        float conc = sim->state.concentrations[sid];
        if (conc < CONC_EPSILON) return 0.0f;

        uint32_t coeff = species_coeffs[i];
        /* For small integer powers, multiply directly for stability */
        for (uint32_t p = 0; p < coeff; p++) {
            rate *= conc;
        }
    }

    return rate;
}

/* ============================================================================
 * Internal: update phases based on temperature
 * ============================================================================ */

static void update_phases(chemistry_sim_t* sim)
{
    float T = sim->state.temperature;

    for (uint32_t s = 0; s < sim->num_substances; s++) {
        if (!sim->substances[s].active) continue;
        if (sim->state.concentrations[s] < CONC_EPSILON) continue;

        const chem_substance_t* sub = &sim->substances[s];

        /* Skip aqueous substances — they stay dissolved */
        if (sub->default_phase == CHEM_PHASE_AQUEOUS) {
            sim->state.phases[s] = CHEM_PHASE_AQUEOUS;
            continue;
        }

        /* Skip if melting/boiling points not set */
        if (sub->melting_point <= 0.0f || sub->boiling_point <= 0.0f) {
            sim->state.phases[s] = sub->default_phase;
            continue;
        }

        if (T < sub->melting_point) {
            sim->state.phases[s] = CHEM_PHASE_SOLID;
        } else if (T < sub->boiling_point) {
            sim->state.phases[s] = CHEM_PHASE_LIQUID;
        } else {
            sim->state.phases[s] = CHEM_PHASE_GAS;
        }
    }
}

/* ============================================================================
 * Internal: update pH from H+ concentration
 * ============================================================================ */

/**
 * pH = -log10([H+])
 *
 * We look for substances that produce H+ ions. In our simplified model:
 * - HCl (hydrochloric_acid) fully dissociates: [H+] = [HCl]
 * - NaOH produces OH-: [OH-] = [NaOH], and Kw = [H+][OH-] = 1e-14
 * - Water autoionization contributes [H+] = 1e-7 at neutral
 */
static void update_ph(chemistry_sim_t* sim)
{
    /* Find HCl and NaOH by name */
    uint32_t hcl_id  = find_substance_by_name(sim, "hydrochloric_acid");
    uint32_t naoh_id = find_substance_by_name(sim, "sodium_hydroxide");

    float h_plus = 1e-7f;  /* water autoionization baseline */

    /* Strong acid contribution: HCl -> H+ + Cl- (complete dissociation) */
    if (hcl_id < sim->num_substances) {
        h_plus += sim->state.concentrations[hcl_id];
    }

    /* Strong base contribution: NaOH -> Na+ + OH-
     * Kw = [H+][OH-] = 1e-14 at 25C
     * If [OH-] increases, [H+] = Kw / [OH-]
     */
    if (naoh_id < sim->num_substances) {
        float oh_minus = sim->state.concentrations[naoh_id] + 1e-7f;
        float kw = 1e-14f;
        float h_from_base = kw / oh_minus;
        /* If base dominates, H+ is lowered */
        float acid_conc = (hcl_id < sim->num_substances) ? sim->state.concentrations[hcl_id] : 0.0f;
        if (sim->state.concentrations[naoh_id] > acid_conc) {
            h_plus = h_from_base;
        }
    }

    /* Clamp to valid range */
    if (h_plus < 1e-14f) h_plus = 1e-14f;
    if (h_plus > 10.0f) h_plus = 10.0f;

    sim->state.pH = -log10f(h_plus);

    /* Clamp pH to [0, 14] */
    if (sim->state.pH < 0.0f) sim->state.pH = 0.0f;
    if (sim->state.pH > 14.0f) sim->state.pH = 14.0f;
}

/* ============================================================================
 * Internal: snapshot initial atom counts for conservation tracking
 * ============================================================================ */

static void snapshot_initial_atoms(chemistry_sim_t* sim)
{
    memset(sim->initial_atom_counts, 0, sizeof(sim->initial_atom_counts));
    compute_atom_counts_internal(sim, sim->initial_atom_counts, CHEM_MAX_ELEMENTS);
    sim->initial_total_mass = compute_total_mass(sim);
}

/* ============================================================================
 * Internal: compute total mass (grams)
 * ============================================================================ */

static float compute_total_mass(const chemistry_sim_t* sim)
{
    float total = 0.0f;
    for (uint32_t s = 0; s < sim->num_substances; s++) {
        if (!sim->substances[s].active) continue;
        /* mass = concentration (mol/L) * volume (L) * molar_mass (g/mol) */
        total += sim->state.concentrations[s] * sim->state.volume
                 * sim->substances[s].molar_mass;
    }
    return total;
}

/* ============================================================================
 * Internal: compute atom counts (moles of each element)
 * ============================================================================ */

static void compute_atom_counts_internal(const chemistry_sim_t* sim,
                                         float* counts, uint32_t max_elem)
{
    memset(counts, 0, max_elem * sizeof(float));

    for (uint32_t s = 0; s < sim->num_substances; s++) {
        if (!sim->substances[s].active) continue;
        float moles = sim->state.concentrations[s] * sim->state.volume;
        if (moles < CONC_EPSILON) continue;

        for (uint32_t e = 0; e < sim->substances[s].num_elements; e++) {
            uint32_t eid = sim->substances[s].element_ids[e];
            if (eid < max_elem) {
                counts[eid] += moles * (float)sim->substances[s].element_counts[e];
            }
        }
    }
}

/* ============================================================================
 * Internal: find element by symbol
 * ============================================================================ */

static uint32_t find_element_by_symbol(const chemistry_sim_t* sim,
                                       const char* symbol)
{
    if (!sim || !symbol) return UINT32_MAX;
    for (uint32_t i = 0; i < sim->num_elements; i++) {
        if (sim->elements[i].active &&
            strcmp(sim->elements[i].symbol, symbol) == 0) {
            return i;
        }
    }
    return UINT32_MAX;
}

/* ============================================================================
 * Internal: find substance by name
 * ============================================================================ */

static uint32_t find_substance_by_name(const chemistry_sim_t* sim,
                                       const char* name)
{
    if (!sim || !name) return UINT32_MAX;
    for (uint32_t i = 0; i < sim->num_substances; i++) {
        if (sim->substances[i].active &&
            strcmp(sim->substances[i].name, name) == 0) {
            return i;
        }
    }
    return UINT32_MAX;
}
