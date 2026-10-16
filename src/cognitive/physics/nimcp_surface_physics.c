/**
 * @file nimcp_surface_physics.c
 * @brief Surface Physics — interfacial phenomena, wetting, capillarity, tribology
 *
 * WHAT: Young-Laplace, Young's equation, Marangoni flow, Fresnel optics, friction
 * WHY:  Everyday surface reasoning: beading, spreading, sliding, reflecting
 * HOW:  Analytical formulae + time-stepping for droplet/film dynamics
 */

#include "cognitive/physics/nimcp_surface_physics.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "SURFACE_PHYSICS"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * Public API — Creation / Destruction
 * ============================================================================ */

surf_phys_config_t surface_physics_default_config(void) {
    return (surf_phys_config_t){
        .ambient_temperature = 293.15f,
        .ambient_pressure = 101325.0f,
        .gravity = 9.81f,
        .enable_marangoni = true,
        .enable_evaporation = true,
        .enable_heat_transfer = true,
        .enable_optical = true,
    };
}

surface_physics_sim_t* surface_physics_create(const surf_phys_config_t* config) {
    surf_phys_config_t cfg = config ? *config : surface_physics_default_config();
    surface_physics_sim_t* sim = nimcp_calloc(1, sizeof(*sim));
    if (!sim) return NULL;
    sim->config = cfg;
    sim->initialized = true;
    LOG_INFO(LOG_TAG, "Surface physics created: T=%.1fK, Marangoni=%s, optical=%s",
             cfg.ambient_temperature,
             cfg.enable_marangoni ? "yes" : "no",
             cfg.enable_optical ? "yes" : "no");
    return sim;
}

void surface_physics_destroy(surface_physics_sim_t* sim) {
    if (!sim) return;
    nimcp_free(sim);
}

/* ============================================================================
 * Material & Interface Management
 * ============================================================================ */

uint32_t surface_physics_add_material(surface_physics_sim_t* sim, const surf_material_t* mat) {
    if (!sim || !mat || sim->num_materials >= SURF_MAX_MATERIALS) return UINT32_MAX;
    uint32_t id = sim->num_materials;
    sim->materials[id] = *mat;
    sim->materials[id].id = id;
    sim->materials[id].active = true;
    sim->num_materials = id + 1;
    return id;
}

uint32_t surface_physics_create_interface(surface_physics_sim_t* sim,
                                           uint32_t material_a, uint32_t material_b,
                                           float area) {
    if (!sim || material_a >= sim->num_materials || material_b >= sim->num_materials)
        return UINT32_MAX;
    if (sim->num_interfaces >= SURF_MAX_INTERFACES) return UINT32_MAX;

    surf_material_t* a = &sim->materials[material_a];
    surf_material_t* b = &sim->materials[material_b];

    uint32_t id = sim->num_interfaces;
    surf_interface_t* iface = &sim->interfaces[id];
    memset(iface, 0, sizeof(*iface));
    iface->id = id;
    iface->material_a = material_a;
    iface->material_b = material_b;
    iface->area = area;
    iface->temperature = sim->config.ambient_temperature;

    /* Compute interfacial energy using geometric mean approximation:
     * γ_AB ≈ γ_A + γ_B - 2√(γ_A · γ_B) (Good-Girifalco) */
    float ga = a->surface_energy > 0 ? a->surface_energy : 0;
    float gb = b->surface_energy > 0 ? b->surface_energy : 0;
    iface->interfacial_energy = ga + gb - 2.0f * sqrtf(ga * gb);
    if (iface->interfacial_energy < 0) iface->interfacial_energy = 0;

    /* Contact angle via Young's equation: cos θ = (γ_SG - γ_SL) / γ_LG
     * If A is solid and B is liquid: γ_SG = A.surface_energy, γ_LG = B.surface_tension */
    if (a->phase == SURF_TYPE_SOLID && b->phase == SURF_TYPE_LIQUID) {
        float gamma_sg = a->surface_energy;
        float gamma_sl = iface->interfacial_energy;
        float gamma_lg = b->surface_tension > 0 ? b->surface_tension : 0.072f;
        float cos_theta = (gamma_sg - gamma_sl) / gamma_lg;
        cos_theta = cos_theta < -1.0f ? -1.0f : (cos_theta > 1.0f ? 1.0f : cos_theta);
        iface->contact_angle = acosf(cos_theta);
    } else {
        iface->contact_angle = (float)(M_PI / 2.0);  /* default 90° */
    }

    /* Friction from material roughness (simplified Coulomb model) */
    float avg_roughness = (a->roughness + b->roughness) * 0.5f;
    iface->friction_static = 0.3f + 2.0f * avg_roughness * 1e3f;  /* rough heuristic */
    if (iface->friction_static > 1.5f) iface->friction_static = 1.5f;
    iface->friction_kinetic = iface->friction_static * 0.7f;

    /* Adhesion: Dupré equation W_A = γ_A + γ_B - γ_AB */
    iface->adhesion_energy = ga + gb - iface->interfacial_energy;

    iface->active = true;
    sim->num_interfaces = id + 1;
    return id;
}

