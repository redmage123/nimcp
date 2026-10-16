/**
 * @file nimcp_immunology.h
 * @brief Immunology simulation engine for world model
 *
 * WHAT: Simulates immune system responses: innate immunity (complement, phagocytosis,
 *       inflammation), adaptive immunity (B-cells, T-cells, antibodies), cytokine
 *       dynamics, and vaccine responses.
 * WHY:  Provides immunological reasoning for world model. Understanding how the
 *       body fights infection is essential for biological reasoning.
 * HOW:  Complement cascade (3 pathways -> MAC), antibody-antigen binding kinetics,
 *       T-cell activation (TCR+MHC), cytokine production/decay dynamics,
 *       primary vs secondary immune response modeling.
 *
 * THEORETICAL FOUNDATION:
 *   - Antibody binding: Ka = [Ab*Ag] / ([Ab]*[Ag])
 *   - Affinity maturation: Ka increases ~10-100x during response
 *   - Cytokine dynamics: dC/dt = production - decay - receptor_binding
 *   - Clonal expansion: exponential growth with ~8h doubling time
 *   - Memory response: 2-3 day onset (vs 7-10 day primary)
 */

#ifndef NIMCP_IMMUNOLOGY_H
#define NIMCP_IMMUNOLOGY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define IMMUNO_MAX_PATHOGENS        32
#define IMMUNO_MAX_IMMUNE_CELLS     128
#define IMMUNO_MAX_ANTIBODIES       64
#define IMMUNO_MAX_CYTOKINES        16
#define IMMUNO_MAX_NAME_LEN         32

/* Timing constants (hours) */
#define IMMUNO_INNATE_RESPONSE_H        0.5f    /* Minutes to hours */
#define IMMUNO_ADAPTIVE_ONSET_H         168.0f  /* 7 days for primary */
#define IMMUNO_MEMORY_ONSET_H           48.0f   /* 2 days for secondary */
#define IMMUNO_TCELL_DOUBLING_H         8.0f    /* Clonal expansion doubling */
#define IMMUNO_BCELL_DOUBLING_H         10.0f
#define IMMUNO_ANTIBODY_HALFLIFE_IGG_H  504.0f  /* 21 days */
#define IMMUNO_ANTIBODY_HALFLIFE_IGM_H  120.0f  /* 5 days */
#define IMMUNO_ANTIBODY_HALFLIFE_IGA_H  144.0f  /* 6 days */
#define IMMUNO_ANTIBODY_HALFLIFE_IGE_H  48.0f   /* 2 days */

/* Affinity constants */
#define IMMUNO_KA_LOW               1.0e5f  /* Low affinity Ka (M^-1) */
#define IMMUNO_KA_MODERATE          1.0e7f  /* Moderate affinity */
#define IMMUNO_KA_HIGH              1.0e9f  /* High affinity (matured) */
#define IMMUNO_KA_VERY_HIGH         1.0e11f /* Very high (secondary response) */

/* Complement cascade */
#define IMMUNO_MAC_THRESHOLD        0.8f    /* Membrane attack complex formation */
#define IMMUNO_COMPLEMENT_HALF_LIFE_H 24.0f

/* Cytokine concentrations (pg/mL) */
#define IMMUNO_IL1_BASELINE         5.0f
#define IMMUNO_IL6_BASELINE         2.0f
#define IMMUNO_TNF_ALPHA_BASELINE   3.0f
#define IMMUNO_IL2_BASELINE         1.0f
#define IMMUNO_IFNG_BASELINE        1.0f
#define IMMUNO_IL10_BASELINE        2.0f    /* Anti-inflammatory */

/* Normal cell counts (cells/uL blood) */
#define IMMUNO_NEUTROPHIL_NORMAL    4500.0f
#define IMMUNO_LYMPHOCYTE_NORMAL    2500.0f
#define IMMUNO_MONOCYTE_NORMAL      500.0f
#define IMMUNO_EOSINOPHIL_NORMAL    200.0f
#define IMMUNO_BASOPHIL_NORMAL      50.0f

/* ============================================================================
 * Enums
 * ============================================================================ */

typedef enum {
    IMMUNO_PATHOGEN_BACTERIA = 0,
    IMMUNO_PATHOGEN_VIRUS,
    IMMUNO_PATHOGEN_FUNGUS,
    IMMUNO_PATHOGEN_PARASITE,
    IMMUNO_PATHOGEN_COUNT
} immuno_pathogen_type_t;

