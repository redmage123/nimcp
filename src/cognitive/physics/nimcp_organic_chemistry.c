/**
 * @file nimcp_organic_chemistry.c
 * @brief Organic Chemistry simulator — functional groups, mechanisms, stereochemistry
 *
 * Implements degree of unsaturation, Huckel aromaticity (4n+2), mechanism
 * prediction (SN1/SN2/E1/E2 based on substrate/nucleophile/solvent),
 * stereochemistry prediction, and reaction step with concentration updates.
 */

#include "cognitive/physics/nimcp_organic_chemistry.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>
#include <math.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define LOG_TAG "OCHEM"

#define CONC_EPSILON    1e-12f
#define R_GAS           8.314f   /* J/(mol*K) */
#define DEFAULT_TEMP    298.15f  /* K */

/* ============================================================================
 * Default config
 * ============================================================================ */

ochem_config_t organic_chemistry_default_config(void)
{
    ochem_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.dt = 0.01f;
    cfg.enable_stereochemistry = true;
    cfg.enable_mechanisms = true;
    return cfg;
}

/* ============================================================================
 * Create / Destroy
 * ============================================================================ */

organic_chemistry_sim_t* organic_chemistry_create(const ochem_config_t* config)
{
    organic_chemistry_sim_t* sim =
        (organic_chemistry_sim_t*)nimcp_calloc(1, sizeof(organic_chemistry_sim_t));
    if (!sim) {
        LOG_ERROR(LOG_TAG, "Failed to allocate organic_chemistry_sim_t");
        return NULL;
    }
    sim->config = config ? *config : organic_chemistry_default_config();
    sim->initialized = true;
    LOG_INFO(LOG_TAG, "Organic chemistry sim created");
    return sim;
}

void organic_chemistry_destroy(organic_chemistry_sim_t* sim)
{
    if (!sim) return;
    LOG_INFO(LOG_TAG, "Destroying organic chemistry sim (steps=%lu)",
             (unsigned long)sim->stats.step_count);
    nimcp_free(sim);
}

/* ============================================================================
 * Add entities
 * ============================================================================ */

uint32_t organic_chemistry_add_molecule(organic_chemistry_sim_t* sim,
                                         const ochem_molecule_t* m)
{
    if (!sim || !m) return UINT32_MAX;
    if (sim->num_molecules >= OCHEM_MAX_MOLECULES) {
        LOG_WARN(LOG_TAG, "Max molecules reached (%d)", OCHEM_MAX_MOLECULES);
        return UINT32_MAX;
    }
    uint32_t idx = sim->num_molecules++;
    sim->molecules[idx] = *m;
    sim->molecules[idx].id = idx;
    sim->molecules[idx].active = true;
    return idx;
}

uint32_t organic_chemistry_add_reaction(organic_chemistry_sim_t* sim,
                                         const ochem_reaction_t* r)
{
    if (!sim || !r) return UINT32_MAX;
    if (sim->num_reactions >= OCHEM_MAX_REACTIONS) {
        LOG_WARN(LOG_TAG, "Max reactions reached (%d)", OCHEM_MAX_REACTIONS);
        return UINT32_MAX;
    }
    uint32_t idx = sim->num_reactions++;
    sim->reactions[idx] = *r;
    sim->reactions[idx].id = idx;
    sim->reactions[idx].active = true;
    return idx;
}

/* ============================================================================
 * Degree of unsaturation (Index of Hydrogen Deficiency)
 * DoU = (2C + 2 + N - H - X) / 2
 * C = carbons, H = hydrogens, N = nitrogens, X = halogens
 * ============================================================================ */

uint32_t organic_chemistry_degree_unsaturation(uint32_t C, uint32_t H,
                                                uint32_t N, uint32_t X)
{
    int val = (int)(2 * C + 2 + N) - (int)(H + X);
    if (val < 0) val = 0;
    return (uint32_t)(val / 2);
}

/* ============================================================================
 * Huckel's rule: 4n+2 pi electrons = aromatic
 * Returns true if pi_electrons = 2, 6, 10, 14, 18, ...
 * ============================================================================ */

bool organic_chemistry_is_huckel_aromatic(uint32_t pi_electrons)
{
    if (pi_electrons < 2) return false;
    /* Check: (pi_electrons - 2) mod 4 == 0 */
    return ((pi_electrons - 2) % 4) == 0;
}

