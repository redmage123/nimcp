/**
 * @file nimcp_cell_biology.h
 * @brief Cell Biology simulation engine for world model
 *
 * WHAT: Simulates cellular processes: cell cycle (G1/S/G2/M), mitosis, meiosis,
 *       membrane transport, signal transduction, and apoptosis.
 * WHY:  Provides cellular biology prior for world model. Understanding cell
 *       division, membrane transport, and signaling cascades is fundamental
 *       to reasoning about living systems.
 * HOW:  Phase-based cell cycle with CDK/cyclin checkpoints, Fick's law for
 *       passive diffusion, Michaelis-Menten for facilitated transport,
 *       ATP-coupled active transport, caspase cascade for apoptosis.
 *
 * THEORETICAL FOUNDATION:
 *   - Cell cycle: G1(11h) -> S(8h) -> G2(4h) -> M(1h) for mammalian cells
 *   - Fick's first law: J = -D * dC/dx (passive diffusion flux)
 *   - Michaelis-Menten: v = Vmax * [S] / (Km + [S]) (facilitated transport)
 *   - van't Hoff: pi = i*M*R*T (osmotic pressure)
 *   - Signal amplification: ~1000x per cascade step
 */

#ifndef NIMCP_CELL_BIOLOGY_H
#define NIMCP_CELL_BIOLOGY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants - Real mammalian cell biology values
 * ============================================================================ */

#define CELL_MAX_CELLS              128
#define CELL_MAX_CHANNELS           64
#define CELL_MAX_SIGNALS            32
#define CELL_MAX_ORGANELLES         16
#define CELL_MAX_NAME_LEN           32

/* Cell cycle phase durations (hours) - typical mammalian somatic cell (~24h total) */
#define CELL_PHASE_G1_DURATION      11.0f   /* Gap 1: growth and checkpoint */
#define CELL_PHASE_S_DURATION       8.0f    /* Synthesis: DNA replication */
#define CELL_PHASE_G2_DURATION      4.0f    /* Gap 2: preparation for mitosis */
#define CELL_PHASE_M_DURATION       1.0f    /* Mitosis: division */
#define CELL_TOTAL_CYCLE_HOURS      24.0f

/* CDK/Cyclin thresholds for checkpoint passage */
#define CELL_G1_CYCLIN_D_THRESHOLD  0.6f    /* Cyclin D/CDK4 for restriction point */
#define CELL_S_CYCLIN_E_THRESHOLD   0.7f    /* Cyclin E/CDK2 for S-phase entry */
#define CELL_G2_CYCLIN_A_THRESHOLD  0.65f   /* Cyclin A/CDK1 for G2 checkpoint */
#define CELL_M_CYCLIN_B_THRESHOLD   0.8f    /* Cyclin B/CDK1 for mitosis entry */

/* Membrane transport constants */
#define CELL_GAS_CONSTANT           8.314f  /* J/(mol*K) */
#define CELL_FARADAY_CONSTANT       96485.0f /* C/mol */
#define CELL_BODY_TEMP_K            310.15f /* 37C in Kelvin */
#define CELL_MEMBRANE_THICKNESS_NM  7.5f    /* typical lipid bilayer thickness */

/* ATP energetics */
#define CELL_ATP_HYDROLYSIS_KJ      30.5f   /* kJ/mol from ATP hydrolysis */
#define CELL_ATP_PER_GLUCOSE        36.0f   /* net ATP from aerobic respiration */

/* Apoptosis thresholds */
#define CELL_CASPASE_ACTIVATION     0.5f    /* threshold for initiator caspase */
#define CELL_APOPTOSIS_POINT_OF_NO_RETURN 0.7f /* executioner caspase threshold */

/* Signal transduction amplification */
#define CELL_SIGNAL_AMPLIFICATION   1000.0f /* per cascade step */
#define CELL_MAX_CASCADE_STEPS      5

/* ============================================================================
 * Enums
 * ============================================================================ */

typedef enum {
    CELL_PHASE_G0 = 0,     /* Quiescent - non-dividing */
    CELL_PHASE_G1,         /* Gap 1 - growth, CDK4/6-CyclinD checkpoint */
    CELL_PHASE_S,          /* DNA Synthesis */
    CELL_PHASE_G2,         /* Gap 2 - CDK1-CyclinA checkpoint */
    CELL_PHASE_M_PROPHASE, /* Chromosome condensation */
    CELL_PHASE_M_METAPHASE,/* Alignment at metaphase plate */
    CELL_PHASE_M_ANAPHASE, /* Sister chromatid separation */
    CELL_PHASE_M_TELOPHASE,/* Nuclear envelope reformation */
    CELL_PHASE_CYTOKINESIS,/* Cytoplasm division */
    CELL_PHASE_APOPTOSIS,  /* Programmed cell death */
    CELL_PHASE_COUNT
} cell_phase_t;

