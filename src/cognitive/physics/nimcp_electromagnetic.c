/**
 * @file nimcp_electromagnetic.c
 * @brief Electromagnetic Field Simulator — Maxwell's equations
 *
 * WHAT: FDTD (Yee grid) for field propagation, Coulomb/Biot-Savart for statics,
 *       Lorentz force for charge dynamics
 * WHY:  Reasoning about electricity, magnetism, light, circuits, motors
 * HOW:  Yee staggered grid, leapfrog time integration, CFL-limited
 */

#include "cognitive/physics/nimcp_electromagnetic.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "ELECTROMAGNETIC"

/* ============================================================================
 * Helpers
 * ============================================================================ */

static inline float em_dot(wm_parietal_vec3_t a, wm_parietal_vec3_t b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
static inline wm_parietal_vec3_t em_cross(wm_parietal_vec3_t a, wm_parietal_vec3_t b) {
    return (wm_parietal_vec3_t){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}
static inline wm_parietal_vec3_t em_add(wm_parietal_vec3_t a, wm_parietal_vec3_t b) {
    return (wm_parietal_vec3_t){ a.x + b.x, a.y + b.y, a.z + b.z };
}
static inline wm_parietal_vec3_t em_sub(wm_parietal_vec3_t a, wm_parietal_vec3_t b) {
    return (wm_parietal_vec3_t){ a.x - b.x, a.y - b.y, a.z - b.z };
}
static inline wm_parietal_vec3_t em_scale(wm_parietal_vec3_t v, float s) {
    return (wm_parietal_vec3_t){ v.x * s, v.y * s, v.z * s };
}
static inline float em_len(wm_parietal_vec3_t v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

/* Grid indexing */
static inline uint32_t grid_idx(uint32_t nx, uint32_t ny, uint32_t ix, uint32_t iy, uint32_t iz) {
    return (iz * ny + iy) * nx + ix;
}

/* Get vector field component at grid point */
static inline wm_parietal_vec3_t vfield_get(const em_vector_field_t* f,
                                              uint32_t ix, uint32_t iy, uint32_t iz) {
    uint32_t idx = grid_idx(f->nx, f->ny, ix, iy, iz) * 3;
    return (wm_parietal_vec3_t){ f->data[idx], f->data[idx+1], f->data[idx+2] };
}

static inline void vfield_set(em_vector_field_t* f,
                                uint32_t ix, uint32_t iy, uint32_t iz,
                                wm_parietal_vec3_t val) {
    uint32_t idx = grid_idx(f->nx, f->ny, ix, iy, iz) * 3;
    f->data[idx]   = val.x;
    f->data[idx+1] = val.y;
    f->data[idx+2] = val.z;
}

/* ============================================================================
 * Field Allocation
 * ============================================================================ */

static bool alloc_vector_field(em_vector_field_t* f, uint32_t n, float cell_size) {
    f->nx = f->ny = f->nz = n;
    f->dx = f->dy = f->dz = cell_size;
    f->origin_x = f->origin_y = f->origin_z = -(float)n * cell_size * 0.5f;
    uint32_t total = n * n * n * 3;
    f->data = nimcp_calloc(total, sizeof(float));
    return f->data != NULL;
}

static bool alloc_scalar_field(em_scalar_field_t* f, uint32_t n, float cell_size) {
    f->nx = f->ny = f->nz = n;
    f->dx = f->dy = f->dz = cell_size;
    uint32_t total = n * n * n;
    f->data = nimcp_calloc(total, sizeof(float));
    return f->data != NULL;
}

/* ============================================================================
 * Static Field Computations
 * ============================================================================ */

wm_parietal_vec3_t em_coulomb_force(float q1, wm_parietal_vec3_t pos1,
                                     float q2, wm_parietal_vec3_t pos2) {
    wm_parietal_vec3_t r = em_sub(pos1, pos2);
    float dist = em_len(r);
    if (dist < 1e-10f) return (wm_parietal_vec3_t){0, 0, 0};
    float F_mag = EM_COULOMB_K * q1 * q2 / (dist * dist);
    return em_scale(r, F_mag / dist);  /* F * r_hat */
}

wm_parietal_vec3_t em_biot_savart(const em_current_t* current, wm_parietal_vec3_t point) {
    /* dB = (mu_0 / 4*pi) * I * (dl x r_hat) / r^2 */
    wm_parietal_vec3_t dl = em_sub(current->end, current->start);
    wm_parietal_vec3_t mid = em_scale(em_add(current->start, current->end), 0.5f);
    wm_parietal_vec3_t r = em_sub(point, mid);
    float dist = em_len(r);
    if (dist < 1e-10f) return (wm_parietal_vec3_t){0, 0, 0};

    wm_parietal_vec3_t dl_cross_r = em_cross(dl, r);
    float coeff = (EM_MU_0 / (4.0f * 3.14159265f)) * current->current / (dist * dist * dist);
    return em_scale(dl_cross_r, coeff);
}

wm_parietal_vec3_t em_lorentz_force(const electromagnetic_sim_t* sim,
                                     float charge, wm_parietal_vec3_t position,
                                     wm_parietal_vec3_t velocity) {
    wm_parietal_vec3_t E = em_get_E_at(sim, position);
    wm_parietal_vec3_t B = em_get_B_at(sim, position);
    /* F = q * (E + v x B) */
    wm_parietal_vec3_t vxB = em_cross(velocity, B);
    wm_parietal_vec3_t E_plus_vxB = em_add(E, vxB);
    return em_scale(E_plus_vxB, charge);
}

/* ============================================================================
 * Field Interpolation
 * ============================================================================ */

wm_parietal_vec3_t em_get_E_at(const electromagnetic_sim_t* sim, wm_parietal_vec3_t pos) {
    if (!sim || !sim->E_field.data) return (wm_parietal_vec3_t){0, 0, 0};
    const em_vector_field_t* f = &sim->E_field;

    /* Convert world position to grid coordinates */
    float gx = (pos.x - f->origin_x) / f->dx;
    float gy = (pos.y - f->origin_y) / f->dy;
    float gz = (pos.z - f->origin_z) / f->dz;

    /* Nearest grid point (clamped) */
    int ix = (int)gx; if (ix < 0) ix = 0; if ((uint32_t)ix >= f->nx - 1) ix = f->nx - 2;
    int iy = (int)gy; if (iy < 0) iy = 0; if ((uint32_t)iy >= f->ny - 1) iy = f->ny - 2;
    int iz = (int)gz; if (iz < 0) iz = 0; if ((uint32_t)iz >= f->nz - 1) iz = f->nz - 2;

    /* Trilinear interpolation weights */
    float fx = gx - ix, fy = gy - iy, fz = gz - iz;
    if (fx < 0) fx = 0; if (fx > 1) fx = 1;
    if (fy < 0) fy = 0; if (fy > 1) fy = 1;
    if (fz < 0) fz = 0; if (fz > 1) fz = 1;

    wm_parietal_vec3_t result = {0, 0, 0};
    for (int dz = 0; dz <= 1; dz++) {
        for (int dy = 0; dy <= 1; dy++) {
            for (int dx = 0; dx <= 1; dx++) {
                float w = (dx ? fx : 1-fx) * (dy ? fy : 1-fy) * (dz ? fz : 1-fz);
                wm_parietal_vec3_t val = vfield_get(f, ix+dx, iy+dy, iz+dz);
                result = em_add(result, em_scale(val, w));
            }
        }
    }
    return result;
}

wm_parietal_vec3_t em_get_B_at(const electromagnetic_sim_t* sim, wm_parietal_vec3_t pos) {
    if (!sim || !sim->B_field.data) return (wm_parietal_vec3_t){0, 0, 0};
    const em_vector_field_t* f = &sim->B_field;

    float gx = (pos.x - f->origin_x) / f->dx;
    float gy = (pos.y - f->origin_y) / f->dy;
    float gz = (pos.z - f->origin_z) / f->dz;

    int ix = (int)gx; if (ix < 0) ix = 0; if ((uint32_t)ix >= f->nx - 1) ix = f->nx - 2;
    int iy = (int)gy; if (iy < 0) iy = 0; if ((uint32_t)iy >= f->ny - 1) iy = f->ny - 2;
    int iz = (int)gz; if (iz < 0) iz = 0; if ((uint32_t)iz >= f->nz - 1) iz = f->nz - 2;

    float fx = gx - ix, fy = gy - iy, fz = gz - iz;
    if (fx < 0) fx = 0; if (fx > 1) fx = 1;
    if (fy < 0) fy = 0; if (fy > 1) fy = 1;
    if (fz < 0) fz = 0; if (fz > 1) fz = 1;

    wm_parietal_vec3_t result = {0, 0, 0};
    for (int dz = 0; dz <= 1; dz++)
        for (int dy = 0; dy <= 1; dy++)
            for (int dx = 0; dx <= 1; dx++) {
                float w = (dx ? fx : 1-fx) * (dy ? fy : 1-fy) * (dz ? fz : 1-fz);
                result = em_add(result, em_scale(vfield_get(f, ix+dx, iy+dy, iz+dz), w));
            }
    return result;
}

/* ============================================================================
 * Poynting Vector and Energy
 * ============================================================================ */

wm_parietal_vec3_t em_poynting_vector(const electromagnetic_sim_t* sim,
                                       wm_parietal_vec3_t pos) {
    wm_parietal_vec3_t E = em_get_E_at(sim, pos);
    wm_parietal_vec3_t B = em_get_B_at(sim, pos);
    /* S = (E x B) / mu_0 */
    return em_scale(em_cross(E, B), 1.0f / EM_MU_0);
}

float em_total_field_energy(const electromagnetic_sim_t* sim) {
    if (!sim || !sim->E_field.data || !sim->B_field.data) return 0;
    const em_vector_field_t* Ef = &sim->E_field;
    const em_vector_field_t* Bf = &sim->B_field;
    uint32_t n = Ef->nx * Ef->ny * Ef->nz;
    float dV = Ef->dx * Ef->dy * Ef->dz;
    float eps = EM_EPSILON_0 * sim->config.permittivity;
    float mu = EM_MU_0 * sim->config.permeability;

    float energy = 0;
    for (uint32_t i = 0; i < n; i++) {
        float Ex = Ef->data[i*3], Ey = Ef->data[i*3+1], Ez = Ef->data[i*3+2];
        float Bx = Bf->data[i*3], By = Bf->data[i*3+1], Bz = Bf->data[i*3+2];
        float E2 = Ex*Ex + Ey*Ey + Ez*Ez;
        float B2 = Bx*Bx + By*By + Bz*Bz;
        energy += (eps * E2 + B2 / mu) * 0.5f * dV;
    }
    return energy;
}

float em_total_charge(const electromagnetic_sim_t* sim) {
    if (!sim) return 0;
    float total = 0;
    for (uint32_t i = 0; i < sim->num_charges; i++)
        if (sim->charges[i].active)
            total += sim->charges[i].charge;
    return total;
}

float em_electric_potential(const electromagnetic_sim_t* sim, wm_parietal_vec3_t pos) {
    if (!sim) return 0;
    float V = 0;
    for (uint32_t i = 0; i < sim->num_charges; i++) {
        if (!sim->charges[i].active) continue;
        float r = em_len(em_sub(pos, sim->charges[i].position));
        if (r < 1e-10f) continue;
        V += EM_COULOMB_K * sim->charges[i].charge / r;
    }
    return V;
}

/* ============================================================================
 * FDTD Step (Yee Grid)
 * ============================================================================ */

static void fdtd_update_B(electromagnetic_sim_t* sim, float dt) {
    /* Faraday's law: dB/dt = -curl(E)
     *
     * Yee grid staggering (Fix #2): E components live on cell edges,
     * B components live on cell faces. The curl is computed using the
     * natural half-cell offsets of the staggered grid, giving second-order
     * accuracy without explicit interpolation.
     *
     * B_x(i,j+½,k+½) uses E_z(i,j+1,k) - E_z(i,j,k) and E_y(i,j,k+1) - E_y(i,j,k)
     * B_y(i+½,j,k+½) uses E_x(i,j,k+1) - E_x(i,j,k) and E_z(i+1,j,k) - E_z(i,j,k)
     * B_z(i+½,j+½,k) uses E_y(i+1,j,k) - E_y(i,j,k) and E_x(i,j+1,k) - E_x(i,j,k)
     */
    em_vector_field_t* E = &sim->E_field;
    em_vector_field_t* B = &sim->B_field;
    uint32_t nx = E->nx, ny = E->ny, nz = E->nz;
    float idx = 1.0f / E->dx, idy = 1.0f / E->dy, idz = 1.0f / E->dz;

    for (uint32_t iz = 0; iz < nz - 1; iz++) {
        for (uint32_t iy = 0; iy < ny - 1; iy++) {
            for (uint32_t ix = 0; ix < nx - 1; ix++) {
                wm_parietal_vec3_t Ec = vfield_get(E, ix, iy, iz);
                wm_parietal_vec3_t Exn = vfield_get(E, ix+1, iy, iz);
                wm_parietal_vec3_t Eyn = vfield_get(E, ix, iy+1, iz);
                wm_parietal_vec3_t Ezn = vfield_get(E, ix, iy, iz+1);

                /* Staggered curl: differences span exactly one cell (not two),
                 * giving the correct half-cell offset for the Yee scheme */
                float curlx = (Eyn.z - Ec.z) * idy - (Ezn.y - Ec.y) * idz;
                float curly = (Ezn.x - Ec.x) * idz - (Exn.z - Ec.z) * idx;
                float curlz = (Exn.y - Ec.y) * idx - (Eyn.x - Ec.x) * idy;

                wm_parietal_vec3_t Bc = vfield_get(B, ix, iy, iz);
                Bc.x -= curlx * dt;
                Bc.y -= curly * dt;
                Bc.z -= curlz * dt;
                vfield_set(B, ix, iy, iz, Bc);
            }
        }
    }
}

static void fdtd_update_E(electromagnetic_sim_t* sim, float dt) {
    /* Ampere-Maxwell: dE/dt = (curl(B)/mu_0 - J) / eps_0
     *
     * Yee staggering: E components at cell edges, B at cell faces.
     * E_x(i+½,j,k) uses B_z(i,j,k) - B_z(i,j-1,k) and B_y(i,j,k) - B_y(i,j,k-1)
     * This naturally gives second-order accuracy. */
    em_vector_field_t* E = &sim->E_field;
    em_vector_field_t* B = &sim->B_field;
    em_vector_field_t* J = &sim->current_density;
    uint32_t nx = E->nx, ny = E->ny, nz = E->nz;
    float idx = 1.0f / E->dx, idy = 1.0f / E->dy, idz = 1.0f / E->dz;
    float eps = EM_EPSILON_0 * sim->config.permittivity;
    float mu = EM_MU_0 * sim->config.permeability;
    float inv_eps = 1.0f / eps;
    float inv_mu = 1.0f / mu;

    for (uint32_t iz = 1; iz < nz; iz++) {
        for (uint32_t iy = 1; iy < ny; iy++) {
            for (uint32_t ix = 1; ix < nx; ix++) {
                wm_parietal_vec3_t Bc = vfield_get(B, ix, iy, iz);
                wm_parietal_vec3_t Bxp = vfield_get(B, ix-1, iy, iz);
                wm_parietal_vec3_t Byp = vfield_get(B, ix, iy-1, iz);
                wm_parietal_vec3_t Bzp = vfield_get(B, ix, iy, iz-1);

                float curlx = (Bc.z - Byp.z) * idy - (Bc.y - Bzp.y) * idz;
                float curly = (Bc.x - Bzp.x) * idz - (Bc.z - Bxp.z) * idx;
                float curlz = (Bc.y - Bxp.y) * idx - (Bc.x - Byp.x) * idy;

                wm_parietal_vec3_t Jc = J->data ? vfield_get(J, ix, iy, iz)
                                                 : (wm_parietal_vec3_t){0,0,0};

                wm_parietal_vec3_t Ec = vfield_get(E, ix, iy, iz);
                Ec.x += (curlx * inv_mu - Jc.x) * inv_eps * dt;
                Ec.y += (curly * inv_mu - Jc.y) * inv_eps * dt;
                Ec.z += (curlz * inv_mu - Jc.z) * inv_eps * dt;
                vfield_set(E, ix, iy, iz, Ec);
            }
        }
    }
}

static void update_charge_dynamics(electromagnetic_sim_t* sim, float dt) {
    if (!sim->config.enable_charge_dynamics) return;

    for (uint32_t i = 0; i < sim->num_charges; i++) {
        em_charge_t* ch = &sim->charges[i];
        if (!ch->active || ch->fixed || ch->mass <= 0) continue;

        /* Lorentz force */
        wm_parietal_vec3_t F = em_lorentz_force(sim, ch->charge, ch->position, ch->velocity);

        /* Also Coulomb forces from other charges */
        for (uint32_t j = 0; j < sim->num_charges; j++) {
            if (i == j || !sim->charges[j].active) continue;
            wm_parietal_vec3_t Fc = em_coulomb_force(ch->charge, ch->position,
                                                       sim->charges[j].charge,
                                                       sim->charges[j].position);
            F = em_add(F, Fc);
        }

        /* F = ma → a = F/m */
        float inv_m = 1.0f / ch->mass;
        ch->velocity.x += F.x * inv_m * dt;
        ch->velocity.y += F.y * inv_m * dt;
        ch->velocity.z += F.z * inv_m * dt;

        ch->position.x += ch->velocity.x * dt;
        ch->position.y += ch->velocity.y * dt;
        ch->position.z += ch->velocity.z * dt;
    }
}

/* ============================================================================
 * Public API
 * ============================================================================ */

em_config_t em_default_config(void) {
    float cell_size = 0.01f;
    return (em_config_t){
        .grid_dim = 32,
        .cell_size = cell_size,
        .dt = cell_size / (2.0f * EM_C),   /* CFL condition */
        .permittivity = 1.0f,
        .permeability = 1.0f,
        .enable_wave_propagation = true,
        .enable_charge_dynamics = true,
    };
}

electromagnetic_sim_t* em_create(const em_config_t* config) {
    em_config_t cfg = config ? *config : em_default_config();

    electromagnetic_sim_t* sim = nimcp_calloc(1, sizeof(*sim));
    if (!sim) return NULL;

    sim->config = cfg;
    uint32_t n = cfg.grid_dim;

    if (!alloc_vector_field(&sim->E_field, n, cfg.cell_size) ||
        !alloc_vector_field(&sim->B_field, n, cfg.cell_size) ||
        !alloc_scalar_field(&sim->charge_density, n, cfg.cell_size) ||
        !alloc_vector_field(&sim->current_density, n, cfg.cell_size)) {
        em_destroy(sim);
        return NULL;
    }

    sim->boundary = EM_BOUNDARY_ABSORBING;
    sim->initialized = true;

    LOG_INFO(LOG_TAG, "EM simulator created: grid=%u^3, cell=%.4fm, dt=%.2e s",
             n, cfg.cell_size, cfg.dt);
    return sim;
}

void em_destroy(electromagnetic_sim_t* sim) {
    if (!sim) return;
    nimcp_free(sim->E_field.data);
    nimcp_free(sim->B_field.data);
    nimcp_free(sim->charge_density.data);
    nimcp_free(sim->current_density.data);
    nimcp_free(sim);
}

uint32_t em_add_charge(electromagnetic_sim_t* sim, const em_charge_t* charge) {
    if (!sim || !charge || sim->num_charges >= EM_MAX_CHARGES) return UINT32_MAX;
    uint32_t id = sim->num_charges;
    sim->charges[id] = *charge;
    sim->charges[id].id = id;
    sim->charges[id].active = true;
    sim->num_charges = id + 1;
    return id;
}

uint32_t em_add_current(electromagnetic_sim_t* sim, const em_current_t* current) {
    if (!sim || !current || sim->num_currents >= EM_MAX_CURRENTS) return UINT32_MAX;
    uint32_t id = sim->num_currents;
    sim->currents[id] = *current;
    sim->currents[id].active = true;
    sim->num_currents = id + 1;
    return id;
}

uint32_t em_add_dipole(electromagnetic_sim_t* sim, const em_dipole_t* dipole) {
    if (!sim || !dipole || sim->num_dipoles >= EM_MAX_DIPOLES) return UINT32_MAX;
    uint32_t id = sim->num_dipoles;
    sim->dipoles[id] = *dipole;
    sim->dipoles[id].active = true;
    sim->num_dipoles = id + 1;
    return id;
}

uint32_t em_add_conductor(electromagnetic_sim_t* sim, const em_conductor_t* cond) {
    if (!sim || !cond || sim->num_conductors >= EM_MAX_CONDUCTORS) return UINT32_MAX;
    uint32_t id = sim->num_conductors;
    sim->conductors[id] = *cond;
    sim->conductors[id].active = true;
    sim->num_conductors = id + 1;
    return id;
}

int em_step(electromagnetic_sim_t* sim, float dt) {
    if (!sim || !sim->initialized) return -1;
    if (dt <= 0) dt = sim->config.dt;

    if (sim->config.enable_wave_propagation) {
        /* Leapfrog: B at half-steps, E at full steps */
        fdtd_update_B(sim, dt * 0.5f);
        fdtd_update_E(sim, dt);
        fdtd_update_B(sim, dt * 0.5f);
    }

    update_charge_dynamics(sim, dt);

    sim->time += dt;
    sim->stats.step_count++;
    sim->stats.total_field_energy = em_total_field_energy(sim);
    sim->stats.total_charge = em_total_charge(sim);
    sim->stats.active_charges = sim->num_charges;

    return 0;
}

void em_set_boundary(electromagnetic_sim_t* sim, em_boundary_t boundary) {
    if (sim) sim->boundary = boundary;
}

void em_inject_plane_wave(electromagnetic_sim_t* sim,
                           wm_parietal_vec3_t direction,
                           wm_parietal_vec3_t polarization,
                           float frequency, float amplitude) {
    if (!sim || !sim->E_field.data) return;
    em_vector_field_t* E = &sim->E_field;
    em_vector_field_t* B = &sim->B_field;
    float k = 2.0f * 3.14159265f * frequency / EM_C;
    wm_parietal_vec3_t B_pol = em_cross(direction, polarization);
    float B_amp = amplitude / EM_C;

    for (uint32_t iz = 0; iz < E->nz; iz++) {
        for (uint32_t iy = 0; iy < E->ny; iy++) {
            for (uint32_t ix = 0; ix < E->nx; ix++) {
                float x = E->origin_x + ix * E->dx;
                float y = E->origin_y + iy * E->dy;
                float z = E->origin_z + iz * E->dz;
                float phase = k * (direction.x * x + direction.y * y + direction.z * z);
                float val = amplitude * sinf(phase);
                float bval = B_amp * sinf(phase);
                wm_parietal_vec3_t Ev = em_scale(polarization, val);
                wm_parietal_vec3_t Bv = em_scale(B_pol, bval);
                vfield_set(E, ix, iy, iz, em_add(vfield_get(E, ix, iy, iz), Ev));
                vfield_set(B, ix, iy, iz, em_add(vfield_get(B, ix, iy, iz), Bv));
            }
        }
    }
}

em_stats_t em_get_stats(const electromagnetic_sim_t* sim) {
    if (!sim) return (em_stats_t){0};
    return sim->stats;
}