uint32_t surface_physics_add_droplet(surface_physics_sim_t* sim,
                                      uint32_t liquid_id, uint32_t surface_id,
                                      wm_parietal_vec3_t position, float volume) {
    if (!sim || sim->num_droplets >= SURF_MAX_DROPLETS) return UINT32_MAX;
    if (liquid_id >= sim->num_materials || surface_id >= sim->num_materials) return UINT32_MAX;

    uint32_t id = sim->num_droplets;
    surf_droplet_t* d = &sim->droplets[id];
    memset(d, 0, sizeof(*d));
    d->id = id;
    d->position = position;
    d->volume = volume;
    d->liquid_id = liquid_id;
    d->surface_id = surface_id;

    /* Compute contact geometry from Young's equation */
    /* Find or create interface */
    float contact_angle = (float)(M_PI / 3.0);  /* default 60° */
    for (uint32_t i = 0; i < sim->num_interfaces; i++) {
        surf_interface_t* iface = &sim->interfaces[i];
        if (iface->active &&
            ((iface->material_a == surface_id && iface->material_b == liquid_id) ||
             (iface->material_b == surface_id && iface->material_a == liquid_id))) {
            contact_angle = iface->contact_angle;
            break;
        }
    }
    d->contact_angle = contact_angle;

    /* Spherical cap geometry: V = πh²(3R-h)/3, where R = contact_radius/sin(θ) */
    /* Simplified: for small droplets, approximate as sphere sector */
    float theta = contact_angle;
    /* R³ ≈ 3V / (π(2-3cos θ+cos³θ)) */
    float cos_t = cosf(theta);
    float denom = (float)(M_PI) * (2.0f - 3.0f * cos_t + cos_t * cos_t * cos_t);
    if (fabsf(denom) < 1e-10f) denom = 1e-10f;
    float R = cbrtf(3.0f * volume / denom);
    d->contact_radius = R * sinf(theta);
    d->height = R * (1.0f - cos_t);

    d->active = true;
    sim->num_droplets = id + 1;
    return id;
}

uint32_t surface_physics_add_film(surface_physics_sim_t* sim, const surf_thin_film_t* film) {
    if (!sim || !film || sim->num_films >= SURF_MAX_FILMS) return UINT32_MAX;
    uint32_t id = sim->num_films;
    sim->films[id] = *film;
    sim->films[id].id = id;
    sim->films[id].active = true;
    sim->num_films = id + 1;
    return id;
}

/* ============================================================================
 * Analytical Physics Functions
 * ============================================================================ */

float surface_physics_capillary_pressure(float surface_tension, float R1, float R2) {
    float inv_R1 = (fabsf(R1) > 1e-12f) ? 1.0f / R1 : 0;
    float inv_R2 = (fabsf(R2) > 1e-12f) ? 1.0f / R2 : 0;
    return surface_tension * (inv_R1 + inv_R2);
}

