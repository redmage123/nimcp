/**
 * @file nimcp_heat_transfer.c
 * @brief Heat Transfer simulation — conduction, convection, radiation
 *
 * WHAT: 3D heat conduction via Fourier's law with explicit Euler integration,
 *       convective (Newton) and radiative (Stefan-Boltzmann) boundary conditions,
 *       multi-material support.
 * WHY:  Reasoning about thermal systems, insulation, cooling, heat exchangers.
 * HOW:  Finite-difference discretization, material properties per cell,
 *       double-buffered explicit time step.
 */

#include "cognitive/physics/nimcp_heat_transfer.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "HEAT_TRANSFER"

/* ============================================================================
 * Grid Helpers
 * ============================================================================ */

static inline uint32_t idx3(uint32_t nx, uint32_t ny, uint32_t ix, uint32_t iy, uint32_t iz) {
    return (iz * ny + iy) * nx + ix;
}

static bool alloc_scalar_field(ht_scalar_field_t* f, uint32_t n, float dx) {
    f->nx = f->ny = f->nz = n;
    f->dx = f->dy = f->dz = dx;
    f->data = nimcp_calloc(n * n * n, sizeof(float));
    return f->data != NULL;
}

static bool alloc_material_field(ht_material_field_t* f, uint32_t n) {
    f->nx = f->ny = f->nz = n;
    f->data = nimcp_calloc(n * n * n, sizeof(uint8_t));
    return f->data != NULL;
}

static void free_scalar_field(ht_scalar_field_t* f) {
    if (f->data) { nimcp_free(f->data); f->data = NULL; }
}

/* ============================================================================
 * Built-in Materials
 * ============================================================================ */

ht_material_t ht_builtin_material(ht_material_type_t type) {
    ht_material_t m;
    memset(&m, 0, sizeof(m));
    m.type = type;
    m.emissivity = 0.9f;

    switch (type) {
        case HT_MAT_COPPER:
            m.conductivity = HT_K_COPPER; m.specific_heat = HT_CP_COPPER;
            m.density = HT_RHO_COPPER; snprintf(m.name, sizeof(m.name), "Copper");
            break;
        case HT_MAT_ALUMINUM:
            m.conductivity = HT_K_ALUMINUM; m.specific_heat = HT_CP_ALUMINUM;
            m.density = HT_RHO_ALUMINUM; snprintf(m.name, sizeof(m.name), "Aluminum");
            break;
        case HT_MAT_STEEL:
            m.conductivity = HT_K_STEEL; m.specific_heat = HT_CP_STEEL;
            m.density = HT_RHO_STEEL; snprintf(m.name, sizeof(m.name), "Steel");
            break;
        case HT_MAT_GLASS:
            m.conductivity = HT_K_GLASS; m.specific_heat = HT_CP_GLASS;
            m.density = 2500.0f; snprintf(m.name, sizeof(m.name), "Glass");
            break;
        case HT_MAT_WOOD:
            m.conductivity = HT_K_WOOD; m.specific_heat = 1700.0f;
            m.density = 600.0f; snprintf(m.name, sizeof(m.name), "Wood");
            break;
        case HT_MAT_WATER:
            m.conductivity = HT_K_WATER; m.specific_heat = HT_CP_WATER;
            m.density = HT_RHO_WATER; snprintf(m.name, sizeof(m.name), "Water");
            break;
        case HT_MAT_AIR:
            m.conductivity = HT_K_AIR; m.specific_heat = HT_CP_AIR;
            m.density = HT_RHO_AIR; snprintf(m.name, sizeof(m.name), "Air");
            break;
        case HT_MAT_CONCRETE:
            m.conductivity = HT_K_CONCRETE; m.specific_heat = HT_CP_CONCRETE;
            m.density = HT_RHO_CONCRETE; snprintf(m.name, sizeof(m.name), "Concrete");
            break;
        default:
            m.conductivity = 1.0f; m.specific_heat = 1000.0f;
            m.density = 1000.0f; snprintf(m.name, sizeof(m.name), "Custom");
            break;
    }
    return m;
}

