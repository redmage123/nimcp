/**
 * @file nimcp_particle_physics.h
 * @brief Particle Physics — Standard Model, decays, cross-sections, conservation
 *
 * Fundamental particles, decay chains, conservation laws (charge, lepton number,
 * baryon number, color charge), Feynman diagram vertex rules, cross-sections.
 */

#ifndef NIMCP_PARTICLE_PHYSICS_H
#define NIMCP_PARTICLE_PHYSICS_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/physics/nimcp_relativistic_physics.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PP_MAX_PARTICLES        256
#define PP_MAX_DECAYS           64
#define PP_MAX_INTERACTIONS     32
#define PP_MAX_NAME             24
#define PP_ALPHA_EM             (1.0f/137.036f) /* fine structure constant */
#define PP_ALPHA_S              0.118f          /* strong coupling at M_Z */
#define PP_GF                   1.166e-5f       /* Fermi constant (GeV^-2) */
#define PP_HBAR                 1.055e-34f      /* reduced Planck (J·s) */
#define PP_EV_TO_KG             1.783e-36f      /* 1 eV/c² in kg */
#define PP_GEV_TO_KG            1.783e-27f

typedef enum {
    PP_FERMION_QUARK = 0,
    PP_FERMION_LEPTON = 1,
    PP_BOSON_GAUGE = 2,
    PP_BOSON_SCALAR = 3,
} pp_particle_class_t;

typedef enum {
    PP_FORCE_EM = 0,            /* electromagnetic (photon) */
    PP_FORCE_WEAK = 1,          /* weak (W±, Z) */
    PP_FORCE_STRONG = 2,        /* strong (gluon) */
    PP_FORCE_GRAVITY = 3,       /* gravitational (graviton?) */
    PP_FORCE_HIGGS = 4,         /* Yukawa coupling */
} pp_force_t;

typedef enum {
    /* Quarks */
    PP_UP=0, PP_DOWN, PP_CHARM, PP_STRANGE, PP_TOP, PP_BOTTOM,
    /* Leptons */
    PP_ELECTRON, PP_MUON, PP_TAU, PP_ELECTRON_NEUTRINO, PP_MUON_NEUTRINO, PP_TAU_NEUTRINO,
    /* Gauge bosons */
    PP_PHOTON, PP_W_PLUS, PP_W_MINUS, PP_Z_BOSON, PP_GLUON,
    /* Scalar boson */
    PP_HIGGS,
    /* Composite */
    PP_PROTON, PP_NEUTRON, PP_PION_PLUS, PP_PION_MINUS, PP_PION_ZERO,
    PP_KAON_PLUS, PP_KAON_ZERO,
    PP_STANDARD_MODEL_COUNT
} pp_particle_id_t;

typedef struct {
    pp_particle_id_t    id;
    char                name[PP_MAX_NAME];
    char                symbol[8];
    pp_particle_class_t pclass;
    float               mass;           /* GeV/c² */
    float               charge;         /* in units of e */
    float               spin;           /* 0, 0.5, 1 */
    int8_t              baryon_number;   /* +1/3 per quark */
    int8_t              lepton_number;   /* +1 for leptons */
    int8_t              strangeness;
    int8_t              charm;
    int8_t              color_charge;    /* 0=colorless, 1=R, 2=G, 3=B, -1=anti-R */
    float               lifetime;       /* seconds (0 = stable) */
    float               width;          /* decay width (GeV) */
    bool                is_antiparticle;
    bool                active;
} pp_particle_def_t;

typedef struct {
    pp_particle_id_t    parent;
    pp_particle_id_t    products[4];
    uint32_t            num_products;
    float               branching_ratio;    /* [0..1] */
    float               partial_width;      /* GeV */
    pp_force_t          mediator;           /* which force mediates this decay */
    bool                active;
} pp_decay_channel_t;

typedef struct {
    pp_particle_id_t    initial[2];         /* colliding particles */
    pp_particle_id_t    final_state[4];     /* products */
    uint32_t            num_final;
    float               cross_section;      /* barns (1e-28 m²) */
    float               threshold_energy;   /* GeV (center of mass) */
    pp_force_t          mediator;
    bool                active;
} pp_interaction_t;

/* Live particle instance (in a simulation) */
typedef struct {
    pp_particle_id_t    type;
    rel_four_vector_t   four_momentum;      /* (E/c, px, py, pz) */
    wm_parietal_vec3_t  position;
    float               proper_time;
    bool                decayed;
    bool                active;
} pp_live_particle_t;

typedef struct {
    float       collision_energy;   /* GeV (center of mass) */
    float       dt;
    bool        enable_decays;
    bool        enable_conservation_checks;
} pp_sim_config_t;

typedef struct {
    uint64_t    step_count;
    uint64_t    total_decays;
    uint64_t    total_interactions;
    uint64_t    conservation_violations;
    float       total_energy;
    float       total_charge;
    int32_t     total_baryon_number;
    int32_t     total_lepton_number;
} pp_sim_stats_t;

typedef struct particle_physics_sim {
    pp_particle_def_t   definitions[PP_STANDARD_MODEL_COUNT];
    pp_decay_channel_t  decays[PP_MAX_DECAYS];
    uint32_t            num_decays;
    pp_interaction_t    interactions[PP_MAX_INTERACTIONS];
    uint32_t            num_interactions;
    pp_live_particle_t  particles[PP_MAX_PARTICLES];
    uint32_t            num_particles;
    pp_sim_config_t     config;
    pp_sim_stats_t      stats;
    float               time;
    bool                initialized;
} particle_physics_sim_t;

particle_physics_sim_t* particle_physics_create(const pp_sim_config_t* config);
void particle_physics_destroy(particle_physics_sim_t* sim);

/** Load the Standard Model particle table */
void particle_physics_load_standard_model(particle_physics_sim_t* sim);
/** Add a decay channel */
uint32_t particle_physics_add_decay(particle_physics_sim_t* sim, const pp_decay_channel_t* d);
/** Add an interaction (scattering process) */
uint32_t particle_physics_add_interaction(particle_physics_sim_t* sim, const pp_interaction_t* i);
/** Inject a live particle into the simulation */
uint32_t particle_physics_inject(particle_physics_sim_t* sim, pp_particle_id_t type,
                                  rel_four_vector_t four_momentum, wm_parietal_vec3_t position);
/** Step the simulation (propagate, decay, interact) */
int particle_physics_step(particle_physics_sim_t* sim, float dt);

/** Check conservation laws for a process: returns 0 if all conserved */
int particle_physics_check_conservation(const particle_physics_sim_t* sim,
                                         const pp_particle_id_t* initial, uint32_t n_initial,
                                         const pp_particle_id_t* final_state, uint32_t n_final);

/** Invariant mass of a particle system: M² = (Σp)² */
float particle_physics_invariant_mass(const rel_four_vector_t* momenta, uint32_t n);
/** Center of mass energy: √s = √((p₁+p₂)²) */
float particle_physics_com_energy(rel_four_vector_t p1, rel_four_vector_t p2);
/** Decay: mean lifetime τ from width Γ: τ = ℏ/Γ */
float particle_physics_lifetime_from_width(float width_gev);
/** De Broglie wavelength: λ = h/p */
float particle_physics_de_broglie(float momentum_gev);
/** Compton wavelength: λ_C = h/(mc) */
float particle_physics_compton_wavelength(float mass_gev);

pp_sim_config_t particle_physics_default_config(void);
pp_sim_stats_t particle_physics_get_stats(const particle_physics_sim_t* sim);

#ifdef __cplusplus
}
#endif
#endif