typedef enum {
    IMMUNO_CELL_NEUTROPHIL = 0,     /* Innate - phagocyte */
    IMMUNO_CELL_MACROPHAGE,         /* Innate - phagocyte + APC */
    IMMUNO_CELL_DENDRITIC,          /* Innate - primary APC */
    IMMUNO_CELL_NK,                 /* Innate - natural killer */
    IMMUNO_CELL_MAST,               /* Innate - histamine release */
    IMMUNO_CELL_B_NAIVE,            /* Adaptive - naive B cell */
    IMMUNO_CELL_B_PLASMA,           /* Adaptive - antibody factory */
    IMMUNO_CELL_B_MEMORY,           /* Adaptive - long-lived memory */
    IMMUNO_CELL_T_HELPER,           /* Adaptive - CD4+ Th */
    IMMUNO_CELL_T_CYTOTOXIC,        /* Adaptive - CD8+ CTL */
    IMMUNO_CELL_T_REGULATORY,       /* Adaptive - Treg (suppressor) */
    IMMUNO_CELL_T_MEMORY,           /* Adaptive - memory T cell */
    IMMUNO_CELL_TYPE_COUNT
} immuno_cell_type_t;

typedef enum {
    IMMUNO_AB_IGM = 0,  /* First responder, pentamer, low affinity */
    IMMUNO_AB_IGG,      /* Most abundant, crosses placenta, high affinity */
    IMMUNO_AB_IGA,      /* Mucosal immunity, dimer */
    IMMUNO_AB_IGE,      /* Allergy/parasite, binds mast cells */
    IMMUNO_AB_IGD,      /* B-cell surface receptor */
    IMMUNO_AB_CLASS_COUNT
} immuno_antibody_class_t;

typedef enum {
    IMMUNO_COMPLEMENT_CLASSICAL = 0,    /* Antibody-mediated (C1q) */
    IMMUNO_COMPLEMENT_ALTERNATIVE,       /* Spontaneous C3 hydrolysis */
    IMMUNO_COMPLEMENT_LECTIN,           /* Mannose-binding lectin */
    IMMUNO_COMPLEMENT_PATHWAY_COUNT
} immuno_complement_pathway_t;

typedef enum {
    IMMUNO_CYTOKINE_IL1 = 0,    /* Pro-inflammatory, fever */
    IMMUNO_CYTOKINE_IL2,        /* T-cell growth factor */
    IMMUNO_CYTOKINE_IL6,        /* Pro-inflammatory, acute phase */
    IMMUNO_CYTOKINE_IL10,       /* Anti-inflammatory, regulatory */
    IMMUNO_CYTOKINE_TNF_ALPHA,  /* Pro-inflammatory, apoptosis */
    IMMUNO_CYTOKINE_IFN_GAMMA,  /* Antiviral, macrophage activation */
    IMMUNO_CYTOKINE_IL4,        /* Th2 response, IgE switching */
    IMMUNO_CYTOKINE_IL12,       /* Th1 response, NK activation */
    IMMUNO_CYTOKINE_TYPE_COUNT
} immuno_cytokine_type_t;

typedef enum {
    IMMUNO_MHC_CLASS_I = 0,     /* All nucleated cells -> CD8+ */
    IMMUNO_MHC_CLASS_II,        /* APCs only -> CD4+ */
    IMMUNO_MHC_CLASS_COUNT
} immuno_mhc_class_t;

/* ============================================================================
 * Structs
 * ============================================================================ */

/** Pathogen (infection agent) */
typedef struct {
    uint32_t                id;
    char                    name[IMMUNO_MAX_NAME_LEN];
    immuno_pathogen_type_t  type;
    float                   load;           /* pathogen count (log10 scale) */
    float                   virulence;      /* [0..1] damage capability */
    float                   replication_rate; /* doublings per hour */
    float                   immune_evasion; /* [0..1] ability to evade detection */
    float                   antigen_strength; /* [0..1] immunogenicity */
    bool                    intracellular;  /* virus/intracellular bacteria */
    bool                    active;
} immuno_pathogen_t;

/** Immune cell */
typedef struct {
    uint32_t            id;
    immuno_cell_type_t  type;
    float               count;          /* cells per uL */
    float               activation;     /* [0..1] activation state */
    float               specificity;    /* [0..1] antigen match quality */
    float               cytotoxicity;   /* [0..1] killing ability */
    float               phagocytic_rate; /* pathogens ingested per hour */
    uint32_t            target_pathogen; /* which pathogen this targets */
    bool                active;
} immuno_cell_t;