/* ============================================================================
 * Default Config
 * ============================================================================ */

ht_config_t ht_default_config(void) {
    ht_config_t c;
    memset(&c, 0, sizeof(c));
    c.grid_dim      = 32;
    c.cell_size     = 0.01f;
    c.dt            = 0.001f;
    c.initial_temp  = 293.15f;  /* 20C in Kelvin */
    c.default_material = HT_MAT_ALUMINUM;
    c.enable_radiation  = false;
    c.enable_convection = true;

    /* Default all boundaries to convective */
    for (int i = 0; i < 6; i++) {
        c.boundary[i].type      = HT_BC_CONVECTIVE;
        c.boundary[i].h_conv    = 10.0f;       /* W/(m^2*K) natural convection in air */
        c.boundary[i].t_ambient = 293.15f;
        c.boundary[i].emissivity = 0.9f;
    }
    return c;
}

/* ============================================================================
 * Create / Destroy
 * ============================================================================ */

heat_transfer_sim_t* ht_create(const ht_config_t* config) {
    ht_config_t cfg = config ? *config : ht_default_config();
    if (cfg.grid_dim > HT_MAX_GRID_DIM) cfg.grid_dim = HT_MAX_GRID_DIM;

    heat_transfer_sim_t* sim = nimcp_calloc(1, sizeof(heat_transfer_sim_t));
    if (!sim) return NULL;

    sim->config = cfg;
    uint32_t n = cfg.grid_dim;
    float dx = cfg.cell_size;

    if (!alloc_scalar_field(&sim->temperature, n, dx) ||
        !alloc_scalar_field(&sim->temp_next, n, dx) ||
        !alloc_material_field(&sim->material_map, n)) {
        ht_destroy(sim);
        return NULL;
    }

    /* Initialize temperature field */
    uint32_t total = n * n * n;
    for (uint32_t i = 0; i < total; i++) {
        sim->temperature.data[i] = cfg.initial_temp;
        sim->temp_next.data[i] = cfg.initial_temp;
    }

    /* Register default material */
    ht_material_t def_mat = ht_builtin_material(cfg.default_material);
    sim->materials[0] = def_mat;
    sim->num_materials = 1;

    sim->initialized = true;
    LOG_INFO(LOG_TAG, "Created %ux%ux%u heat transfer sim (T0=%.1fK, mat=%s)",
             n, n, n, cfg.initial_temp, def_mat.name);
    return sim;
}

void ht_destroy(heat_transfer_sim_t* sim) {
    if (!sim) return;
    free_scalar_field(&sim->temperature);
    free_scalar_field(&sim->temp_next);
    if (sim->material_map.data) nimcp_free(sim->material_map.data);
    nimcp_free(sim);
}

/* ============================================================================
 * Boundary Condition Application
 * ============================================================================ */

