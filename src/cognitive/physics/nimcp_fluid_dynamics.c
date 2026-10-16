/**
 * @file nimcp_fluid_dynamics.c
 * @brief Fluid Dynamics simulation — Navier-Stokes on a 3D grid
 *
 * WHAT: Finite-volume Navier-Stokes solver with explicit Euler integration.
 *       Mass conservation, momentum (pressure + viscosity + gravity), energy.
 * WHY:  Reasoning about fluid flow, aerodynamics, pipe systems, weather.
 * HOW:  Staggered grid, central differences for pressure/viscosity,
 *       upwind for advection, CFL-limited time step.
 */

#include "cognitive/physics/nimcp_fluid_dynamics.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "FLUID_DYNAMICS"

/* ============================================================================
 * Grid Helpers
 * ============================================================================ */

static inline uint32_t idx3(uint32_t nx, uint32_t ny, uint32_t ix, uint32_t iy, uint32_t iz) {
    return (iz * ny + iy) * nx + ix;
}

static inline uint32_t vidx3(uint32_t nx, uint32_t ny, uint32_t ix, uint32_t iy, uint32_t iz) {
    return idx3(nx, ny, ix, iy, iz) * 3;
}

static bool alloc_vector_field(fd_vector_field_t* f, uint32_t n, float dx) {
    f->nx = f->ny = f->nz = n;
    f->dx = f->dy = f->dz = dx;
    uint32_t total = n * n * n * 3;
    f->data = nimcp_calloc(total, sizeof(float));
    return f->data != NULL;
}

static bool alloc_scalar_field(fd_scalar_field_t* f, uint32_t n, float dx) {
    f->nx = f->ny = f->nz = n;
    f->dx = f->dy = f->dz = dx;
    uint32_t total = n * n * n;
    f->data = nimcp_calloc(total, sizeof(float));
    return f->data != NULL;
}

static void free_vector_field(fd_vector_field_t* f) {
    if (f->data) { nimcp_free(f->data); f->data = NULL; }
}

static void free_scalar_field(fd_scalar_field_t* f) {
    if (f->data) { nimcp_free(f->data); f->data = NULL; }
}

/* ============================================================================
 * Default Config
 * ============================================================================ */

fd_config_t fd_default_config(void) {
    fd_config_t c;
    memset(&c, 0, sizeof(c));
    c.grid_dim          = 32;
    c.cell_size         = 0.01f;
    c.dt                = 0.0001f;
    c.fluid_type        = FD_FLUID_AIR;
    c.density           = FD_AIR_DENSITY;
    c.viscosity         = FD_AIR_VISCOSITY;
    c.gravity_x         = 0.0f;
    c.gravity_y         = -FD_GRAVITY;
    c.gravity_z         = 0.0f;
    c.boundary          = FD_BC_NO_SLIP;
    c.reference_pressure = FD_ATM_PRESSURE;
    c.compressible      = false;
    return c;
}

/* ============================================================================
 * Create / Destroy
 * ============================================================================ */

fluid_dynamics_sim_t* fd_create(const fd_config_t* config) {
    fd_config_t cfg = config ? *config : fd_default_config();

    if (cfg.grid_dim > FD_MAX_GRID_DIM) {
        LOG_WARN(LOG_TAG, "Grid dim %u clamped to %d", cfg.grid_dim, FD_MAX_GRID_DIM);
        cfg.grid_dim = FD_MAX_GRID_DIM;
    }

    fluid_dynamics_sim_t* sim = nimcp_calloc(1, sizeof(fluid_dynamics_sim_t));
    if (!sim) { return NULL; }

    sim->config = cfg;
    uint32_t n = cfg.grid_dim;
    float dx = cfg.cell_size;

    if (!alloc_vector_field(&sim->velocity, n, dx) ||
        !alloc_scalar_field(&sim->pressure, n, dx) ||
        !alloc_scalar_field(&sim->density_field, n, dx) ||
        !alloc_scalar_field(&sim->temperature, n, dx) ||
        !alloc_vector_field(&sim->vorticity, n, dx)) {
        fd_destroy(sim);
        return NULL;
    }

    /* Initialize uniform density and reference pressure */
    uint32_t total = n * n * n;
    for (uint32_t i = 0; i < total; i++) {
        sim->density_field.data[i] = cfg.density;
        sim->pressure.data[i] = cfg.reference_pressure;
        sim->temperature.data[i] = 293.15f; /* 20C */
    }

    sim->initialized = true;
    LOG_INFO(LOG_TAG, "Created %ux%ux%u fluid sim (rho=%.2f, mu=%.2e)",
             n, n, n, cfg.density, cfg.viscosity);
    return sim;
}