typedef enum {
    CELL_TRANSPORT_PASSIVE_DIFFUSION = 0,   /* Fick's law: J = -D * dC/dx */
    CELL_TRANSPORT_FACILITATED,             /* Michaelis-Menten kinetics */
    CELL_TRANSPORT_ACTIVE_PRIMARY,          /* ATP-powered (Na+/K+ ATPase) */
    CELL_TRANSPORT_ACTIVE_SECONDARY,        /* Coupled to ion gradient */
    CELL_TRANSPORT_OSMOSIS,                 /* Water movement: van't Hoff */
    CELL_TRANSPORT_ENDOCYTOSIS,             /* Vesicle-mediated import */
    CELL_TRANSPORT_EXOCYTOSIS,              /* Vesicle-mediated export */
    CELL_TRANSPORT_COUNT
} cell_transport_type_t;

typedef enum {
    CELL_SIGNAL_GPCR = 0,      /* G-protein coupled receptor */
    CELL_SIGNAL_RTK,            /* Receptor tyrosine kinase */
    CELL_SIGNAL_ION_CHANNEL,    /* Ligand-gated ion channel */
    CELL_SIGNAL_NUCLEAR,        /* Intracellular/nuclear receptor */
    CELL_SIGNAL_COUNT
} cell_signal_type_t;

typedef enum {
    CELL_ORGANELLE_NUCLEUS = 0,
    CELL_ORGANELLE_MITOCHONDRIA,
    CELL_ORGANELLE_ER_ROUGH,
    CELL_ORGANELLE_ER_SMOOTH,
    CELL_ORGANELLE_GOLGI,
    CELL_ORGANELLE_LYSOSOME,
    CELL_ORGANELLE_RIBOSOME,
    CELL_ORGANELLE_CYTOSKELETON,
    CELL_ORGANELLE_COUNT
} cell_organelle_type_t;

typedef enum {
    CELL_DIVISION_MITOSIS = 0,
    CELL_DIVISION_MEIOSIS_I,
    CELL_DIVISION_MEIOSIS_II,
    CELL_DIVISION_COUNT
} cell_division_type_t;

/* ============================================================================
 * Structs
 * ============================================================================ */

/** Organelle state within a cell */
typedef struct {
    cell_organelle_type_t type;
    float           health;         /* [0..1] */
    float           activity;       /* [0..1] functional output */
    uint32_t        count;          /* number (e.g., ~1000 mitochondria) */
} cell_organelle_t;

/** CDK/Cyclin complex levels controlling cell cycle */
typedef struct {
    float cyclin_d;     /* G1 checkpoint: CyclinD/CDK4,6 */
    float cyclin_e;     /* G1/S transition: CyclinE/CDK2 */
    float cyclin_a;     /* S and G2: CyclinA/CDK2,1 */
    float cyclin_b;     /* M-phase: CyclinB/CDK1 (MPF) */
    float p53;          /* Tumor suppressor - DNA damage checkpoint */
    float p21;          /* CDK inhibitor induced by p53 */
    float rb;           /* Retinoblastoma protein - G1 gatekeeper */
} cell_cycle_regulators_t;

/** Individual simulated cell */
typedef struct {
    uint32_t            id;
    cell_phase_t        phase;
    float               phase_progress;     /* [0..1] progress through current phase */
    float               volume_um3;         /* cell volume in cubic micrometers */
    float               membrane_potential_mv; /* resting ~-70mV */
    float               atp_level;          /* [0..1] normalized ATP availability */
    float               dna_integrity;      /* [0..1] 1=intact, decreases with damage */
    uint32_t            chromosome_count;   /* 46 for human diploid */
    bool                is_diploid;
    cell_cycle_regulators_t regulators;
    cell_organelle_t    organelles[CELL_MAX_ORGANELLES];
    uint32_t            num_organelles;
    float               intracellular_ca;   /* [Ca2+] in uM, resting ~0.1 */
    float               intracellular_na;   /* [Na+] in mM, ~12 */
    float               intracellular_k;    /* [K+] in mM, ~140 */
    float               extracellular_na;   /* [Na+] in mM, ~145 */
    float               extracellular_k;    /* [K+] in mM, ~4 */
    bool                alive;
    float               age_hours;
} cell_sim_cell_t;

/** Membrane transport channel */
typedef struct {
    char                name[CELL_MAX_NAME_LEN];
    cell_transport_type_t type;
    float               diffusion_coeff;    /* D in m^2/s (Fick's law) */
    float               vmax;               /* Vmax for facilitated (mM/s) */
    float               km;                 /* Km for Michaelis-Menten (mM) */
    float               atp_cost;           /* ATP molecules per transport event */
    float               selectivity;        /* [0..1] ion selectivity */
    float               current_flux;       /* current transport rate */
    bool                open;               /* gate state */
} cell_transport_channel_t;