static void apply_boundary_face(heat_transfer_sim_t* sim, int face,
                                 uint32_t ix, uint32_t iy, uint32_t iz,
                                 uint32_t interior_ix, uint32_t interior_iy,
                                 uint32_t interior_iz) {
    uint32_t n = sim->config.grid_dim;
    float* T = sim->temp_next.data;
    ht_boundary_config_t* bc = &sim->config.boundary[face];
    uint32_t bi = idx3(n, n, ix, iy, iz);
    uint32_t ii = idx3(n, n, interior_ix, interior_iy, interior_iz);

    switch (bc->type) {
        case HT_BC_ISOTHERMAL:
            T[bi] = bc->temperature;
            break;
        case HT_BC_ADIABATIC:
            T[bi] = T[ii];     /* zero gradient */
            break;
        case HT_BC_CONVECTIVE: {
            /* Ghost cell: T_ghost = T_interior - (h*dx/k)*(T_interior - T_amb) */
            uint8_t mi = sim->material_map.data[ii];
            float k = sim->materials[mi].conductivity;
            float dx = sim->config.cell_size;
            if (k < 1e-6f) { T[bi] = T[ii]; break; }  /* insulator: adiabatic */
            float ratio = bc->h_conv * dx / k;
            T[bi] = T[ii] - ratio * (T[ii] - bc->t_ambient);
            break;
        }
        case HT_BC_RADIATIVE: {
            float Ts = sim->temperature.data[ii];
            float Ta = bc->t_ambient;
            /* Linearized: q = eps*sigma*4*Tavg^3*(Ts-Ta) treated as convective h_rad */
            float Tavg = 0.5f * (Ts + Ta);
            float h_rad = bc->emissivity * HT_STEFAN_BOLTZMANN * 4.0f * Tavg * Tavg * Tavg;
            uint8_t mi = sim->material_map.data[ii];
            float k = sim->materials[mi].conductivity;
            float dx = sim->config.cell_size;
            float ratio = h_rad * dx / (k + 1e-20f);
            T[bi] = T[ii] - ratio * (T[ii] - Ta);
            break;
        }
        case HT_BC_MIXED: {
            float Ts = sim->temperature.data[ii];
            float Ta = bc->t_ambient;
            float Tavg = 0.5f * (Ts + Ta);
            float h_rad = bc->emissivity * HT_STEFAN_BOLTZMANN * 4.0f * Tavg * Tavg * Tavg;
            float h_total = bc->h_conv + h_rad;
            uint8_t mi = sim->material_map.data[ii];
            float k = sim->materials[mi].conductivity;
            float dx = sim->config.cell_size;
            float ratio = h_total * dx / (k + 1e-20f);
            T[bi] = T[ii] - ratio * (T[ii] - Ta);
            break;
        }
    }
}

/* ============================================================================
 * Step: Explicit Euler for heat equation
 * ============================================================================ */