float surface_physics_contact_angle(float gamma_sg, float gamma_sl, float gamma_lg) {
    if (gamma_lg < 1e-12f) return (float)(M_PI / 2.0);
    float cos_theta = (gamma_sg - gamma_sl) / gamma_lg;
    cos_theta = cos_theta < -1.0f ? -1.0f : (cos_theta > 1.0f ? 1.0f : cos_theta);
    return acosf(cos_theta);
}

float surface_physics_capillary_rise(float surface_tension, float contact_angle,
                                      float density, float gravity, float tube_radius) {
    if (density < 1e-10f || gravity < 1e-10f || tube_radius < 1e-10f) return 0;
    return 2.0f * surface_tension * cosf(contact_angle) / (density * gravity * tube_radius);
}

float surface_physics_marangoni_velocity(float tension_gradient, float viscosity,
                                          float thickness) {
    /* Marangoni flow: v = (dγ/dx) · h / (2μ) — linear velocity profile */
    if (viscosity < 1e-12f || thickness < 1e-12f) return 0;
    return tension_gradient * thickness / (2.0f * viscosity);
}

float surface_physics_heat_transfer(const surface_physics_sim_t* sim,
                                     uint32_t interface_id) {
    if (!sim || interface_id >= sim->num_interfaces) return 0;
    const surf_interface_t* iface = &sim->interfaces[interface_id];
    const surf_material_t* a = &sim->materials[iface->material_a];
    const surf_material_t* b = &sim->materials[iface->material_b];

    /* Fourier's law: q = k_eff · ΔT / L_eff
     * Use harmonic mean of conductivities, assume unit thickness */
    float k_eff = 2.0f * a->thermal_conductivity * b->thermal_conductivity /
                  (a->thermal_conductivity + b->thermal_conductivity + 1e-12f);
    float dT = iface->temperature - sim->config.ambient_temperature;
    return k_eff * dT * iface->area;
}

float surface_physics_radiative_flux(float emissivity, float T1, float T2) {
    /* Stefan-Boltzmann: q = εσ(T₁⁴ - T₂⁴) */
    double t1_4 = (double)T1 * T1 * T1 * T1;
    double t2_4 = (double)T2 * T2 * T2 * T2;
    return emissivity * SURF_STEFAN_BOLTZMANN * (float)(t1_4 - t2_4);
}

float surface_physics_fresnel_normal(float n1, float n2) {
    /* R = ((n₁-n₂)/(n₁+n₂))² */
    float ratio = (n1 - n2) / (n1 + n2);
    return ratio * ratio;
}

float surface_physics_fresnel_s(float n1, float n2, float angle_incidence) {
    /* R_s = |( n₁ cos θ_i - n₂ cos θ_t ) / ( n₁ cos θ_i + n₂ cos θ_t )|² */
    float cos_i = cosf(angle_incidence);
    float sin_i = sinf(angle_incidence);
    float sin_t2 = (n1 / n2) * (n1 / n2) * sin_i * sin_i;
    if (sin_t2 >= 1.0f) return 1.0f;  /* total internal reflection */
    float cos_t = sqrtf(1.0f - sin_t2);
    float num = n1 * cos_i - n2 * cos_t;
    float den = n1 * cos_i + n2 * cos_t;
    return (num * num) / (den * den + 1e-12f);
}

float surface_physics_fresnel_p(float n1, float n2, float angle_incidence) {
    /* R_p = |( n₂ cos θ_i - n₁ cos θ_t ) / ( n₂ cos θ_i + n₁ cos θ_t )|² */
    float cos_i = cosf(angle_incidence);
    float sin_i = sinf(angle_incidence);
    float sin_t2 = (n1 / n2) * (n1 / n2) * sin_i * sin_i;
    if (sin_t2 >= 1.0f) return 1.0f;
    float cos_t = sqrtf(1.0f - sin_t2);
    float num = n2 * cos_i - n1 * cos_t;
    float den = n2 * cos_i + n1 * cos_t;
    return (num * num) / (den * den + 1e-12f);
}