/* ============================================================================
 * Mechanism prediction
 *
 * Based on:
 * 1. Substrate: primary/secondary/tertiary carbon (from hybridization, H count)
 * 2. Nucleophile strength (from reagent name heuristics)
 * 3. Solvent polarity (from solvent name heuristics)
 *
 * Decision tree:
 * - Methyl/primary + strong nuc + polar aprotic -> SN2
 * - Tertiary + weak nuc + polar protic -> SN1/E1
 * - Tertiary + strong base + any -> E2
 * - Secondary: depends on base strength and temperature
 * ============================================================================ */

/** Heuristic: count C atoms bonded to the reactive center */
static int estimate_substrate_class(const ochem_molecule_t* mol)
{
    /* Look at first carbon's hydrogen count as proxy */
    /* 3H = primary (methyl), 2H = primary, 1H = secondary, 0H = tertiary */
    for (uint32_t i = 0; i < mol->num_atoms; i++) {
        if (mol->atoms[i].type == OCHEM_ATOM_C) {
            uint8_t nH = mol->atoms[i].num_hydrogens;
            if (nH >= 3) return 0;   /* methyl */
            if (nH == 2) return 1;   /* primary */
            if (nH == 1) return 2;   /* secondary */
            return 3;                /* tertiary */
        }
    }
    return 1;  /* default: primary */
}

/** Heuristic: is this a strong nucleophile/base? */
static bool is_strong_nucleophile(const char* reagent)
{
    if (!reagent) return false;
    /* Strong nucleophiles: CN-, I-, RS-, OH-, RO-, NH2- */
    if (strstr(reagent, "CN") || strstr(reagent, "NaI") ||
        strstr(reagent, "OH") || strstr(reagent, "OMe") ||
        strstr(reagent, "OEt") || strstr(reagent, "NH2") ||
        strstr(reagent, "SH") || strstr(reagent, "thiolate"))
        return true;
    return false;
}

/** Heuristic: is solvent polar protic? */
static bool is_polar_protic(const char* solvent)
{
    if (!solvent) return false;
    if (strstr(solvent, "water") || strstr(solvent, "H2O") ||
        strstr(solvent, "methanol") || strstr(solvent, "MeOH") ||
        strstr(solvent, "ethanol") || strstr(solvent, "EtOH") ||
        strstr(solvent, "acetic"))
        return true;
    return false;
}

/** Heuristic: is this a strong base? */
static bool is_strong_base(const char* reagent)
{
    if (!reagent) return false;
    if (strstr(reagent, "tBuO") || strstr(reagent, "LDA") ||
        strstr(reagent, "NaH") || strstr(reagent, "KOH") ||
        strstr(reagent, "NaOH") || strstr(reagent, "DBU"))
        return true;
    return false;
}

ochem_reaction_type_t organic_chemistry_predict_mechanism(
    const ochem_molecule_t* substrate, const char* reagent, const char* solvent)
{
    if (!substrate) return OCHEM_RXN_SN2;

    int sub_class = estimate_substrate_class(substrate);
    bool strong_nuc = is_strong_nucleophile(reagent);
    bool strong_base = is_strong_base(reagent);
    bool protic = is_polar_protic(solvent);

    /* Methyl or primary: SN2 favored with strong nucleophile */
    if (sub_class <= 1) {
        if (strong_base && !strong_nuc) return OCHEM_RXN_E2;
        return OCHEM_RXN_SN2;
    }

    /* Tertiary: SN2 impossible (steric), SN1 or E1/E2 */
    if (sub_class >= 3) {
        if (strong_base) return OCHEM_RXN_E2;
        if (protic) {
            /* Weak nuc + protic: SN1 and E1 compete, E1 wins at high T */
            return OCHEM_RXN_SN1;  /* default to SN1 at room temp */
        }
        return OCHEM_RXN_E1;
    }

    /* Secondary: borderline case */
    if (strong_base) return OCHEM_RXN_E2;
    if (strong_nuc && !protic) return OCHEM_RXN_SN2;
    if (protic && !strong_nuc) return OCHEM_RXN_SN1;
    return OCHEM_RXN_SN2;  /* default */
}

/* ============================================================================
 * Stereochemistry prediction
 *
 * SN2: Walden inversion (R->S, S->R)
 * SN1: Racemization (R or S -> NONE, representing mixture)
 * E2: Anti-periplanar elimination (E geometry if possible)
 * E1: No stereospecificity
 * ============================================================================ */

