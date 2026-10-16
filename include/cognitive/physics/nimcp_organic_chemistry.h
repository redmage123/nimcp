/**
 * @file nimcp_organic_chemistry.h
 * @brief Organic Chemistry — functional groups, mechanisms, stereochemistry, synthesis
 *
 * Reaction types (SN1/SN2/E1/E2), functional group transformations,
 * stereochemistry (R/S, E/Z), retrosynthetic analysis, aromaticity.
 */

#ifndef NIMCP_ORGANIC_CHEMISTRY_H
#define NIMCP_ORGANIC_CHEMISTRY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OCHEM_MAX_MOLECULES     64
#define OCHEM_MAX_REACTIONS     128
#define OCHEM_MAX_ATOMS         64
#define OCHEM_MAX_BONDS         128
#define OCHEM_MAX_FGROUPS       16
#define OCHEM_MAX_NAME          48

typedef enum {
    OCHEM_ATOM_C=0, OCHEM_ATOM_H, OCHEM_ATOM_O, OCHEM_ATOM_N, OCHEM_ATOM_S,
    OCHEM_ATOM_P, OCHEM_ATOM_F, OCHEM_ATOM_CL, OCHEM_ATOM_BR, OCHEM_ATOM_I,
    OCHEM_ATOM_COUNT
} ochem_atom_type_t;

typedef enum {
    OCHEM_BOND_SINGLE=1, OCHEM_BOND_DOUBLE=2, OCHEM_BOND_TRIPLE=3,
    OCHEM_BOND_AROMATIC=4
} ochem_bond_order_t;

typedef enum {
    OCHEM_FG_ALKANE=0, OCHEM_FG_ALKENE, OCHEM_FG_ALKYNE, OCHEM_FG_ALCOHOL,
    OCHEM_FG_ALDEHYDE, OCHEM_FG_KETONE, OCHEM_FG_CARBOXYLIC_ACID,
    OCHEM_FG_ESTER, OCHEM_FG_ETHER, OCHEM_FG_AMINE, OCHEM_FG_AMIDE,
    OCHEM_FG_NITRILE, OCHEM_FG_THIOL, OCHEM_FG_PHENOL, OCHEM_FG_AROMATIC,
    OCHEM_FG_HALIDE, OCHEM_FG_EPOXIDE, OCHEM_FG_COUNT
} ochem_functional_group_t;

typedef enum {
    OCHEM_STEREO_NONE=0, OCHEM_STEREO_R, OCHEM_STEREO_S,
    OCHEM_STEREO_E, OCHEM_STEREO_Z,
    OCHEM_STEREO_CIS, OCHEM_STEREO_TRANS
} ochem_stereochemistry_t;

typedef enum {
    OCHEM_RXN_SN1=0, OCHEM_RXN_SN2, OCHEM_RXN_E1, OCHEM_RXN_E2,
    OCHEM_RXN_ADDITION, OCHEM_RXN_ELIMINATION, OCHEM_RXN_SUBSTITUTION,
    OCHEM_RXN_OXIDATION, OCHEM_RXN_REDUCTION, OCHEM_RXN_REARRANGEMENT,
    OCHEM_RXN_CONDENSATION, OCHEM_RXN_HYDROLYSIS, OCHEM_RXN_DIELS_ALDER,
    OCHEM_RXN_GRIGNARD, OCHEM_RXN_ALDOL, OCHEM_RXN_FRIEDEL_CRAFTS,
    OCHEM_RXN_RADICAL, OCHEM_RXN_POLYMERIZATION, OCHEM_RXN_COUNT
} ochem_reaction_type_t;

typedef struct {
    ochem_atom_type_t   type;
    int8_t              formal_charge;
    uint8_t             hybridization;  /* sp=1, sp2=2, sp3=3 */
    ochem_stereochemistry_t stereo;
    uint8_t             num_hydrogens;
} ochem_atom_t;

typedef struct {
    uint32_t            atom_a;
    uint32_t            atom_b;
    ochem_bond_order_t  order;
    bool                is_rotatable;
} ochem_bond_t;

typedef struct {
    uint32_t        id;
    char            name[OCHEM_MAX_NAME];
    char            smiles[OCHEM_MAX_NAME];     /* simplified molecular input */
    ochem_atom_t    atoms[OCHEM_MAX_ATOMS];
    uint32_t        num_atoms;
    ochem_bond_t    bonds[OCHEM_MAX_BONDS];
    uint32_t        num_bonds;
    ochem_functional_group_t functional_groups[OCHEM_MAX_FGROUPS];
    uint32_t        num_fgroups;
    float           molecular_weight;
    float           concentration;      /* mol/L (for kinetics) */
    float           logP;               /* partition coefficient (hydrophobicity) */
    uint32_t        degree_of_unsaturation;
    bool            is_aromatic;
    bool            is_chiral;
    bool            active;
} ochem_molecule_t;

typedef struct {
    uint32_t                id;
    ochem_reaction_type_t   type;
    uint32_t                reactant_ids[4];
    uint32_t                num_reactants;
    uint32_t                product_ids[4];
    uint32_t                num_products;
    float                   activation_energy;  /* kJ/mol */
    float                   rate_constant;
    bool                    stereospecific;
    ochem_stereochemistry_t product_stereo;     /* stereochemical outcome */
    char                    reagent[OCHEM_MAX_NAME];
    char                    solvent[OCHEM_MAX_NAME];
    bool                    active;
} ochem_reaction_t;

typedef struct { float dt; bool enable_stereochemistry; bool enable_mechanisms; } ochem_config_t;
typedef struct { uint64_t step_count; uint64_t reactions_run; float total_yield; } ochem_stats_t;

typedef struct organic_chemistry_sim {
    ochem_molecule_t    molecules[OCHEM_MAX_MOLECULES];
    uint32_t            num_molecules;
    ochem_reaction_t    reactions[OCHEM_MAX_REACTIONS];
    uint32_t            num_reactions;
    ochem_config_t      config;
    ochem_stats_t       stats;
    float               time;
    bool                initialized;
} organic_chemistry_sim_t;

organic_chemistry_sim_t* organic_chemistry_create(const ochem_config_t* config);
void organic_chemistry_destroy(organic_chemistry_sim_t* sim);
uint32_t organic_chemistry_add_molecule(organic_chemistry_sim_t* sim, const ochem_molecule_t* m);
uint32_t organic_chemistry_add_reaction(organic_chemistry_sim_t* sim, const ochem_reaction_t* r);
int organic_chemistry_step(organic_chemistry_sim_t* sim, float dt);

/** Predict reaction type from substrate + reagent */
ochem_reaction_type_t organic_chemistry_predict_mechanism(
    const ochem_molecule_t* substrate, const char* reagent, const char* solvent);
/** Compute degree of unsaturation: DoU = (2C+2+N-H-X)/2 */
uint32_t organic_chemistry_degree_unsaturation(uint32_t C, uint32_t H, uint32_t N, uint32_t X);
/** Check Huckel's rule: 4n+2 π electrons = aromatic */
bool organic_chemistry_is_huckel_aromatic(uint32_t pi_electrons);
/** Predict stereochemistry of SN2 (inversion) vs SN1 (racemization) */
ochem_stereochemistry_t organic_chemistry_predict_stereo(ochem_reaction_type_t rxn,
                                                          ochem_stereochemistry_t input);

ochem_config_t organic_chemistry_default_config(void);
ochem_stats_t organic_chemistry_get_stats(const organic_chemistry_sim_t* sim);

#ifdef __cplusplus
}
#endif
#endif