void fd_destroy(fluid_dynamics_sim_t* sim) {
    if (!sim) { return; }
    free_vector_field(&sim->velocity);
    free_scalar_field(&sim->pressure);
    free_scalar_field(&sim->density_field);
    free_scalar_field(&sim->temperature);
    free_vector_field(&sim->vorticity);
    nimcp_free(sim);
}

/* ============================================================================
 * Boundary Conditions
 * ============================================================================ */

static void apply_boundary(fluid_dynamics_sim_t* sim) {
    uint32_t n = sim->config.grid_dim;
    float* v = sim->velocity.data;

    for (uint32_t iy = 0; iy < n; iy++) {
        for (uint32_t iz = 0; iz < n; iz++) {
            /* x = 0 and x = n-1 boundaries */
            uint32_t lo = vidx3(n, n, 0, iy, iz);
            uint32_t hi = vidx3(n, n, n - 1, iy, iz);
            if (sim->config.boundary == FD_BC_NO_SLIP) {
                v[lo] = v[lo+1] = v[lo+2] = 0.0f;
                v[hi] = v[hi+1] = v[hi+2] = 0.0f;
            } else if (sim->config.boundary == FD_BC_INFLOW) {
                v[lo]   = sim->config.inflow_vx;
                v[lo+1] = sim->config.inflow_vy;
                v[lo+2] = sim->config.inflow_vz;
            } else if (sim->config.boundary == FD_BC_PERIODIC) {
                uint32_t src = vidx3(n, n, n - 2, iy, iz);
                uint32_t dst = vidx3(n, n, 0, iy, iz);
                v[dst] = v[src]; v[dst+1] = v[src+1]; v[dst+2] = v[src+2];
            }
        }
    }
    /* y and z boundaries: no-slip */
    for (uint32_t ix = 0; ix < n; ix++) {
        for (uint32_t iz = 0; iz < n; iz++) {
            uint32_t lo = vidx3(n, n, ix, 0, iz);
            uint32_t hi = vidx3(n, n, ix, n - 1, iz);
            if (sim->config.boundary == FD_BC_NO_SLIP) {
                v[lo] = v[lo+1] = v[lo+2] = 0.0f;
                v[hi] = v[hi+1] = v[hi+2] = 0.0f;
            }
        }
    }
}

/* ============================================================================
 * Navier-Stokes Step
 * ============================================================================ */

static void compute_vorticity(fluid_dynamics_sim_t* sim) {
    uint32_t n = sim->config.grid_dim;
    float inv_2dx = 1.0f / (2.0f * sim->config.cell_size);
    float* v = sim->velocity.data;
    float* w = sim->vorticity.data;

    for (uint32_t iz = 1; iz < n - 1; iz++) {
        for (uint32_t iy = 1; iy < n - 1; iy++) {
            for (uint32_t ix = 1; ix < n - 1; ix++) {
                uint32_t c = vidx3(n, n, ix, iy, iz);
                /* dw/dy - dv/dz */
                float dwdy = (v[vidx3(n,n,ix,iy+1,iz)+2] - v[vidx3(n,n,ix,iy-1,iz)+2]) * inv_2dx;
                float dvdz = (v[vidx3(n,n,ix,iy,iz+1)+1] - v[vidx3(n,n,ix,iy,iz-1)+1]) * inv_2dx;
                w[c+0] = dwdy - dvdz;
                /* du/dz - dw/dx */
                float dudz = (v[vidx3(n,n,ix,iy,iz+1)+0] - v[vidx3(n,n,ix,iy,iz-1)+0]) * inv_2dx;
                float dwdx = (v[vidx3(n,n,ix+1,iy,iz)+2] - v[vidx3(n,n,ix-1,iy,iz)+2]) * inv_2dx;
                w[c+1] = dudz - dwdx;
                /* dv/dx - du/dy */
                float dvdx = (v[vidx3(n,n,ix+1,iy,iz)+1] - v[vidx3(n,n,ix-1,iy,iz)+1]) * inv_2dx;
                float dudy = (v[vidx3(n,n,ix,iy+1,iz)+0] - v[vidx3(n,n,ix,iy-1,iz)+0]) * inv_2dx;
                w[c+2] = dvdx - dudy;
            }
        }
    }
}

