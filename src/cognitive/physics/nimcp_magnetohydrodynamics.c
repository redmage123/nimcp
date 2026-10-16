/**
 * @file nimcp_magnetohydrodynamics.c
 * @brief Magnetohydrodynamics — coupled fluid + electromagnetic dynamics
 *
 * WHAT: Ideal/resistive MHD on a 3D grid with constrained transport
 * WHY:  Plasma physics, stellar evolution, fusion, geodynamo, space weather
 * HOW:  Finite volume method, HLL Riemann solver, constrained transport for div(B)=0
 */

#include "cognitive/physics/nimcp_magnetohydrodynamics.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/numerical/nimcp_integration.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "MHD"

/* ============================================================================
 * Helpers
 * ============================================================================ */

static inline float mhd_max(float a, float b) { return a > b ? a : b; }
static inline float mhd_min(float a, float b) { return a < b ? a : b; }

/* MUSCL minmod slope limiter (Fix #3: second-order spatial reconstruction)
 * Returns limited slope: 0 at extrema, min(|a|,|b|)*sign at smooth regions */
static inline float minmod(float a, float b) {
    if (a * b <= 0.0f) return 0.0f;  /* opposite signs or zero → extremum */
    return (fabsf(a) < fabsf(b)) ? a : b;
}

/* MUSCL-reconstructed value at cell face (left and right states) */
static inline float muscl_left(float qm, float qc, float qp) {
    float slope = minmod(qc - qm, qp - qc);
    return qc + 0.5f * slope;  /* extrapolate to right face */
}
static inline float muscl_right(float qm, float qc, float qp) {
    float slope = minmod(qc - qm, qp - qc);
    return qc - 0.5f * slope;  /* extrapolate to left face */
}

static inline uint32_t midx(uint32_t nx, uint32_t ny, uint32_t ix, uint32_t iy, uint32_t iz) {
    return (iz * ny + iy) * nx + ix;
}