int ht_step(heat_transfer_sim_t* sim, float dt) {
    if (!sim || !sim->initialized) return -1;

    uint32_t n = sim->config.grid_dim;
    float dx = sim->config.cell_size;
    float inv_dx2 = 1.0f / (dx * dx);
    float* T = sim->temperature.data;
    float* Tn = sim->temp_next.data;

    if (dt <= 0.0f) dt = sim->config.dt;

    /* Interior update: rho*Cp * dT/dt = k * laplacian(T) + Q_gen */
    for (uint32_t iz = 1; iz < n - 1; iz++) {
        for (uint32_t iy = 1; iy < n - 1; iy++) {
            for (uint32_t ix = 1; ix < n - 1; ix++) {
                uint32_t ci = idx3(n, n, ix, iy, iz);
                uint8_t mi = sim->material_map.data[ci];
                ht_material_t* mat = &sim->materials[mi];
                float k = mat->conductivity;
                float rho_cp = mat->density * mat->specific_heat;
                if (rho_cp < 1e-10f) rho_cp = 1e-10f;

                /* 7-point stencil Laplacian */
                float lap = (T[idx3(n,n,ix+1,iy,iz)] + T[idx3(n,n,ix-1,iy,iz)]
                           + T[idx3(n,n,ix,iy+1,iz)] + T[idx3(n,n,ix,iy-1,iz)]
                           + T[idx3(n,n,ix,iy,iz+1)] + T[idx3(n,n,ix,iy,iz-1)]
                           - 6.0f * T[ci]) * inv_dx2;

                float dTdt = k * lap / rho_cp;

                /* Add volumetric heat sources */
                for (uint32_t si = 0; si < sim->num_sources; si++) {
                    ht_heat_source_t* src = &sim->sources[si];
                    if (!src->active) continue;
                    if (src->ix == ix && src->iy == iy && src->iz == iz) {
                        float dV = dx * dx * dx;
                        float q_vol = src->power / dV + src->power_density;
                        dTdt += q_vol / rho_cp;
                    }
                }

                Tn[ci] = T[ci] + dt * dTdt;
            }
        }
    }

    /* Apply boundary conditions on all 6 faces */
    for (uint32_t iy = 0; iy < n; iy++) {
        for (uint32_t iz = 0; iz < n; iz++) {
            apply_boundary_face(sim, 0, 0, iy, iz, 1, iy, iz);             /* -x */
            apply_boundary_face(sim, 1, n-1, iy, iz, n-2, iy, iz);         /* +x */
        }
    }
    for (uint32_t ix = 0; ix < n; ix++) {
        for (uint32_t iz = 0; iz < n; iz++) {
            apply_boundary_face(sim, 2, ix, 0, iz, ix, 1, iz);             /* -y */
            apply_boundary_face(sim, 3, ix, n-1, iz, ix, n-2, iz);         /* +y */
        }
    }
    for (uint32_t ix = 0; ix < n; ix++) {
        for (uint32_t iy = 0; iy < n; iy++) {
            apply_boundary_face(sim, 4, ix, iy, 0, ix, iy, 1);             /* -z */
            apply_boundary_face(sim, 5, ix, iy, n-1, ix, iy, n-2);         /* +z */
        }
    }

    /* Swap buffers */
    memcpy(T, Tn, n * n * n * sizeof(float));

    /* Update statistics */
    sim->time += dt;
    sim->stats.step_count++;
    sim->stats.time = sim->time;
    float max_t = -1e30f, min_t = 1e30f, sum_t = 0.0f;
    float max_grad = 0.0f, total_energy = 0.0f;
    float dV = dx * dx * dx;
    uint32_t total = n * n * n;
    for (uint32_t i = 0; i < total; i++) {
        if (T[i] > max_t) max_t = T[i];
        if (T[i] < min_t) min_t = T[i];
        sum_t += T[i];
        uint8_t mi = sim->material_map.data[i];
        total_energy += sim->materials[mi].density * sim->materials[mi].specific_heat * T[i] * dV;
    }
    sim->stats.max_temperature = max_t;
    sim->stats.min_temperature = min_t;
    sim->stats.avg_temperature = sum_t / (float)total;
    sim->stats.total_internal_energy = total_energy;

    /* Compute max gradient (interior only) */
    for (uint32_t iz = 1; iz < n - 1; iz++) {
        for (uint32_t iy = 1; iy < n - 1; iy++) {
            for (uint32_t ix = 1; ix < n - 1; ix++) {
                float dTdx = (T[idx3(n,n,ix+1,iy,iz)] - T[idx3(n,n,ix-1,iy,iz)]) / (2.0f * dx);
                float dTdy = (T[idx3(n,n,ix,iy+1,iz)] - T[idx3(n,n,ix,iy-1,iz)]) / (2.0f * dx);
                float dTdz = (T[idx3(n,n,ix,iy,iz+1)] - T[idx3(n,n,ix,iy,iz-1)]) / (2.0f * dx);
                float grad = sqrtf(dTdx*dTdx + dTdy*dTdy + dTdz*dTdz);
                if (grad > max_grad) max_grad = grad;
            }
        }
    }
    sim->stats.max_gradient = max_grad;

    return 0;
}

ht_stats_t ht_get_stats(const heat_transfer_sim_t* sim) {
    if (!sim) { ht_stats_t s; memset(&s, 0, sizeof(s)); return s; }
    return sim->stats;
}

/* ============================================================================
 * Setup Functions
 * ============================================================================ */

uint32_t ht_add_material(heat_transfer_sim_t* sim, const ht_material_t* mat) {
    if (!sim || sim->num_materials >= HT_MAX_MATERIALS) return UINT32_MAX;
    uint32_t idx = sim->num_materials;
    sim->materials[idx] = *mat;
    sim->num_materials++;
    return idx;
}