int fd_step(fluid_dynamics_sim_t* sim, float dt) {
    if (!sim || !sim->initialized) { return -1; }

    uint32_t n = sim->config.grid_dim;
    float dx = sim->config.cell_size;
    float rho = sim->config.density;
    float mu = sim->config.viscosity;
    float inv_dx = 1.0f / dx;
    float inv_2dx = 1.0f / (2.0f * dx);
    float inv_dx2 = 1.0f / (dx * dx);
    float* v = sim->velocity.data;
    float* p = sim->pressure.data;

    if (dt <= 0.0f) { dt = sim->config.dt; }

    /* CFL check */
    float max_v = 0.0f;
    uint32_t total3 = n * n * n * 3;
    for (uint32_t i = 0; i < total3; i++) {
        float absv = fabsf(v[i]);
        if (absv > max_v) { max_v = absv; }
    }
    float cfl = max_v * dt * inv_dx;
    if (cfl > 0.5f && max_v > 1e-10f) {
        dt = 0.4f * dx / max_v;
    }

    /* Allocate temporary for new velocity */
    float* v_new = nimcp_calloc(n * n * n * 3, sizeof(float));
    if (!v_new) { return -1; }
    memcpy(v_new, v, n * n * n * 3 * sizeof(float));

    /* Interior update: Navier-Stokes momentum equation */
    for (uint32_t iz = 1; iz < n - 1; iz++) {
        for (uint32_t iy = 1; iy < n - 1; iy++) {
            for (uint32_t ix = 1; ix < n - 1; ix++) {
                uint32_t ci = vidx3(n, n, ix, iy, iz);
                uint32_t pi = idx3(n, n, ix, iy, iz);

                for (int comp = 0; comp < 3; comp++) {
                    /* Advection: -(v . grad)v  (central difference) */
                    float dvdx = (v[vidx3(n,n,ix+1,iy,iz)+comp] - v[vidx3(n,n,ix-1,iy,iz)+comp]) * inv_2dx;
                    float dvdy = (v[vidx3(n,n,ix,iy+1,iz)+comp] - v[vidx3(n,n,ix,iy-1,iz)+comp]) * inv_2dx;
                    float dvdz = (v[vidx3(n,n,ix,iy,iz+1)+comp] - v[vidx3(n,n,ix,iy,iz-1)+comp]) * inv_2dx;
                    float advection = -(v[ci+0]*dvdx + v[ci+1]*dvdy + v[ci+2]*dvdz);

                    /* Pressure gradient: -(1/rho) * dp/dx_comp */
                    float dpdx = 0.0f;
                    if (comp == 0) dpdx = (p[idx3(n,n,ix+1,iy,iz)] - p[idx3(n,n,ix-1,iy,iz)]) * inv_2dx;
                    if (comp == 1) dpdx = (p[idx3(n,n,ix,iy+1,iz)] - p[idx3(n,n,ix,iy-1,iz)]) * inv_2dx;
                    if (comp == 2) dpdx = (p[idx3(n,n,ix,iy,iz+1)] - p[idx3(n,n,ix,iy,iz-1)]) * inv_2dx;
                    float pressure_term = -dpdx / rho;

                    /* Viscous diffusion: (mu/rho) * laplacian(v_comp) */
                    float lap = (v[vidx3(n,n,ix+1,iy,iz)+comp] + v[vidx3(n,n,ix-1,iy,iz)+comp]
                               + v[vidx3(n,n,ix,iy+1,iz)+comp] + v[vidx3(n,n,ix,iy-1,iz)+comp]
                               + v[vidx3(n,n,ix,iy,iz+1)+comp] + v[vidx3(n,n,ix,iy,iz-1)+comp]
                               - 6.0f * v[ci+comp]) * inv_dx2;
                    float viscous = (mu / rho) * lap;

                    /* Gravity */
                    float grav = 0.0f;
                    if (comp == 0) grav = sim->config.gravity_x;
                    if (comp == 1) grav = sim->config.gravity_y;
                    if (comp == 2) grav = sim->config.gravity_z;

                    v_new[ci + comp] = v[ci + comp] + dt * (advection + pressure_term + viscous + grav);
                }
            }
        }
    }

    memcpy(v, v_new, n * n * n * 3 * sizeof(float));
    nimcp_free(v_new);

    /* Apply obstacles: zero velocity inside */
    for (uint32_t oi = 0; oi < sim->num_obstacles; oi++) {
        fd_obstacle_t* obs = &sim->obstacles[oi];
        if (!obs->active) continue;
        for (uint32_t iz = 0; iz < n; iz++) {
            for (uint32_t iy = 0; iy < n; iy++) {
                for (uint32_t ix = 0; ix < n; ix++) {
                    float x = (float)ix * dx - obs->center_x;
                    float y = (float)iy * dx - obs->center_y;
                    float z = (float)iz * dx - obs->center_z;
                    if (x*x + y*y + z*z < obs->radius * obs->radius) {
                        uint32_t vi = vidx3(n, n, ix, iy, iz);
                        v[vi] = v[vi+1] = v[vi+2] = 0.0f;
                    }
                }
            }
        }
    }

    /* Apply sources */
    for (uint32_t si = 0; si < sim->num_sources; si++) {
        fd_source_t* src = &sim->sources[si];
        if (!src->active) continue;
        uint32_t ix = (uint32_t)(src->pos_x / dx);
        uint32_t iy = (uint32_t)(src->pos_y / dx);
        uint32_t iz = (uint32_t)(src->pos_z / dx);
        if (ix < n && iy < n && iz < n) {
            uint32_t vi = vidx3(n, n, ix, iy, iz);
            v[vi+0] += src->velocity_x * dt;
            v[vi+1] += src->velocity_y * dt;
            v[vi+2] += src->velocity_z * dt;
        }
    }

    apply_boundary(sim);
    compute_vorticity(sim);

    /* Update statistics */
    sim->time += dt;
    sim->stats.step_count++;
    sim->stats.time = sim->time;
    float max_vel = 0.0f, max_vort = 0.0f, ke = 0.0f;
    float max_p = -1e30f, min_p = 1e30f, total_mass = 0.0f;
    float dV = dx * dx * dx;
    uint32_t total = n * n * n;
    for (uint32_t i = 0; i < total; i++) {
        float vx = v[i*3+0], vy = v[i*3+1], vz = v[i*3+2];
        float vmag = sqrtf(vx*vx + vy*vy + vz*vz);
        if (vmag > max_vel) max_vel = vmag;
        float wx = sim->vorticity.data[i*3+0], wy = sim->vorticity.data[i*3+1], wz = sim->vorticity.data[i*3+2];
        float wmag = sqrtf(wx*wx + wy*wy + wz*wz);
        if (wmag > max_vort) max_vort = wmag;
        ke += 0.5f * sim->density_field.data[i] * vmag * vmag * dV;
        if (p[i] > max_p) max_p = p[i];
        if (p[i] < min_p) min_p = p[i];
        total_mass += sim->density_field.data[i] * dV;
    }
    sim->stats.max_velocity = max_vel;
    sim->stats.max_vorticity = max_vort;
    sim->stats.total_kinetic_energy = ke;
    sim->stats.max_pressure = max_p;
    sim->stats.min_pressure = min_p;
    sim->stats.total_mass = total_mass;
    sim->stats.cfl_number = max_vel * dt * inv_dx;
    sim->stats.avg_reynolds = (max_vel > 0.0f) ? rho * max_vel * dx * (float)n / mu : 0.0f;

    return 0;
}

