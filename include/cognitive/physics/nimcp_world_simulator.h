/**
 * @file nimcp_world_simulator.h
 * @brief Unified World Simulator — coupled multi-physics/chemistry/biology engine
 *
 * WHAT: Orchestrates ALL simulation engines as a single coupled system with
 *       shared state, cross-domain force/energy/mass coupling, synchronized
 *       time-stepping, and global conservation enforcement.
 * WHY:  Real-world phenomena are inherently multi-domain. A cup of coffee
 *       cooling involves fluid dynamics (convection), heat transfer (conduction
 *       + radiation + evaporative cooling), surface chemistry (volatile escape),
 *       and Newtonian mechanics (cup on table). These can't be simulated in
 *       isolation — they must couple through shared state.
 * HOW:  Shared world state (temperature, pressure, concentration, fields) that
 *       all engines read from and write to. Operator-splitting time integration:
 *       fast engines (particle, EM) sub-stepped within slow engines (biology,
 *       geology). Cross-domain couplings registered as transfer functions.
 *       Global energy/mass/charge conservation enforced post-step.
 *
 * COUPLING ARCHITECTURE:
 *
 *   ┌──────────────────── SHARED WORLD STATE ────────────────────┐
 *   │ temperature_field, pressure_field, velocity_field,          │
 *   │ concentration_field[], E_field, B_field, density_field,     │
 *   │ stress_field, biological_state                              │
 *   └──┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┘
 *      │   │   │   │   │   │   │   │   │   │   │   │   │   │
 *   Physics Fluid Heat  EM  MHD Chem Surf Biochem Molbio Cell ...
 *      │   │   │   │   │   │   │   │   │   │   │   │   │   │
 *   └──┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘
 *                     CROSS-DOMAIN COUPLINGS:
 *   Heat → Chemistry: Arrhenius rate = A·exp(-Ea/RT)
 *   Chemistry → Heat: ΔH reaction → heat source
 *   Fluid → Heat: convective transport
 *   Heat → Fluid: buoyancy (Boussinesq)
 *   EM → Fluid: Lorentz force (MHD)
 *   Chemistry → Biology: metabolite concentrations
 *   Biology → Chemistry: enzyme production rates
 *   Surface → Fluid: boundary conditions (no-slip, wetting)
 *   Nuclear → Heat: fission/fusion energy release
 *   Astrophysics → Relativistic: gravitational coupling
 */

#ifndef NIMCP_WORLD_SIMULATOR_H
#define NIMCP_WORLD_SIMULATOR_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/physics/nimcp_intuitive_physics.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define WSIM_MAX_ENGINES            36
#define WSIM_MAX_COUPLINGS          128
#define WSIM_MAX_GRID_DIM           32      /* shared field grid per axis */
#define WSIM_MAX_CONSERVED          8       /* tracked conservation quantities */
#define WSIM_MAX_TIMESCALES         8

/* ============================================================================
 * Engine Registry
 * ============================================================================ */