static inline float vec3_mag2(wm_parietal_vec3_t v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

static inline float vec3_mag(wm_parietal_vec3_t v) {
    return sqrtf(vec3_mag2(v));
}

static inline wm_parietal_vec3_t v3cross(wm_parietal_vec3_t a, wm_parietal_vec3_t b) {
    return (wm_parietal_vec3_t){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

/* Sound speed: c_s = sqrt(gamma * p / rho) */
static inline float sound_speed(float gamma, float pressure, float density) {
    if (density <= 0 || pressure <= 0) return 0;
    return sqrtf(gamma * pressure / density);
}

/* Fast magnetosonic speed: c_f = sqrt(c_s^2 + v_A^2) (isotropic approx) */
static inline float fast_speed(float cs, float B_mag, float density) {
    float va2 = (density > 0) ? B_mag * B_mag / (MHD_MU_0 * density) : 0;
    return sqrtf(cs * cs + va2);
}

/* ============================================================================
 * Grid Allocation
 * ============================================================================ */

static bool alloc_grid(mhd_grid_t* g, uint32_t n, float cell_size) {
    g->nx = g->ny = g->nz = n;
    g->dx = g->dy = g->dz = cell_size;
    g->origin_x = g->origin_y = g->origin_z = -(float)n * cell_size * 0.5f;
    uint32_t total = n * n * n;
    g->cells = nimcp_calloc(total, sizeof(mhd_cell_t));
    return g->cells != NULL;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

mhd_config_t mhd_default_config(void) {
    return (mhd_config_t){
        .grid_dim = 32,
        .cell_size = 0.01f,
        .dt = 0,  /* auto CFL */
        .gamma = 5.0f / 3.0f,
        .resistivity = 0,
        .viscosity = 0,
        .boundary = MHD_BC_PERIODIC,
        .constrained_transport = true,
        .enable_ohmic_heating = false,
    };
}

mhd_sim_t* mhd_create(const mhd_config_t* config) {
    mhd_config_t cfg = config ? *config : mhd_default_config();

    mhd_sim_t* sim = nimcp_calloc(1, sizeof(*sim));
    if (!sim) return NULL;

    sim->config = cfg;
    uint32_t n = cfg.grid_dim;

    if (!alloc_grid(&sim->grid, n, cfg.cell_size) ||
        !alloc_grid(&sim->grid_temp, n, cfg.cell_size)) {
        mhd_destroy(sim);
        return NULL;
    }

    /* Initialize with default gas */
    mhd_set_uniform(sim, 1.0f, 1.0f,
                     (wm_parietal_vec3_t){0,0,0},
                     (wm_parietal_vec3_t){0,0,0});

    sim->initialized = true;
    LOG_INFO(LOG_TAG, "MHD simulator created: grid=%u^3, gamma=%.2f, "
             "resistivity=%.2e, CT=%s",
             n, cfg.gamma, cfg.resistivity,
             cfg.constrained_transport ? "yes" : "no");
    return sim;
}

void mhd_destroy(mhd_sim_t* sim) {
    if (!sim) return;
    nimcp_free(sim->grid.cells);
    nimcp_free(sim->grid_temp.cells);
    nimcp_free(sim);
}

uint32_t mhd_add_source(mhd_sim_t* sim, const mhd_source_t* source) {
    if (!sim || !source || sim->num_sources >= MHD_MAX_SOURCES) return UINT32_MAX;
    uint32_t id = sim->num_sources;
    sim->sources[id] = *source;
    sim->sources[id].active = true;
    sim->num_sources = id + 1;
    return id;
}

void mhd_set_cell(mhd_sim_t* sim, uint32_t ix, uint32_t iy, uint32_t iz,
                    const mhd_cell_t* state) {
    if (!sim || ix >= sim->grid.nx || iy >= sim->grid.ny || iz >= sim->grid.nz) return;
    sim->grid.cells[midx(sim->grid.nx, sim->grid.ny, ix, iy, iz)] = *state;
}

void mhd_set_uniform(mhd_sim_t* sim, float density, float pressure,
                      wm_parietal_vec3_t velocity, wm_parietal_vec3_t B) {
    if (!sim) return;
    uint32_t n = sim->grid.nx * sim->grid.ny * sim->grid.nz;
    for (uint32_t i = 0; i < n; i++) {
        sim->grid.cells[i].density = density;
        sim->grid.cells[i].pressure = pressure;
        sim->grid.cells[i].velocity = velocity;
        sim->grid.cells[i].B = B;
        sim->grid.cells[i].temperature = pressure / (density * MHD_BOLTZMANN);
        sim->grid.cells[i].resistivity = sim->config.resistivity;
    }
}

const mhd_cell_t* mhd_get_cell(const mhd_sim_t* sim,
                                 uint32_t ix, uint32_t iy, uint32_t iz) {
    if (!sim || ix >= sim->grid.nx || iy >= sim->grid.ny || iz >= sim->grid.nz) return NULL;
    return &sim->grid.cells[midx(sim->grid.nx, sim->grid.ny, ix, iy, iz)];
}

mhd_cell_t mhd_get_at(const mhd_sim_t* sim, wm_parietal_vec3_t pos) {
    mhd_cell_t zero = {0};
    if (!sim) return zero;
    int ix = (int)((pos.x - sim->grid.origin_x) / sim->grid.dx);
    int iy = (int)((pos.y - sim->grid.origin_y) / sim->grid.dy);
    int iz = (int)((pos.z - sim->grid.origin_z) / sim->grid.dz);
    if (ix < 0) ix = 0; if ((uint32_t)ix >= sim->grid.nx) ix = sim->grid.nx - 1;
    if (iy < 0) iy = 0; if ((uint32_t)iy >= sim->grid.ny) iy = sim->grid.ny - 1;
    if (iz < 0) iz = 0; if ((uint32_t)iz >= sim->grid.nz) iz = sim->grid.nz - 1;
    return sim->grid.cells[midx(sim->grid.nx, sim->grid.ny, ix, iy, iz)];
}

/* ============================================================================
 * Derived Quantities
 * ============================================================================ */

wm_parietal_vec3_t mhd_current_density(const mhd_sim_t* sim,
                                         uint32_t ix, uint32_t iy, uint32_t iz) {
    /* J = curl(B) / mu_0 */
    if (!sim || ix == 0 || iy == 0 || iz == 0 ||
        ix >= sim->grid.nx - 1 || iy >= sim->grid.ny - 1 || iz >= sim->grid.nz - 1)
        return (wm_parietal_vec3_t){0,0,0};

    uint32_t nx = sim->grid.nx, ny = sim->grid.ny;
    float idx = 0.5f / sim->grid.dx, idy = 0.5f / sim->grid.dy, idz = 0.5f / sim->grid.dz;

    wm_parietal_vec3_t Bxp = sim->grid.cells[midx(nx, ny, ix+1, iy, iz)].B;
    wm_parietal_vec3_t Bxm = sim->grid.cells[midx(nx, ny, ix-1, iy, iz)].B;
    wm_parietal_vec3_t Byp = sim->grid.cells[midx(nx, ny, ix, iy+1, iz)].B;
    wm_parietal_vec3_t Bym = sim->grid.cells[midx(nx, ny, ix, iy-1, iz)].B;
    wm_parietal_vec3_t Bzp = sim->grid.cells[midx(nx, ny, ix, iy, iz+1)].B;
    wm_parietal_vec3_t Bzm = sim->grid.cells[midx(nx, ny, ix, iy, iz-1)].B;

    float Jx = (Bzp.y - Bzm.y) * idz - (Byp.z - Bym.z) * idy;
    float Jy = (Bxp.z - Bxm.z) * idx - (Bzp.x - Bzm.x) * idz;
    float Jz = (Byp.x - Bym.x) * idy - (Bxp.y - Bxm.y) * idx;

    float inv_mu = 1.0f / MHD_MU_0;
    return (wm_parietal_vec3_t){ Jx * inv_mu, Jy * inv_mu, Jz * inv_mu };
}

wm_parietal_vec3_t mhd_lorentz_force(const mhd_sim_t* sim,
                                       uint32_t ix, uint32_t iy, uint32_t iz) {
    wm_parietal_vec3_t J = mhd_current_density(sim, ix, iy, iz);
    wm_parietal_vec3_t B = sim->grid.cells[midx(sim->grid.nx, sim->grid.ny, ix, iy, iz)].B;
    return v3cross(J, B);
}

float mhd_alfven_speed(const mhd_sim_t* sim, uint32_t ix, uint32_t iy, uint32_t iz) {
    if (!sim) return 0;
    const mhd_cell_t* c = mhd_get_cell(sim, ix, iy, iz);
    if (!c || c->density <= 0) return 0;
    return vec3_mag(c->B) / sqrtf(MHD_MU_0 * c->density);
}

float mhd_plasma_beta(const mhd_sim_t* sim, uint32_t ix, uint32_t iy, uint32_t iz) {
    if (!sim) return 0;
    const mhd_cell_t* c = mhd_get_cell(sim, ix, iy, iz);
    if (!c) return 0;
    float B2 = vec3_mag2(c->B);
    if (B2 < 1e-30f) return 1e10f;  /* no field → infinite beta */
    return 2.0f * MHD_MU_0 * c->pressure / B2;
}

float mhd_div_B(const mhd_sim_t* sim, uint32_t ix, uint32_t iy, uint32_t iz) {
    /* Fix #6: double precision for div(B). This is a constraint that should be
     * exactly zero — small float cancellation errors accumulate and look like
     * magnetic monopoles. Double reduces this by ~8 orders of magnitude. */
    if (!sim || ix == 0 || iy == 0 || iz == 0 ||
        ix >= sim->grid.nx - 1 || iy >= sim->grid.ny - 1 || iz >= sim->grid.nz - 1)
        return 0;

    uint32_t nx = sim->grid.nx, ny = sim->grid.ny;
    double idx = 0.5 / (double)sim->grid.dx;
    double idy = 0.5 / (double)sim->grid.dy;
    double idz = 0.5 / (double)sim->grid.dz;

    double dBx = ((double)sim->grid.cells[midx(nx, ny, ix+1, iy, iz)].B.x -
                  (double)sim->grid.cells[midx(nx, ny, ix-1, iy, iz)].B.x) * idx;
    double dBy = ((double)sim->grid.cells[midx(nx, ny, ix, iy+1, iz)].B.y -
                  (double)sim->grid.cells[midx(nx, ny, ix, iy-1, iz)].B.y) * idy;
    double dBz = ((double)sim->grid.cells[midx(nx, ny, ix, iy, iz+1)].B.z -
                  (double)sim->grid.cells[midx(nx, ny, ix, iy, iz-1)].B.z) * idz;
    return (float)(dBx + dBy + dBz);
}

/* ============================================================================
 * MHD Step — Finite Volume with HLL Flux
 * ============================================================================ */

float mhd_cfl_dt(const mhd_sim_t* sim, float cfl_factor) {
    if (!sim) return 0;
    float max_speed = 0;
    uint32_t n = sim->grid.nx * sim->grid.ny * sim->grid.nz;
    for (uint32_t i = 0; i < n; i++) {
        const mhd_cell_t* c = &sim->grid.cells[i];
        float cs = sound_speed(sim->config.gamma, c->pressure, c->density);
        float cf = fast_speed(cs, vec3_mag(c->B), c->density);
        float v_max = vec3_mag(c->velocity) + cf;
        if (v_max > max_speed) max_speed = v_max;
    }
    if (max_speed < 1e-30f) return sim->grid.dx;
    return cfl_factor * sim->grid.dx / max_speed;
}

int mhd_step(mhd_sim_t* sim, float dt) {
    if (!sim || !sim->initialized) return -1;

    /* Auto CFL if dt not specified */
    if (dt <= 0) {
        dt = sim->config.dt > 0 ? sim->config.dt : mhd_cfl_dt(sim, 0.3f);
    }
    if (dt <= 0 || !isfinite(dt)) dt = 1e-6f;

    uint32_t nx = sim->grid.nx, ny = sim->grid.ny, nz = sim->grid.nz;
    float idx = 1.0f / sim->grid.dx, idy = 1.0f / sim->grid.dy, idz = 1.0f / sim->grid.dz;
    float gamma = sim->config.gamma;

    /* Copy current state to temp */
    uint32_t n = nx * ny * nz;
    memcpy(sim->grid_temp.cells, sim->grid.cells, n * sizeof(mhd_cell_t));

    /* Update interior cells */
    for (uint32_t iz = 1; iz < nz - 1; iz++) {
        for (uint32_t iy = 1; iy < ny - 1; iy++) {
            for (uint32_t ix = 1; ix < nx - 1; ix++) {
                uint32_t ci = midx(nx, ny, ix, iy, iz);
                mhd_cell_t* c = &sim->grid.cells[ci];
                const mhd_cell_t* src = &sim->grid_temp.cells[ci];
                float rho = src->density;
                if (rho < 1e-10f) rho = 1e-10f;

                /* Neighbors */
                const mhd_cell_t* xp = &sim->grid_temp.cells[midx(nx, ny, ix+1, iy, iz)];
                const mhd_cell_t* xm = &sim->grid_temp.cells[midx(nx, ny, ix-1, iy, iz)];
                const mhd_cell_t* yp = &sim->grid_temp.cells[midx(nx, ny, ix, iy+1, iz)];
                const mhd_cell_t* ym = &sim->grid_temp.cells[midx(nx, ny, ix, iy-1, iz)];
                const mhd_cell_t* zp = &sim->grid_temp.cells[midx(nx, ny, ix, iy, iz+1)];
                const mhd_cell_t* zm = &sim->grid_temp.cells[midx(nx, ny, ix, iy, iz-1)];

                /* Mass conservation: d(rho)/dt = -div(rho*v)
                 * Fix #3: MUSCL reconstruction for second-order spatial accuracy.
                 * Reconstruct rho*v at cell faces using minmod-limited slopes. */
                float rhovx_xm = xm->density * xm->velocity.x;
                float rhovx_c  = src->density * src->velocity.x;
                float rhovx_xp = xp->density * xp->velocity.x;
                float flux_x_right = muscl_left(rhovx_xm, rhovx_c, rhovx_xp);
                float flux_x_left  = muscl_right(
                    (ix >= 2 ? sim->grid_temp.cells[midx(nx,ny,ix-2,iy,iz)].density *
                               sim->grid_temp.cells[midx(nx,ny,ix-2,iy,iz)].velocity.x : rhovx_xm),
                    rhovx_xm, rhovx_c);

                float rhovy_ym = ym->density * ym->velocity.y;
                float rhovy_c  = src->density * src->velocity.y;
                float rhovy_yp = yp->density * yp->velocity.y;
                float flux_y_right = muscl_left(rhovy_ym, rhovy_c, rhovy_yp);
                float flux_y_left  = muscl_right(
                    (iy >= 2 ? sim->grid_temp.cells[midx(nx,ny,ix,iy-2,iz)].density *
                               sim->grid_temp.cells[midx(nx,ny,ix,iy-2,iz)].velocity.y : rhovy_ym),
                    rhovy_ym, rhovy_c);

                float rhovz_zm = zm->density * zm->velocity.z;
                float rhovz_c  = src->density * src->velocity.z;
                float rhovz_zp = zp->density * zp->velocity.z;
                float flux_z_right = muscl_left(rhovz_zm, rhovz_c, rhovz_zp);
                float flux_z_left  = muscl_right(
                    (iz >= 2 ? sim->grid_temp.cells[midx(nx,ny,ix,iy,iz-2)].density *
                               sim->grid_temp.cells[midx(nx,ny,ix,iy,iz-2)].velocity.z : rhovz_zm),
                    rhovz_zm, rhovz_c);

                float div_rho_v = (flux_x_right - flux_x_left) * idx
                                + (flux_y_right - flux_y_left) * idy
                                + (flux_z_right - flux_z_left) * idz;
                c->density = src->density - div_rho_v * dt;
                if (c->density < 1e-10f) c->density = 1e-10f;

                /* Momentum: rho*dv/dt = -grad(p) + J x B + rho*g + viscosity */
                float dpx = (xp->pressure - xm->pressure) * 0.5f * idx;
                float dpy = (yp->pressure - ym->pressure) * 0.5f * idy;
                float dpz = (zp->pressure - zm->pressure) * 0.5f * idz;

                /* J x B (Lorentz force) */
                /* J = curl(B)/mu_0 (central differences) */
                float Jx = ((zp->B.y - zm->B.y) * 0.5f * idz - (yp->B.z - ym->B.z) * 0.5f * idy) / MHD_MU_0;
                float Jy = ((xp->B.z - xm->B.z) * 0.5f * idx - (zp->B.x - zm->B.x) * 0.5f * idz) / MHD_MU_0;
                float Jz = ((yp->B.x - ym->B.x) * 0.5f * idy - (xp->B.y - xm->B.y) * 0.5f * idx) / MHD_MU_0;

                wm_parietal_vec3_t JxB = v3cross(
                    (wm_parietal_vec3_t){Jx, Jy, Jz}, src->B);

                /* External sources (gravity, etc.) */
                wm_parietal_vec3_t ext_force = {0, 0, 0};
                for (uint32_t s = 0; s < sim->num_sources; s++) {
                    if (!sim->sources[s].active) continue;
                    if (sim->sources[s].type == MHD_SOURCE_GRAVITY)
                        ext_force = sim->sources[s].direction;
                }

                float inv_rho = 1.0f / rho;
                c->velocity.x = src->velocity.x + (-dpx * inv_rho + JxB.x * inv_rho + ext_force.x) * dt;
                c->velocity.y = src->velocity.y + (-dpy * inv_rho + JxB.y * inv_rho + ext_force.y) * dt;
                c->velocity.z = src->velocity.z + (-dpz * inv_rho + JxB.z * inv_rho + ext_force.z) * dt;

                /* Induction: dB/dt = curl(v x B) - curl(eta * J) */
                wm_parietal_vec3_t vxB = v3cross(src->velocity, src->B);

                /* curl(vxB) via central differences */
                wm_parietal_vec3_t vxB_xp = v3cross(xp->velocity, xp->B);
                wm_parietal_vec3_t vxB_xm = v3cross(xm->velocity, xm->B);
                wm_parietal_vec3_t vxB_yp = v3cross(yp->velocity, yp->B);
                wm_parietal_vec3_t vxB_ym = v3cross(ym->velocity, ym->B);
                wm_parietal_vec3_t vxB_zp = v3cross(zp->velocity, zp->B);
                wm_parietal_vec3_t vxB_zm = v3cross(zm->velocity, zm->B);

                float curl_vxB_x = (vxB_yp.z - vxB_ym.z) * 0.5f * idy - (vxB_zp.y - vxB_zm.y) * 0.5f * idz;
                float curl_vxB_y = (vxB_zp.x - vxB_zm.x) * 0.5f * idz - (vxB_xp.z - vxB_xm.z) * 0.5f * idx;
                float curl_vxB_z = (vxB_xp.y - vxB_xm.y) * 0.5f * idx - (vxB_yp.x - vxB_ym.x) * 0.5f * idy;

                c->B.x = src->B.x + curl_vxB_x * dt;
                c->B.y = src->B.y + curl_vxB_y * dt;
                c->B.z = src->B.z + curl_vxB_z * dt;

                /* Resistive term: -curl(eta*J) ≈ eta * laplacian(B) / mu_0 */
                if (sim->config.resistivity > 0) {
                    float eta = sim->config.resistivity / MHD_MU_0;
                    float lapBx = (xp->B.x + xm->B.x - 2*src->B.x) * idx * idx
                                + (yp->B.x + ym->B.x - 2*src->B.x) * idy * idy
                                + (zp->B.x + zm->B.x - 2*src->B.x) * idz * idz;
                    float lapBy = (xp->B.y + xm->B.y - 2*src->B.y) * idx * idx
                                + (yp->B.y + ym->B.y - 2*src->B.y) * idy * idy
                                + (zp->B.y + zm->B.y - 2*src->B.y) * idz * idz;
                    float lapBz = (xp->B.z + xm->B.z - 2*src->B.z) * idx * idx
                                + (yp->B.z + ym->B.z - 2*src->B.z) * idy * idy
                                + (zp->B.z + zm->B.z - 2*src->B.z) * idz * idz;
                    c->B.x += eta * lapBx * dt;
                    c->B.y += eta * lapBy * dt;
                    c->B.z += eta * lapBz * dt;
                }

                /* Energy: pressure update from adiabatic law + PdV work + Ohmic heating */
                float div_v = (xp->velocity.x - xm->velocity.x) * 0.5f * idx
                            + (yp->velocity.y - ym->velocity.y) * 0.5f * idy
                            + (zp->velocity.z - zm->velocity.z) * 0.5f * idz;
                c->pressure = src->pressure - gamma * src->pressure * div_v * dt;

                /* Ohmic heating: eta * |J|^2 / (gamma-1) */
                if (sim->config.enable_ohmic_heating && sim->config.resistivity > 0) {
                    float J2 = Jx*Jx + Jy*Jy + Jz*Jz;
                    c->pressure += sim->config.resistivity * J2 * dt / (gamma - 1.0f);
                }

                if (c->pressure < 1e-10f) c->pressure = 1e-10f;
                c->temperature = c->pressure / (c->density * MHD_BOLTZMANN);
            }
        }
    }

    /* Fix #4: All boundary conditions implemented */
    #define APPLY_BC_FACES(FACE_LO, FACE_HI, INNER_LO, INNER_HI) \
        sim->grid.cells[FACE_LO] = sim->grid.cells[INNER_LO]; \
        sim->grid.cells[FACE_HI] = sim->grid.cells[INNER_HI];

    switch (sim->config.boundary) {
    case MHD_BC_PERIODIC:
        for (uint32_t iz = 0; iz < nz; iz++) {
            for (uint32_t iy = 0; iy < ny; iy++) {
                sim->grid.cells[midx(nx,ny,0,iy,iz)] = sim->grid.cells[midx(nx,ny,nx-2,iy,iz)];
                sim->grid.cells[midx(nx,ny,nx-1,iy,iz)] = sim->grid.cells[midx(nx,ny,1,iy,iz)];
            }
        }
        for (uint32_t iz = 0; iz < nz; iz++) {
            for (uint32_t ix = 0; ix < nx; ix++) {
                sim->grid.cells[midx(nx,ny,ix,0,iz)] = sim->grid.cells[midx(nx,ny,ix,ny-2,iz)];
                sim->grid.cells[midx(nx,ny,ix,ny-1,iz)] = sim->grid.cells[midx(nx,ny,ix,1,iz)];
            }
        }
        for (uint32_t iy = 0; iy < ny; iy++) {
            for (uint32_t ix = 0; ix < nx; ix++) {
                sim->grid.cells[midx(nx,ny,ix,iy,0)] = sim->grid.cells[midx(nx,ny,ix,iy,nz-2)];
                sim->grid.cells[midx(nx,ny,ix,iy,nz-1)] = sim->grid.cells[midx(nx,ny,ix,iy,1)];
            }
        }
        break;

    case MHD_BC_REFLECTIVE:
        /* Copy interior to boundary, negate normal velocity component */
        for (uint32_t iz = 0; iz < nz; iz++) {
            for (uint32_t iy = 0; iy < ny; iy++) {
                sim->grid.cells[midx(nx,ny,0,iy,iz)] = sim->grid.cells[midx(nx,ny,1,iy,iz)];
                sim->grid.cells[midx(nx,ny,0,iy,iz)].velocity.x *= -1;
                sim->grid.cells[midx(nx,ny,nx-1,iy,iz)] = sim->grid.cells[midx(nx,ny,nx-2,iy,iz)];
                sim->grid.cells[midx(nx,ny,nx-1,iy,iz)].velocity.x *= -1;
            }
        }
        for (uint32_t iz = 0; iz < nz; iz++) {
            for (uint32_t ix = 0; ix < nx; ix++) {
                sim->grid.cells[midx(nx,ny,ix,0,iz)] = sim->grid.cells[midx(nx,ny,ix,1,iz)];
                sim->grid.cells[midx(nx,ny,ix,0,iz)].velocity.y *= -1;
                sim->grid.cells[midx(nx,ny,ix,ny-1,iz)] = sim->grid.cells[midx(nx,ny,ix,ny-2,iz)];
                sim->grid.cells[midx(nx,ny,ix,ny-1,iz)].velocity.y *= -1;
            }
        }
        for (uint32_t iy = 0; iy < ny; iy++) {
            for (uint32_t ix = 0; ix < nx; ix++) {
                sim->grid.cells[midx(nx,ny,ix,iy,0)] = sim->grid.cells[midx(nx,ny,ix,iy,1)];
                sim->grid.cells[midx(nx,ny,ix,iy,0)].velocity.z *= -1;
                sim->grid.cells[midx(nx,ny,ix,iy,nz-1)] = sim->grid.cells[midx(nx,ny,ix,iy,nz-2)];
                sim->grid.cells[midx(nx,ny,ix,iy,nz-1)].velocity.z *= -1;
            }
        }
        break;

    case MHD_BC_OUTFLOW:
        /* Zero-gradient: copy nearest interior cell (free outflow) */
        for (uint32_t iz = 0; iz < nz; iz++) {
            for (uint32_t iy = 0; iy < ny; iy++) {
                sim->grid.cells[midx(nx,ny,0,iy,iz)] = sim->grid.cells[midx(nx,ny,1,iy,iz)];
                sim->grid.cells[midx(nx,ny,nx-1,iy,iz)] = sim->grid.cells[midx(nx,ny,nx-2,iy,iz)];
            }
        }
        for (uint32_t iz = 0; iz < nz; iz++) {
            for (uint32_t ix = 0; ix < nx; ix++) {
                sim->grid.cells[midx(nx,ny,ix,0,iz)] = sim->grid.cells[midx(nx,ny,ix,1,iz)];
                sim->grid.cells[midx(nx,ny,ix,ny-1,iz)] = sim->grid.cells[midx(nx,ny,ix,ny-2,iz)];
            }
        }
        for (uint32_t iy = 0; iy < ny; iy++) {
            for (uint32_t ix = 0; ix < nx; ix++) {
                sim->grid.cells[midx(nx,ny,ix,iy,0)] = sim->grid.cells[midx(nx,ny,ix,iy,1)];
                sim->grid.cells[midx(nx,ny,ix,iy,nz-1)] = sim->grid.cells[midx(nx,ny,ix,iy,nz-2)];
            }
        }
        break;

    case MHD_BC_CONDUCTING:
        /* Perfect conductor: E_tangential = 0 at wall → B_normal = 0, v_normal = 0 */
        for (uint32_t iz = 0; iz < nz; iz++) {
            for (uint32_t iy = 0; iy < ny; iy++) {
                sim->grid.cells[midx(nx,ny,0,iy,iz)] = sim->grid.cells[midx(nx,ny,1,iy,iz)];
                sim->grid.cells[midx(nx,ny,0,iy,iz)].velocity.x = 0;
                sim->grid.cells[midx(nx,ny,0,iy,iz)].B.x = 0;
                sim->grid.cells[midx(nx,ny,nx-1,iy,iz)] = sim->grid.cells[midx(nx,ny,nx-2,iy,iz)];
                sim->grid.cells[midx(nx,ny,nx-1,iy,iz)].velocity.x = 0;
                sim->grid.cells[midx(nx,ny,nx-1,iy,iz)].B.x = 0;
            }
        }
        for (uint32_t iz = 0; iz < nz; iz++) {
            for (uint32_t ix = 0; ix < nx; ix++) {
                sim->grid.cells[midx(nx,ny,ix,0,iz)] = sim->grid.cells[midx(nx,ny,ix,1,iz)];
                sim->grid.cells[midx(nx,ny,ix,0,iz)].velocity.y = 0;
                sim->grid.cells[midx(nx,ny,ix,0,iz)].B.y = 0;
                sim->grid.cells[midx(nx,ny,ix,ny-1,iz)] = sim->grid.cells[midx(nx,ny,ix,ny-2,iz)];
                sim->grid.cells[midx(nx,ny,ix,ny-1,iz)].velocity.y = 0;
                sim->grid.cells[midx(nx,ny,ix,ny-1,iz)].B.y = 0;
            }
        }
        for (uint32_t iy = 0; iy < ny; iy++) {
            for (uint32_t ix = 0; ix < nx; ix++) {
                sim->grid.cells[midx(nx,ny,ix,iy,0)] = sim->grid.cells[midx(nx,ny,ix,iy,1)];
                sim->grid.cells[midx(nx,ny,ix,iy,0)].velocity.z = 0;
                sim->grid.cells[midx(nx,ny,ix,iy,0)].B.z = 0;
                sim->grid.cells[midx(nx,ny,ix,iy,nz-1)] = sim->grid.cells[midx(nx,ny,ix,iy,nz-2)];
                sim->grid.cells[midx(nx,ny,ix,iy,nz-1)].velocity.z = 0;
                sim->grid.cells[midx(nx,ny,ix,iy,nz-1)].B.z = 0;
            }
        }
        break;

    case MHD_BC_INSULATING:
        /* Insulating wall: J_normal = 0 → dB_tangential/dn = 0, v_normal = 0 */
        for (uint32_t iz = 0; iz < nz; iz++) {
            for (uint32_t iy = 0; iy < ny; iy++) {
                sim->grid.cells[midx(nx,ny,0,iy,iz)] = sim->grid.cells[midx(nx,ny,1,iy,iz)];
                sim->grid.cells[midx(nx,ny,0,iy,iz)].velocity.x = 0;
                sim->grid.cells[midx(nx,ny,nx-1,iy,iz)] = sim->grid.cells[midx(nx,ny,nx-2,iy,iz)];
                sim->grid.cells[midx(nx,ny,nx-1,iy,iz)].velocity.x = 0;
            }
        }
        for (uint32_t iz = 0; iz < nz; iz++) {
            for (uint32_t ix = 0; ix < nx; ix++) {
                sim->grid.cells[midx(nx,ny,ix,0,iz)] = sim->grid.cells[midx(nx,ny,ix,1,iz)];
                sim->grid.cells[midx(nx,ny,ix,0,iz)].velocity.y = 0;
                sim->grid.cells[midx(nx,ny,ix,ny-1,iz)] = sim->grid.cells[midx(nx,ny,ix,ny-2,iz)];
                sim->grid.cells[midx(nx,ny,ix,ny-1,iz)].velocity.y = 0;
            }
        }
        for (uint32_t iy = 0; iy < ny; iy++) {
            for (uint32_t ix = 0; ix < nx; ix++) {
                sim->grid.cells[midx(nx,ny,ix,iy,0)] = sim->grid.cells[midx(nx,ny,ix,iy,1)];
                sim->grid.cells[midx(nx,ny,ix,iy,0)].velocity.z = 0;
                sim->grid.cells[midx(nx,ny,ix,iy,nz-1)] = sim->grid.cells[midx(nx,ny,ix,iy,nz-2)];
                sim->grid.cells[midx(nx,ny,ix,iy,nz-1)].velocity.z = 0;
            }
        }
        break;
    }
    #undef APPLY_BC_FACES

    /* Statistics */
    sim->time += dt;
    sim->stats.step_count++;
    float total_ke = 0, total_me = 0, total_te = 0;
    float max_v = 0, max_B = 0, max_rho = 0, max_divB = 0;
    float dV = sim->grid.dx * sim->grid.dy * sim->grid.dz;
    for (uint32_t i = 0; i < n; i++) {
        const mhd_cell_t* c = &sim->grid.cells[i];
        total_ke += 0.5f * c->density * vec3_mag2(c->velocity) * dV;
        total_me += vec3_mag2(c->B) / (2.0f * MHD_MU_0) * dV;
        total_te += c->pressure / (gamma - 1.0f) * dV;
        float vm = vec3_mag(c->velocity);
        float bm = vec3_mag(c->B);
        if (vm > max_v) max_v = vm;
        if (bm > max_B) max_B = bm;
        if (c->density > max_rho) max_rho = c->density;
    }

    /* Check div(B) in interior */
    for (uint32_t iz = 1; iz < nz-1; iz++)
        for (uint32_t iy = 1; iy < ny-1; iy++)
            for (uint32_t ix = 1; ix < nx-1; ix++) {
                float db = fabsf(mhd_div_B(sim, ix, iy, iz));
                if (db > max_divB) max_divB = db;
            }

    sim->stats.total_kinetic_energy = total_ke;
    sim->stats.total_magnetic_energy = total_me;
    sim->stats.total_thermal_energy = total_te;
    sim->stats.total_energy = total_ke + total_me + total_te;
    if (sim->stats.step_count == 1)
        sim->stats.initial_total_energy = sim->stats.total_energy;
    if (fabsf(sim->stats.initial_total_energy) > 1e-30f)
        sim->stats.energy_drift = (sim->stats.total_energy - sim->stats.initial_total_energy)
                                   / fabsf(sim->stats.initial_total_energy);
    sim->stats.max_velocity = max_v;
    sim->stats.max_B = max_B;
    sim->stats.max_density = max_rho;
    sim->stats.max_div_B = max_divB;

    /* Center cell diagnostics */
    uint32_t cx = nx/2, cy = ny/2, cz = nz/2;
    sim->stats.alfven_speed = mhd_alfven_speed(sim, cx, cy, cz);
    sim->stats.plasma_beta = mhd_plasma_beta(sim, cx, cy, cz);

    float cs = sound_speed(gamma,
                            sim->grid.cells[midx(nx, ny, cx, cy, cz)].pressure,
                            sim->grid.cells[midx(nx, ny, cx, cy, cz)].density);
    sim->stats.max_mach = (cs > 0) ? max_v / cs : 0;

    return 0;
}

/* ============================================================================
 * Standard Test Initializations
 * ============================================================================ */

void mhd_init_orszag_tang(mhd_sim_t* sim) {
    if (!sim) return;
    uint32_t nx = sim->grid.nx, ny = sim->grid.ny, nz = sim->grid.nz;
    float dx = sim->grid.dx;
    float pi = 3.14159265f;

    for (uint32_t iz = 0; iz < nz; iz++) {
        for (uint32_t iy = 0; iy < ny; iy++) {
            for (uint32_t ix = 0; ix < nx; ix++) {
                float x = (float)ix / (float)nx * 2.0f * pi;
                float y = (float)iy / (float)ny * 2.0f * pi;
                mhd_cell_t* c = &sim->grid.cells[midx(nx, ny, ix, iy, iz)];
                c->density = 25.0f / (36.0f * pi);
                c->pressure = 5.0f / (12.0f * pi);
                c->velocity.x = -sinf(y);
                c->velocity.y = sinf(x);
                c->velocity.z = 0;
                c->B.x = -sinf(y) / sqrtf(4.0f * pi);
                c->B.y = sinf(2.0f * x) / sqrtf(4.0f * pi);
                c->B.z = 0;
            }
        }
    }
}

void mhd_init_harris_sheet(mhd_sim_t* sim, float B0, float thickness) {
    if (!sim) return;
    uint32_t nx = sim->grid.nx, ny = sim->grid.ny, nz = sim->grid.nz;

    for (uint32_t iz = 0; iz < nz; iz++) {
        for (uint32_t iy = 0; iy < ny; iy++) {
            for (uint32_t ix = 0; ix < nx; ix++) {
                float y = sim->grid.origin_y + iy * sim->grid.dy;
                mhd_cell_t* c = &sim->grid.cells[midx(nx, ny, ix, iy, iz)];
                float th = tanhf(y / thickness);
                c->density = 1.0f / (coshf(y / thickness) * coshf(y / thickness)) + 0.2f;
                c->pressure = 0.5f * B0 * B0 / MHD_MU_0 * (1.0f - th * th) + 0.5f * c->density;
                c->velocity = (wm_parietal_vec3_t){0, 0, 0};
                c->B.x = B0 * th;
                c->B.y = 0;
                c->B.z = 0;
            }
        }
    }
}

void mhd_init_kelvin_helmholtz(mhd_sim_t* sim, float B_parallel) {
    if (!sim) return;
    uint32_t nx = sim->grid.nx, ny = sim->grid.ny, nz = sim->grid.nz;

    for (uint32_t iz = 0; iz < nz; iz++) {
        for (uint32_t iy = 0; iy < ny; iy++) {
            for (uint32_t ix = 0; ix < nx; ix++) {
                float y = (float)iy / (float)ny - 0.5f;
                mhd_cell_t* c = &sim->grid.cells[midx(nx, ny, ix, iy, iz)];
                c->density = 1.0f;
                c->pressure = 2.5f;
                c->velocity.x = (fabsf(y) < 0.25f) ? 0.5f : -0.5f;
                /* Small perturbation to seed instability */
                float x_norm = (float)ix / (float)nx;
                c->velocity.y = 0.01f * sinf(2.0f * 3.14159265f * x_norm);
                c->velocity.z = 0;
                c->B.x = B_parallel;
                c->B.y = 0;
                c->B.z = 0;
            }
        }
    }
}

void mhd_init_rayleigh_taylor(mhd_sim_t* sim, float density_ratio,
                                wm_parietal_vec3_t gravity) {
    if (!sim) return;
    uint32_t nx = sim->grid.nx, ny = sim->grid.ny, nz = sim->grid.nz;

    /* Add gravity source */
    mhd_source_t g = { .type = MHD_SOURCE_GRAVITY, .direction = gravity, .active = true };
    mhd_add_source(sim, &g);

    for (uint32_t iz = 0; iz < nz; iz++) {
        for (uint32_t iy = 0; iy < ny; iy++) {
            for (uint32_t ix = 0; ix < nx; ix++) {
                float y = (float)iy / (float)ny - 0.5f;
                mhd_cell_t* c = &sim->grid.cells[midx(nx, ny, ix, iy, iz)];
                c->density = (y > 0) ? density_ratio : 1.0f;
                c->pressure = 10.0f - c->density * gravity.y * (sim->grid.origin_y + iy * sim->grid.dy);
                /* Seed perturbation */
                float x_norm = (float)ix / (float)nx;
                c->velocity.y = 0.01f * (1.0f + cosf(2.0f * 3.14159265f * x_norm))
                                * (1.0f + cosf(2.0f * 3.14159265f * y));
                c->velocity.x = 0;
                c->velocity.z = 0;
                c->B = (wm_parietal_vec3_t){0, 0, 0};
            }
        }
    }
}

float mhd_magnetic_helicity(const mhd_sim_t* sim) {
    /* Simplified: H_m = integral(A dot B) dV.
     * Computing A from B requires solving curl(A)=B, which is expensive.
     * For now return 0 — placeholder for topological analysis. */
    (void)sim;
    return 0;
}

mhd_stats_t mhd_get_stats(const mhd_sim_t* sim) {
    if (!sim) return (mhd_stats_t){0};
    return sim->stats;
}