float surface_physics_critical_angle(float n1, float n2) {
    if (n1 <= n2) return (float)(M_PI / 2.0);  /* no TIR if n1 <= n2 */
    float ratio = n2 / n1;
    if (ratio > 1.0f) ratio = 1.0f;   /* defensive: asinf domain is [-1,1] */
    if (ratio < 0.0f) ratio = 0.0f;
    return asinf(ratio);
}

float surface_physics_friction_force(const surface_physics_sim_t* sim,
                                      uint32_t interface_id, float normal_force,
                                      bool kinetic) {
    if (!sim || interface_id >= sim->num_interfaces) return 0;
    const surf_interface_t* iface = &sim->interfaces[interface_id];
    float mu = kinetic ? iface->friction_kinetic : iface->friction_static;
    return mu * fabsf(normal_force);
}

bool surface_physics_droplet_equilibrium(const surface_physics_sim_t* sim, uint32_t droplet_id) {
    if (!sim || droplet_id >= sim->num_droplets) return false;
    const surf_droplet_t* d = &sim->droplets[droplet_id];
    return d->active && d->velocity < 0.001f && d->evaporation_rate < 1e-12f;
}

/* ============================================================================
 * Simulation Step
 * ============================================================================ */

int surface_physics_step(surface_physics_sim_t* sim, float dt) {
    if (!sim || !sim->initialized) return -1;

    /* Update droplets */
    for (uint32_t i = 0; i < sim->num_droplets; i++) {
        surf_droplet_t* d = &sim->droplets[i];
        if (!d->active) continue;

        /* Evaporation: dV/dt ∝ surface_area * (T - T_boil_approx) */
        if (sim->config.enable_evaporation && d->volume > 0) {
            float T = sim->config.ambient_temperature;
            /* Simplified: evaporation rate proportional to exposed area and temperature */
            float exposed_area = (float)(M_PI) * d->contact_radius * d->contact_radius;
            float evap_rate = exposed_area * 1e-9f * (T / 373.15f);  /* crude model */
            d->evaporation_rate = evap_rate;
            d->volume -= evap_rate * dt;
            if (d->volume <= 0) {
                d->volume = 0;
                d->active = false;
                sim->stats.total_evaporated += evap_rate * dt;
                continue;
            }
            /* Recalculate geometry */
            float theta = d->contact_angle;
            float cos_t = cosf(theta);
            float denom = (float)(M_PI) * (2.0f - 3.0f * cos_t + cos_t * cos_t * cos_t);
            if (fabsf(denom) < 1e-10f) denom = 1e-10f;
            float R = cbrtf(3.0f * d->volume / denom);
            d->contact_radius = R * sinf(theta);
            d->height = R * (1.0f - cos_t);
        }

        /* Gravity-driven sliding on inclined surfaces */
        /* Simplified: droplet slides if gravity component > friction */
        /* For now, just damp any existing velocity */
        d->velocity *= 0.99f;

        d->position.x += d->velocity * dt;
    }

    /* Update thin films (Marangoni flow) */
    if (sim->config.enable_marangoni) {
        for (uint32_t i = 0; i < sim->num_films; i++) {
            surf_thin_film_t* f = &sim->films[i];
            if (!f->active || f->material_id >= sim->num_materials) continue;
            float visc = sim->materials[f->material_id].viscosity;
            if (visc < 1e-12f) visc = 1e-3f;
            f->flow_velocity = surface_physics_marangoni_velocity(
                f->tension_gradient, visc, f->thickness);
            if (f->flow_velocity > sim->stats.max_marangoni_velocity)
                sim->stats.max_marangoni_velocity = f->flow_velocity;
        }
    }

    /* Update interface heat transfer */
    if (sim->config.enable_heat_transfer) {
        for (uint32_t i = 0; i < sim->num_interfaces; i++) {
            surf_interface_t* iface = &sim->interfaces[i];
            if (!iface->active) continue;
            iface->heat_flux = surface_physics_heat_transfer(sim, i) / (iface->area + 1e-12f);
            sim->stats.total_heat_transferred += fabsf(iface->heat_flux) * iface->area * dt;
        }
    }

    /* Compute total surface energy */
    float total_se = 0;
    for (uint32_t i = 0; i < sim->num_interfaces; i++) {
        if (sim->interfaces[i].active)
            total_se += sim->interfaces[i].interfacial_energy * sim->interfaces[i].area;
    }
    sim->stats.total_surface_energy = total_se;

    sim->time += dt;
    sim->stats.step_count++;
    sim->stats.active_interfaces = sim->num_interfaces;
    sim->stats.active_droplets = 0;
    for (uint32_t i = 0; i < sim->num_droplets; i++)
        if (sim->droplets[i].active) sim->stats.active_droplets++;

    return 0;
}