typedef enum {
    /* Physics */
    WSIM_ENGINE_NEWTONIAN       = 0,
    WSIM_ENGINE_RELATIVISTIC    = 1,
    WSIM_ENGINE_ELECTROMAGNETIC = 2,
    WSIM_ENGINE_MHD             = 3,
    WSIM_ENGINE_FLUID           = 4,
    WSIM_ENGINE_HEAT            = 5,
    WSIM_ENGINE_ACOUSTICS       = 6,
    WSIM_ENGINE_OPTICS          = 7,
    WSIM_ENGINE_NUCLEAR         = 8,
    WSIM_ENGINE_CONDENSED       = 9,
    WSIM_ENGINE_SURFACE_PHYS    = 10,
    WSIM_ENGINE_QED             = 11,
    WSIM_ENGINE_PARTICLE        = 12,
    WSIM_ENGINE_ASTROPHYSICS    = 13,
    /* Chemistry */
    WSIM_ENGINE_BULK_CHEM       = 14,
    WSIM_ENGINE_SURFACE_CHEM    = 15,
    WSIM_ENGINE_PHYSICAL_CHEM   = 16,
    WSIM_ENGINE_ORGANIC_CHEM    = 17,
    WSIM_ENGINE_POLYMER_CHEM    = 18,
    WSIM_ENGINE_ANALYTICAL_CHEM = 19,
    WSIM_ENGINE_INORGANIC_CHEM  = 20,
    WSIM_ENGINE_BIOCHEMISTRY    = 21,
    WSIM_ENGINE_CHEM_ENGINEERING = 22,
    WSIM_ENGINE_GEOCHEMISTRY    = 23,
    /* Biology */
    WSIM_ENGINE_BIOLOGY         = 24,
    WSIM_ENGINE_CELL_BIOLOGY    = 25,
    WSIM_ENGINE_MOLECULAR_BIO   = 26,
    WSIM_ENGINE_IMMUNOLOGY      = 27,
    WSIM_ENGINE_NEUROSCIENCE    = 28,
    WSIM_ENGINE_ECOLOGY         = 29,
    WSIM_ENGINE_EVOLUTION       = 30,
    WSIM_ENGINE_PHYSIOLOGY      = 31,
    /* Infrastructure */
    WSIM_ENGINE_ENTITY_TRACKER  = 32,
    WSIM_ENGINE_SCENE_GRAPH     = 33,
    WSIM_ENGINE_PHYSICS_PRIOR   = 34,
    WSIM_ENGINE_PERCEPTION      = 35,
    WSIM_ENGINE_COUNT
} wsim_engine_id_t;

/* ============================================================================
 * Shared World State — the common substrate all engines read/write
 * ============================================================================ */

typedef struct {
    /* Scalar fields (3D grids, shared across engines) */
    float*      temperature;        /* [nx*ny*nz] Kelvin */
    float*      pressure;           /* [nx*ny*nz] Pa */
    float*      density;            /* [nx*ny*nz] kg/m³ */

    /* Vector fields */
    float*      velocity;           /* [nx*ny*nz*3] m/s */
    float*      E_field;            /* [nx*ny*nz*3] V/m */
    float*      B_field;            /* [nx*ny*nz*3] Tesla */

    /* Chemical concentrations (per-species scalar fields) */
    float*      concentrations;     /* [num_species * nx*ny*nz] mol/L */
    uint32_t    num_species;

    /* Grid dimensions */
    uint32_t    nx, ny, nz;
    float       dx, dy, dz;        /* cell size (meters) */

    /* Global scalars (not spatially varying) */
    float       ambient_temperature;/* K */
    float       ambient_pressure;   /* Pa */
    float       total_time;         /* seconds */
    float       ph;                 /* pH of solution */
    float       gravity;            /* m/s² */
} wsim_shared_state_t;

/* ============================================================================
 * Cross-Domain Coupling
 * ============================================================================ */