void ht_set_material_region(heat_transfer_sim_t* sim,
                             uint32_t x0, uint32_t y0, uint32_t z0,
                             uint32_t x1, uint32_t y1, uint32_t z1,
                             uint32_t mat_index) {
    if (!sim || !sim->initialized) return;
    uint32_t n = sim->config.grid_dim;
    if (mat_index >= sim->num_materials) return;
    if (x1 > n) x1 = n; if (y1 > n) y1 = n; if (z1 > n) z1 = n;
    for (uint32_t iz = z0; iz < z1; iz++)
        for (uint32_t iy = y0; iy < y1; iy++)
            for (uint32_t ix = x0; ix < x1; ix++)
                sim->material_map.data[idx3(n, n, ix, iy, iz)] = (uint8_t)mat_index;
}

uint32_t ht_add_heat_source(heat_transfer_sim_t* sim, const ht_heat_source_t* src) {
    if (!sim || sim->num_sources >= HT_MAX_HEAT_SOURCES) return UINT32_MAX;
    uint32_t idx = sim->num_sources;
    sim->sources[idx] = *src;
    sim->sources[idx].active = true;
    sim->num_sources++;
    return idx;
}

void ht_set_temperature(heat_transfer_sim_t* sim,
                         uint32_t ix, uint32_t iy, uint32_t iz, float temp_k) {
    if (!sim || !sim->initialized) return;
    uint32_t n = sim->config.grid_dim;
    if (ix >= n || iy >= n || iz >= n) return;
    sim->temperature.data[idx3(n, n, ix, iy, iz)] = temp_k;
}

float ht_get_temperature(const heat_transfer_sim_t* sim,
                          uint32_t ix, uint32_t iy, uint32_t iz) {
    if (!sim || !sim->initialized) return 0.0f;
    uint32_t n = sim->config.grid_dim;
    if (ix >= n || iy >= n || iz >= n) return 0.0f;
    return sim->temperature.data[idx3(n, n, ix, iy, iz)];
}

/* ============================================================================
 * Physics Computations
 * ============================================================================ */

float ht_thermal_resistance(float length, float conductivity, float area) {
    if (conductivity * area < 1e-30f) return 1e30f;
    return length / (conductivity * area);
}

float ht_biot_number(float h_conv, float char_length, float conductivity) {
    if (conductivity < 1e-30f) return 1e10f;
    return h_conv * char_length / conductivity;
}

float ht_nusselt_number(float h_conv, float length, float k_fluid) {
    if (k_fluid < 1e-30f) return 0.0f;
    return h_conv * length / k_fluid;
}

float ht_fin_efficiency(float h_conv, float perimeter, float conductivity,
                         float cross_area, float fin_length) {
    if (conductivity * cross_area < 1e-30f) return 0.0f;
    float m = sqrtf(h_conv * perimeter / (conductivity * cross_area));
    float mL = m * fin_length;
    if (mL < 1e-10f) return 1.0f;
    return tanhf(mL) / mL;
}

float ht_radiative_flux(float emissivity, float T1, float T2) {
    float T1_4 = T1 * T1 * T1 * T1;
    float T2_4 = T2 * T2 * T2 * T2;
    return emissivity * HT_STEFAN_BOLTZMANN * (T1_4 - T2_4);
}

float ht_thermal_diffusivity(float conductivity, float density, float specific_heat) {
    float denom = density * specific_heat;
    if (denom < 1e-30f) return 0.0f;
    return conductivity / denom;
}

float ht_lumped_time_constant(float density, float volume, float specific_heat,
                               float h_conv, float surface_area) {
    float denom = h_conv * surface_area;
    if (denom < 1e-30f) return 1e30f;
    return density * volume * specific_heat / denom;
}

/* ============================================================================
 * Legacy API Wrappers
 * ============================================================================ */

heat_transfer_sim_t* heat_transfer_create(const heat_transfer_config_t* c) { return ht_create(c); }
void heat_transfer_destroy(heat_transfer_sim_t* s) { ht_destroy(s); }
int heat_transfer_step(heat_transfer_sim_t* s, float dt) { return ht_step(s, dt); }
heat_transfer_config_t heat_transfer_default_config(void) { return ht_default_config(); }
heat_transfer_stats_t heat_transfer_get_stats(const heat_transfer_sim_t* s) { return ht_get_stats(s); }