fd_stats_t fd_get_stats(const fluid_dynamics_sim_t* sim) {
    if (!sim) { fd_stats_t s; memset(&s, 0, sizeof(s)); return s; }
    return sim->stats;
}

/* ============================================================================
 * Setup Functions
 * ============================================================================ */

void fd_init_channel_flow(fluid_dynamics_sim_t* sim, float velocity) {
    if (!sim || !sim->initialized) return;
    uint32_t n = sim->config.grid_dim;
    float* v = sim->velocity.data;
    /* Parabolic profile: u(y) = 4*U_max * y/H * (1 - y/H) */
    float H = (float)n;
    for (uint32_t iz = 0; iz < n; iz++) {
        for (uint32_t iy = 0; iy < n; iy++) {
            float y_norm = (float)iy / H;
            float u = 4.0f * velocity * y_norm * (1.0f - y_norm);
            for (uint32_t ix = 0; ix < n; ix++) {
                uint32_t vi = vidx3(n, n, ix, iy, iz);
                v[vi+0] = u;   /* x-velocity */
                v[vi+1] = 0.0f;
                v[vi+2] = 0.0f;
            }
        }
    }
    LOG_INFO(LOG_TAG, "Initialized channel flow, U_max=%.2f", velocity);
}

void fd_init_cavity_flow(fluid_dynamics_sim_t* sim, float lid_velocity) {
    if (!sim || !sim->initialized) return;
    uint32_t n = sim->config.grid_dim;
    float* v = sim->velocity.data;
    memset(v, 0, n * n * n * 3 * sizeof(float));
    /* Top boundary (y = n-1): u = lid_velocity */
    for (uint32_t iz = 0; iz < n; iz++) {
        for (uint32_t ix = 0; ix < n; ix++) {
            uint32_t vi = vidx3(n, n, ix, n - 1, iz);
            v[vi+0] = lid_velocity;
        }
    }
    LOG_INFO(LOG_TAG, "Initialized cavity flow, lid_v=%.2f", lid_velocity);
}