ochem_stereochemistry_t organic_chemistry_predict_stereo(
    ochem_reaction_type_t rxn, ochem_stereochemistry_t input)
{
    switch (rxn) {
    case OCHEM_RXN_SN2:
        /* Walden inversion */
        if (input == OCHEM_STEREO_R) return OCHEM_STEREO_S;
        if (input == OCHEM_STEREO_S) return OCHEM_STEREO_R;
        return input;

    case OCHEM_RXN_SN1:
        /* Racemization: loss of stereochemistry */
        if (input == OCHEM_STEREO_R || input == OCHEM_STEREO_S)
            return OCHEM_STEREO_NONE;  /* racemic mixture */
        return input;

    case OCHEM_RXN_E2:
        /* Anti-periplanar elimination gives trans (E) product */
        return OCHEM_STEREO_TRANS;

    case OCHEM_RXN_E1:
        /* No stereospecificity — Zaitsev's rule gives more substituted alkene */
        return OCHEM_STEREO_NONE;

    case OCHEM_RXN_DIELS_ALDER:
        /* Suprafacial-suprafacial: cis/endo rule */
        return OCHEM_STEREO_CIS;

    default:
        return input;
    }
}

/* ============================================================================
 * Step — run reactions and update concentrations
 * ============================================================================ */

int organic_chemistry_step(organic_chemistry_sim_t* sim, float dt)
{
    if (!sim || !sim->initialized) return -1;
    if (dt <= 0.0f) dt = sim->config.dt;
    if (dt <= 0.0f) dt = 0.01f;

    for (uint32_t i = 0; i < sim->num_reactions; i++) {
        ochem_reaction_t* rxn = &sim->reactions[i];
        if (!rxn->active) continue;

        /* Compute reaction rate using Arrhenius kinetics */
        float k = rxn->rate_constant;
        if (rxn->activation_energy > 0.0f) {
            /* k = A * exp(-Ea/(RT)), using rate_constant as pre-exponential A */
            k *= expf(-rxn->activation_energy * 1000.0f / (R_GAS * DEFAULT_TEMP));
        }

        /* Rate = k * product([reactant_i]) for each reactant */
        float rate = k;
        for (uint32_t j = 0; j < rxn->num_reactants; j++) {
            uint32_t rid = rxn->reactant_ids[j];
            if (rid < sim->num_molecules) {
                float conc = sim->molecules[rid].concentration;
                rate *= (conc > 1e-12f) ? conc : 1e-3f; /* default 1mM if unset */
            }
        }

        float consumed = rate * dt;

        /* Consume reactants (reduce concentration) */
        for (uint32_t j = 0; j < rxn->num_reactants; j++) {
            uint32_t rid = rxn->reactant_ids[j];
            if (rid < sim->num_molecules) {
                if (consumed > sim->molecules[rid].concentration)
                    consumed = sim->molecules[rid].concentration;
                sim->molecules[rid].concentration -= consumed;
                if (sim->molecules[rid].concentration < 0)
                    sim->molecules[rid].concentration = 0;
            }
        }

        /* Produce products (increase concentration) */
        for (uint32_t j = 0; j < rxn->num_products; j++) {
            uint32_t pid = rxn->product_ids[j];
            if (pid < sim->num_molecules) {
                sim->molecules[pid].concentration += consumed;
            }
        }

        /* Apply stereochemistry if enabled */
        if (sim->config.enable_stereochemistry && rxn->stereospecific) {
            for (uint32_t j = 0; j < rxn->num_products; j++) {
                uint32_t pid = rxn->product_ids[j];
                if (pid < sim->num_molecules) {
                    /* For stereospecific reactions, set the predicted stereo */
                    if (rxn->product_stereo != OCHEM_STEREO_NONE) {
                        /* Mark product atoms with predicted stereochemistry */
                        for (uint32_t a = 0; a < sim->molecules[pid].num_atoms; a++) {
                            if (sim->molecules[pid].atoms[a].type == OCHEM_ATOM_C &&
                                sim->molecules[pid].atoms[a].hybridization == 3) {
                                sim->molecules[pid].atoms[a].stereo = rxn->product_stereo;
                            }
                        }
                    }
                }
            }
        }

        sim->stats.reactions_run++;
    }

    sim->stats.step_count++;
    sim->time += dt;

    /* Compute total yield (sum of product concentrations) */
    float yield = 0.0f;
    for (uint32_t i = 0; i < sim->num_molecules; i++) {
        if (sim->molecules[i].active) {
            yield += sim->molecules[i].molecular_weight;
        }
    }
    sim->stats.total_yield = yield;

    return 0;
}

/* ============================================================================
 * Stats
 * ============================================================================ */

ochem_stats_t organic_chemistry_get_stats(const organic_chemistry_sim_t* sim)
{
    ochem_stats_t zero;
    memset(&zero, 0, sizeof(zero));
    if (!sim) return zero;
    return sim->stats;
}