/** Signal transduction pathway */
typedef struct {
    char                name[CELL_MAX_NAME_LEN];
    cell_signal_type_t  type;
    float               ligand_concentration;   /* [L] in nM */
    float               receptor_occupancy;     /* [0..1] fraction bound */
    float               second_messenger;       /* cAMP, IP3, DAG, Ca2+ */
    float               kinase_activity[CELL_MAX_CASCADE_STEPS]; /* cascade amplification */
    uint32_t            cascade_depth;
    float               response_magnitude;     /* final cellular response */
    float               decay_rate;             /* signal termination rate */
} cell_signal_pathway_t;

/** Apoptosis (programmed cell death) state */
typedef struct {
    float   cytochrome_c;       /* mitochondrial release [0..1] */
    float   caspase_9;          /* initiator caspase [0..1] */
    float   caspase_3;          /* executioner caspase [0..1] */
    float   bcl2;               /* anti-apoptotic [0..1] */
    float   bax;                /* pro-apoptotic [0..1] */
    float   dna_fragmentation;  /* [0..1] */
    bool    committed;          /* past point of no return */
} cell_apoptosis_state_t;

/* ============================================================================
 * Config
 * ============================================================================ */

typedef struct {
    float   dt;                 /* time step in hours */
    float   temperature_c;     /* environment temperature (37C default) */
    bool    enable_apoptosis;
    bool    enable_signal_transduction;
    bool    enable_meiosis;
    float   growth_factor_conc; /* external growth factor [0..1] */
    float   nutrient_level;     /* external nutrient availability [0..1] */
    float   oxygen_level;       /* O2 partial pressure [0..1] */
} cell_biology_config_t;

/* ============================================================================
 * Stats
 * ============================================================================ */

typedef struct {
    uint64_t    step_count;
    uint32_t    total_cells;
    uint32_t    cells_in_mitosis;
    uint32_t    cells_in_apoptosis;
    uint32_t    divisions_completed;
    float       mean_atp;
    float       mean_volume;
    float       total_membrane_flux;
    float       max_signal_amplitude;
} cell_biology_stats_t;

/* ============================================================================
 * Engine
 * ============================================================================ */

typedef struct cell_biology_sim {
    cell_sim_cell_t         cells[CELL_MAX_CELLS];
    uint32_t                num_cells;
    cell_transport_channel_t channels[CELL_MAX_CHANNELS];
    uint32_t                num_channels;
    cell_signal_pathway_t   signals[CELL_MAX_SIGNALS];
    uint32_t                num_signals;
    cell_apoptosis_state_t  apoptosis;
    cell_biology_config_t   config;
    cell_biology_stats_t    stats;
    bool                    initialized;
} cell_biology_sim_t;

/* ============================================================================
 * API
 * ============================================================================ */

cell_biology_sim_t* cell_biology_create(const cell_biology_config_t* config);
void cell_biology_destroy(cell_biology_sim_t* sim);
int cell_biology_step(cell_biology_sim_t* sim, float dt);
cell_biology_config_t cell_biology_default_config(void);
cell_biology_stats_t cell_biology_get_stats(const cell_biology_sim_t* sim);

/** Cell cycle operations */
int cell_biology_add_cell(cell_biology_sim_t* sim, const cell_sim_cell_t* cell);
int cell_biology_advance_cycle(cell_biology_sim_t* sim, uint32_t cell_idx, float dt);
bool cell_biology_check_checkpoint(const cell_biology_sim_t* sim, uint32_t cell_idx);

/** Division */
int cell_biology_mitosis(cell_biology_sim_t* sim, uint32_t cell_idx);
int cell_biology_meiosis(cell_biology_sim_t* sim, uint32_t cell_idx,
                         float crossover_rate);

/** Membrane transport */
int cell_biology_add_channel(cell_biology_sim_t* sim,
                             const cell_transport_channel_t* ch);
float cell_biology_fick_diffusion(float D, float dC, float dx);
float cell_biology_facilitated_transport(float vmax, float km, float substrate);
float cell_biology_osmotic_pressure(float i_factor, float molarity, float temp_k);
int cell_biology_step_transport(cell_biology_sim_t* sim, uint32_t cell_idx, float dt);

/** Signal transduction */
int cell_biology_add_signal(cell_biology_sim_t* sim,
                            const cell_signal_pathway_t* sig);
int cell_biology_step_signals(cell_biology_sim_t* sim, float dt);

/** Apoptosis */
int cell_biology_step_apoptosis(cell_biology_sim_t* sim, uint32_t cell_idx, float dt);

/** Load preset mammalian cell */
void cell_biology_load_mammalian_cell(cell_biology_sim_t* sim);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CELL_BIOLOGY_H */