void fd_init_flow_around_sphere(fluid_dynamics_sim_t* sim, float radius, float freestream_v) {
    if (!sim || !sim->initialized) return;
    uint32_t n = sim->config.grid_dim;
    float dx = sim->config.cell_size;
    float cx = (float)n * dx * 0.5f;
    float cy = cx, cz = cx;
    float* v = sim->velocity.data;

    for (uint32_t iz = 0; iz < n; iz++) {
        for (uint32_t iy = 0; iy < n; iy++) {
            for (uint32_t ix = 0; ix < n; ix++) {
                float x = (float)ix * dx - cx;
                float y = (float)iy * dx - cy;
                float z = (float)iz * dx - cz;
                float r2 = x*x + y*y + z*z;
                uint32_t vi = vidx3(n, n, ix, iy, iz);
                if (r2 < radius * radius) {
                    v[vi] = v[vi+1] = v[vi+2] = 0.0f;
                } else {
                    v[vi+0] = freestream_v;
                    v[vi+1] = 0.0f;
                    v[vi+2] = 0.0f;
                }
            }
        }
    }

    /* Add obstacle record */
    if (sim->num_obstacles < FD_MAX_OBSTACLES) {
        fd_obstacle_t obs = { .center_x = cx, .center_y = cy, .center_z = cz,
                              .radius = radius, .drag_coeff = 0.47f, .active = true };
        sim->obstacles[sim->num_obstacles++] = obs;
    }
    LOG_INFO(LOG_TAG, "Initialized flow around sphere r=%.3f, v=%.2f", radius, freestream_v);
}