/** Antibody population */
typedef struct {
    uint32_t                id;
    immuno_antibody_class_t ab_class;
    float                   concentration;  /* ug/mL */
    float                   affinity_ka;    /* association constant M^-1 */
    uint32_t                target_pathogen;
    float                   neutralization_rate; /* pathogen neutralized/h */
    float                   opsonization_factor; /* phagocytosis enhancement */
    bool                    active;
} immuno_antibody_t;

/** Cytokine in the system */
typedef struct {
    immuno_cytokine_type_t  type;
    float                   concentration;  /* pg/mL */
    float                   production_rate; /* pg/mL/h */
    float                   decay_rate;     /* 1/h first-order decay */
    float                   baseline;       /* homeostatic level */
} immuno_cytokine_t;

/** Complement cascade state */
typedef struct {
    float   c3_level;       /* [0..1] C3 available */
    float   c3a_level;      /* anaphylatoxin */
    float   c3b_level;      /* opsonin */
    float   c5a_level;      /* chemotactic factor */
    float   mac_level;      /* membrane attack complex [0..1] */
    immuno_complement_pathway_t active_pathway;
    bool    activated;
} immuno_complement_state_t;

/** Inflammation state */
typedef struct {
    float   severity;       /* [0..1] */
    float   vasodilation;   /* [0..1] */
    float   permeability;   /* [0..1] vascular permeability */
    float   chemotaxis;     /* [0..1] immune cell recruitment */
    float   temperature_c;  /* local temperature (fever) */
    bool    acute;          /* acute vs chronic */
} immuno_inflammation_t;

/* ============================================================================
 * Config
 * ============================================================================ */

typedef struct {
    float   dt;                     /* time step in hours */
    float   body_temperature_c;     /* 37C default */
    bool    enable_complement;
    bool    enable_adaptive;
    bool    enable_inflammation;
    bool    enable_memory_cells;
    float   immune_competence;      /* [0..1] overall immune system health */
} immunology_config_t;

/* ============================================================================
 * Stats
 * ============================================================================ */

typedef struct {
    uint64_t    step_count;
    uint32_t    active_pathogens;
    uint32_t    active_immune_cells;
    uint32_t    antibody_types;
    float       total_pathogen_load;
    float       total_antibody_conc;
    float       inflammation_level;
    float       complement_activation;
    bool        infection_cleared;
} immunology_stats_t;

/* ============================================================================
 * Engine
 * ============================================================================ */

typedef struct immunology_sim {
    immuno_pathogen_t       pathogens[IMMUNO_MAX_PATHOGENS];
    uint32_t                num_pathogens;
    immuno_cell_t           cells[IMMUNO_MAX_IMMUNE_CELLS];
    uint32_t                num_cells;
    immuno_antibody_t       antibodies[IMMUNO_MAX_ANTIBODIES];
    uint32_t                num_antibodies;
    immuno_cytokine_t       cytokines[IMMUNO_CYTOKINE_TYPE_COUNT];
    immuno_complement_state_t complement;
    immuno_inflammation_t   inflammation;
    immunology_config_t     config;
    immunology_stats_t      stats;
    float                   time_since_infection_h;
    bool                    primary_response;   /* first exposure */
    bool                    initialized;
} immunology_sim_t;

/* ============================================================================
 * API
 * ============================================================================ */

immunology_sim_t* immunology_create(const immunology_config_t* config);
void immunology_destroy(immunology_sim_t* sim);
int immunology_step(immunology_sim_t* sim, float dt);
immunology_config_t immunology_default_config(void);
immunology_stats_t immunology_get_stats(const immunology_sim_t* sim);

/** Pathogen management */
int immunology_add_pathogen(immunology_sim_t* sim, const immuno_pathogen_t* p);
int immunology_infect(immunology_sim_t* sim, uint32_t pathogen_idx, float load);

/** Immune cell management */
int immunology_add_immune_cell(immunology_sim_t* sim, const immuno_cell_t* cell);

/** Antibody operations */
float immunology_antibody_binding(float ka, float ab_conc, float ag_conc);
int immunology_class_switch(immunology_sim_t* sim, uint32_t ab_idx,
                            immuno_antibody_class_t new_class);

/** Complement cascade */
int immunology_activate_complement(immunology_sim_t* sim,
                                   immuno_complement_pathway_t pathway);
int immunology_step_complement(immunology_sim_t* sim, float dt);

/** Cytokine dynamics */
int immunology_step_cytokines(immunology_sim_t* sim, float dt);

/** Vaccine simulation */
int immunology_vaccinate(immunology_sim_t* sim, uint32_t pathogen_idx,
                         float antigen_dose);

/** Load preset: bacterial infection scenario */
void immunology_load_bacterial_infection(immunology_sim_t* sim);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_IMMUNOLOGY_H */