typedef enum {
    /* Heat couplings */
    WSIM_COUPLE_CHEM_TO_HEAT,       /* reaction enthalpy → heat source */
    WSIM_COUPLE_HEAT_TO_CHEM,       /* temperature → Arrhenius rates */
    WSIM_COUPLE_NUCLEAR_TO_HEAT,    /* fission/fusion energy → heat */
    WSIM_COUPLE_FLUID_TO_HEAT,      /* convective heat transport */
    WSIM_COUPLE_HEAT_TO_FLUID,      /* buoyancy (Boussinesq) */
    WSIM_COUPLE_EM_TO_HEAT,         /* Ohmic heating (J²/σ) */
    /* Fluid couplings */
    WSIM_COUPLE_EM_TO_FLUID,        /* Lorentz force J×B (MHD) */
    WSIM_COUPLE_FLUID_TO_PHYSICS,   /* drag/buoyancy on rigid bodies */
    WSIM_COUPLE_SURFACE_TO_FLUID,   /* wetting, surface tension BC */
    /* Chemistry couplings */
    WSIM_COUPLE_PH_TO_CHEM,         /* pH affects reaction equilibria */
    WSIM_COUPLE_CHEM_TO_BIO,        /* metabolite concentrations → biology */
    WSIM_COUPLE_BIO_TO_CHEM,        /* enzyme production → reaction rates */
    WSIM_COUPLE_SURFACE_TO_CHEM,    /* surface adsorption modifies bulk conc */
    WSIM_COUPLE_GEOCHEM_TO_CHEM,    /* mineral dissolution → ions */
    /* Biology couplings */
    WSIM_COUPLE_BIOCHEM_TO_CELL,    /* ATP/metabolites → cell energy */
    WSIM_COUPLE_CELL_TO_PHYSIOL,    /* cell growth → organ function */
    WSIM_COUPLE_IMMUNE_TO_CELL,     /* immune response → cell fate */
    WSIM_COUPLE_MOLBIO_TO_BIOCHEM,  /* gene expression → enzyme levels */
    WSIM_COUPLE_ECOLOGY_TO_BIO,     /* environment → population dynamics */
    WSIM_COUPLE_EVOLUTION_TO_BIO,   /* selection → allele frequencies */
    /* Physics couplings */
    WSIM_COUPLE_ASTRO_TO_REL,       /* gravitational field → spacetime */
    WSIM_COUPLE_QED_TO_EM,          /* quantum corrections → classical EM */
    WSIM_COUPLE_CONDENSED_TO_EM,    /* band structure → conductivity */
    WSIM_COUPLE_COUNT
} wsim_coupling_type_t;

typedef struct {
    wsim_coupling_type_t type;
    wsim_engine_id_t    source;     /* engine that produces the quantity */
    wsim_engine_id_t    target;     /* engine that consumes it */
    float               strength;   /* coupling coefficient [0..1] */
    bool                enabled;
} wsim_coupling_t;

/* ============================================================================
 * Timescale Management
 * ============================================================================ */

typedef struct {
    wsim_engine_id_t    engine;
    float               dt;         /* timestep for this engine */
    float               min_dt;     /* smallest allowed */
    float               max_dt;     /* largest allowed */
    uint32_t            substeps;   /* how many sub-steps per master step */
} wsim_timescale_t;

/* ============================================================================
 * Conservation Tracking
 * ============================================================================ */

typedef enum {
    WSIM_CONSERVE_ENERGY    = 0,
    WSIM_CONSERVE_MASS      = 1,
    WSIM_CONSERVE_CHARGE    = 2,
    WSIM_CONSERVE_MOMENTUM  = 3,
    WSIM_CONSERVE_BARYON    = 4,
    WSIM_CONSERVE_LEPTON    = 5,
    WSIM_CONSERVE_ATOMS     = 6,    /* per-element atom count */
    WSIM_CONSERVE_COUNT
} wsim_conservation_t;

typedef struct {
    float       initial_value;
    float       current_value;
    float       drift;              /* (current - initial) / |initial| */
    float       max_drift;          /* worst drift seen */
    bool        tracked;
} wsim_conservation_tracker_t;

/* ============================================================================
 * Config
 * ============================================================================ */

typedef struct {
    float       master_dt;          /* outer timestep (seconds, default: 0.01) */
    uint32_t    grid_dim;           /* shared grid resolution (default: 32) */
    float       cell_size;          /* meters per cell (default: 0.01) */
    float       ambient_temperature;/* K (default: 293.15) */
    float       ambient_pressure;   /* Pa (default: 101325) */
    float       gravity;            /* m/s² (default: 9.81) */
    bool        enforce_conservation; /* correct drift post-step */
    float       conservation_tolerance; /* max allowed drift before correction */
    bool        enable_all_couplings;   /* turn on all registered couplings */
} wsim_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