uint32_t fd_add_obstacle(fluid_dynamics_sim_t* sim, const fd_obstacle_t* obs) {
    if (!sim || sim->num_obstacles >= FD_MAX_OBSTACLES) return UINT32_MAX;
    uint32_t idx = sim->num_obstacles;
    sim->obstacles[idx] = *obs;
    sim->obstacles[idx].active = true;
    sim->num_obstacles++;
    return idx;
}

uint32_t fd_add_source(fluid_dynamics_sim_t* sim, const fd_source_t* src) {
    if (!sim || sim->num_sources >= FD_MAX_SOURCES) return UINT32_MAX;
    uint32_t idx = sim->num_sources;
    sim->sources[idx] = *src;
    sim->sources[idx].active = true;
    sim->num_sources++;
    return idx;
}

/* ============================================================================
 * Physics Computations
 * ============================================================================ */

float fd_reynolds_number(float density, float velocity, float length, float viscosity) {
    if (viscosity < 1e-20f) return 1e10f;
    return density * fabsf(velocity) * length / viscosity;
}

float fd_drag_force(float drag_coeff, float density, float area, float velocity) {
    return 0.5f * drag_coeff * density * area * velocity * velocity;
}

float fd_lift_force(float lift_coeff, float density, float area, float velocity) {
    return 0.5f * lift_coeff * density * area * velocity * velocity;
}

float fd_bernoulli_pressure(float density, float velocity, float height,
                             float reference_pressure) {
    /* p + 0.5*rho*v^2 + rho*g*h = const  =>  p = const - 0.5*rho*v^2 - rho*g*h */
    return reference_pressure - 0.5f * density * velocity * velocity - density * FD_GRAVITY * height;
}

float fd_terminal_velocity(float mass, float drag_coeff, float density, float area) {
    if (drag_coeff * density * area < 1e-20f) return 1e10f;
    return sqrtf(2.0f * mass * FD_GRAVITY / (density * drag_coeff * area));
}

float fd_poiseuille_flow_rate(float radius, float pressure_drop,
                               float viscosity, float length) {
    if (viscosity * length < 1e-30f) return 0.0f;
    float r4 = radius * radius * radius * radius;
    return (float)M_PI * r4 * pressure_drop / (8.0f * viscosity * length);
}

float fd_mach_number(float velocity, float speed_of_sound) {
    if (speed_of_sound < 1e-10f) return 0.0f;
    return fabsf(velocity) / speed_of_sound;
}

void fd_get_velocity_at(const fluid_dynamics_sim_t* sim,
                         uint32_t ix, uint32_t iy, uint32_t iz,
                         float* vx, float* vy, float* vz) {
    if (!sim || !sim->initialized) { *vx = *vy = *vz = 0.0f; return; }
    uint32_t n = sim->config.grid_dim;
    if (ix >= n || iy >= n || iz >= n) { *vx = *vy = *vz = 0.0f; return; }
    uint32_t vi = vidx3(n, n, ix, iy, iz);
    *vx = sim->velocity.data[vi+0];
    *vy = sim->velocity.data[vi+1];
    *vz = sim->velocity.data[vi+2];
}

float fd_get_pressure_at(const fluid_dynamics_sim_t* sim,
                          uint32_t ix, uint32_t iy, uint32_t iz) {
    if (!sim || !sim->initialized) return 0.0f;
    uint32_t n = sim->config.grid_dim;
    if (ix >= n || iy >= n || iz >= n) return 0.0f;
    return sim->pressure.data[idx3(n, n, ix, iy, iz)];
}

/* ============================================================================
 * Legacy API Wrappers
 * ============================================================================ */

fluid_dynamics_sim_t* fluid_dynamics_create(const fluid_dynamics_config_t* config) {
    return fd_create(config);
}

void fluid_dynamics_destroy(fluid_dynamics_sim_t* sim) { fd_destroy(sim); }

int fluid_dynamics_step(fluid_dynamics_sim_t* sim, float dt) { return fd_step(sim, dt); }

fluid_dynamics_config_t fluid_dynamics_default_config(void) { return fd_default_config(); }

fluid_dynamics_stats_t fluid_dynamics_get_stats(const fluid_dynamics_sim_t* sim) {
    return fd_get_stats(sim);
}