/* ============================================================================
 * Common Materials
 * ============================================================================ */

void surface_physics_load_common_materials(surface_physics_sim_t* sim) {
    if (!sim) return;

    struct { const char* name; surf_phase_t phase; float se; float st; float visc;
             float dens; float k; float cp; float emiss; float n; float rough;
             bool hydrophobic; } common[] = {
        {"water",    SURF_TYPE_LIQUID, 0.072f, 0.0728f, 1.002e-3f, 998.0f, 0.606f, 4186.0f, 0.96f, 1.333f, 0, false},
        {"glass",    SURF_TYPE_SOLID,  0.30f,  0,       0,          2500.0f, 1.05f, 840.0f,  0.92f, 1.52f,  1e-7f, false},
        {"steel",    SURF_TYPE_SOLID,  1.0f,   0,       0,          7800.0f, 50.0f, 500.0f,  0.3f,  2.5f,   1e-6f, false},
        {"teflon",   SURF_TYPE_SOLID,  0.018f, 0,       0,          2150.0f, 0.25f, 1000.0f, 0.04f, 1.35f,  5e-7f, true},
        {"wood",     SURF_TYPE_SOLID,  0.05f,  0,       0,          600.0f,  0.17f, 1700.0f, 0.9f,  1.5f,   1e-4f, false},
        {"air",      SURF_TYPE_GAS,    0,      0,       1.8e-5f,    1.225f,  0.026f, 1005.0f, 0,    1.0003f, 0, false},
        {"oil",      SURF_TYPE_LIQUID, 0.032f, 0.032f,  0.03f,      900.0f,  0.15f, 2000.0f, 0.94f, 1.47f,  0, true},
        {"rubber",   SURF_TYPE_SOLID,  0.025f, 0,       0,          1100.0f, 0.16f, 2010.0f, 0.86f, 1.52f,  1e-5f, true},
        {"ice",      SURF_TYPE_SOLID,  0.109f, 0,       0,          917.0f,  2.22f, 2090.0f, 0.97f, 1.31f,  1e-7f, false},
        {"copper",   SURF_TYPE_SOLID,  1.8f,   0,       0,          8960.0f, 401.0f, 385.0f, 0.03f, 0.64f,  1e-6f, false},
        {"diamond",  SURF_TYPE_SOLID,  5.3f,   0,       0,          3510.0f, 2200.0f, 520.0f, 0.02f, 2.42f, 1e-9f, false},
        {"mercury",  SURF_TYPE_LIQUID, 0.487f, 0.487f,  1.53e-3f,   13534.0f, 8.3f, 140.0f, 0.1f,  1.62f,  0, true},
    };

    for (uint32_t i = 0; i < sizeof(common) / sizeof(common[0]); i++) {
        surf_material_t mat = {0};
        strncpy(mat.name, common[i].name, sizeof(mat.name) - 1);
        mat.phase = common[i].phase;
        mat.surface_energy = common[i].se;
        mat.surface_tension = common[i].st;
        mat.viscosity = common[i].visc;
        mat.density = common[i].dens;
        mat.thermal_conductivity = common[i].k;
        mat.specific_heat = common[i].cp;
        mat.emissivity = common[i].emiss;
        mat.refractive_index = common[i].n;
        mat.roughness = common[i].rough;
        mat.hydrophobic = common[i].hydrophobic;
        surface_physics_add_material(sim, &mat);
    }
}

surf_phys_stats_t surface_physics_get_stats(const surface_physics_sim_t* sim) {
    if (!sim) return (surf_phys_stats_t){0};
    return sim->stats;
}