typedef struct {
    uint64_t    master_steps;
    uint64_t    total_substeps;     /* sum of all engine substeps */
    uint64_t    couplings_applied;
    uint64_t    conservation_corrections;
    float       total_energy;
    float       total_mass;
    float       total_charge;
    float       energy_drift;
    float       mass_drift;
    float       charge_drift;
    float       wall_time_us;       /* per master step */
    uint32_t    active_engines;
    uint32_t    active_couplings;
} wsim_stats_t;

/* ============================================================================
 * World Simulator
 * ============================================================================ */

typedef struct world_simulator {
    /* Shared state */
    wsim_shared_state_t     state;

    /* Registered engines (opaque pointers — the simulator doesn't own them) */
    void*                   engines[WSIM_ENGINE_COUNT];
    bool                    engine_active[WSIM_ENGINE_COUNT];

    /* Cross-domain couplings */
    wsim_coupling_t         couplings[WSIM_MAX_COUPLINGS];
    uint32_t                num_couplings;

    /* Timescale management */
    wsim_timescale_t        timescales[WSIM_ENGINE_COUNT];

    /* Conservation tracking */
    wsim_conservation_tracker_t conservation[WSIM_CONSERVE_COUNT];

    wsim_config_t           config;
    wsim_stats_t            stats;
    bool                    initialized;
} world_simulator_t;

/* ============================================================================
 * API
 * ============================================================================ */

world_simulator_t* wsim_create(const wsim_config_t* config);
void wsim_destroy(world_simulator_t* sim);

/** Register an engine (non-owning pointer) */
void wsim_register_engine(world_simulator_t* sim, wsim_engine_id_t id,
                            void* engine, float dt);

/** Register a cross-domain coupling */
uint32_t wsim_register_coupling(world_simulator_t* sim, const wsim_coupling_t* coupling);

/** Enable all standard couplings (heat↔chemistry, fluid↔heat, etc.) */
void wsim_enable_standard_couplings(world_simulator_t* sim);

/**
 * @brief Step the entire world forward by master_dt
 *
 * Operator-splitting integration:
 * 1. Apply cross-domain couplings (source → shared state → target)
 * 2. Sub-step fast engines (particle, EM at their own dt)
 * 3. Step medium engines (fluid, heat, chemistry)
 * 4. Step slow engines (biology, ecology, geology)
 * 5. Enforce conservation laws (energy, mass, charge)
 * 6. Update shared state from engine outputs
 */
int wsim_step(world_simulator_t* sim, float dt);

/** Set temperature at a grid point (affects all engines reading temperature) */
void wsim_set_temperature(world_simulator_t* sim,
                            uint32_t ix, uint32_t iy, uint32_t iz, float T);

/** Set pressure at a grid point */
void wsim_set_pressure(world_simulator_t* sim,
                         uint32_t ix, uint32_t iy, uint32_t iz, float P);

/** Set chemical concentration at a grid point */
void wsim_set_concentration(world_simulator_t* sim, uint32_t species,
                              uint32_t ix, uint32_t iy, uint32_t iz, float conc);

/** Get temperature at a grid point */
float wsim_get_temperature(const world_simulator_t* sim,
                             uint32_t ix, uint32_t iy, uint32_t iz);

/** Get total energy across all engines */
float wsim_total_energy(const world_simulator_t* sim);

/** Get total mass across all engines */
float wsim_total_mass(const world_simulator_t* sim);

/** Get conservation report */
void wsim_conservation_report(const world_simulator_t* sim,
                                float* energy_drift, float* mass_drift,
                                float* charge_drift);

/** Check if a scenario violates any conservation law */
bool wsim_check_violations(const world_simulator_t* sim);

/** Get stats */
wsim_stats_t wsim_get_stats(const world_simulator_t* sim);

/** Default config */
wsim_config_t wsim_default_config(void);

/**
 * @brief Auto-wire all engines from a brain struct
 *
 * Scans the brain for all available simulation engines and registers
 * them with the world simulator, including standard couplings.
 */
struct brain_struct;
int wsim_auto_wire_from_brain(world_simulator_t* sim, struct brain_struct* brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WORLD_SIMULATOR_H */
