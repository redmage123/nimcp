/**
 * @file nimcp_acoustics.c
 * @brief Acoustics & Wave Mechanics simulation — wave equation solver
 *
 * WHAT: FDTD solution of d^2p/dt^2 = c^2 * laplacian(p) on 1D/2D grids.
 *       Point/line/plane wave sources, Doppler shifts, standing waves.
 * WHY:  Reasoning about sound, music, speech, sonar, room acoustics.
 * HOW:  Leapfrog time integration, absorbing/rigid boundary conditions,
 *       source injection via soft sources.
 */

#include "cognitive/physics/nimcp_acoustics.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "ACOUSTICS"

/* ============================================================================
 * Grid Helpers
 * ============================================================================ */

static inline uint32_t idx2(uint32_t nx, uint32_t ix, uint32_t iy) {
    return iy * nx + ix;
}

static bool alloc_field(ac_field_t* f, uint32_t nx, uint32_t ny, float dx, float dy) {
    f->nx = nx; f->ny = ny;
    f->dx = dx; f->dy = (ny > 1) ? dy : dx;
    uint32_t total = nx * ny;
    f->p_curr = nimcp_calloc(total, sizeof(float));
    f->p_prev = nimcp_calloc(total, sizeof(float));
    return f->p_curr && f->p_prev;
}

static void free_field(ac_field_t* f) {
    if (f->p_curr) { nimcp_free(f->p_curr); f->p_curr = NULL; }
    if (f->p_prev) { nimcp_free(f->p_prev); f->p_prev = NULL; }
}

/* ============================================================================
 * Default Config
 * ============================================================================ */

ac_config_t ac_default_config(void) {
    ac_config_t c;
    memset(&c, 0, sizeof(c));
    c.dimension         = AC_DIM_2D;
    c.grid_nx           = 128;
    c.grid_ny           = 128;
    c.cell_size         = 0.01f;        /* 1 cm cells */
    c.speed_of_sound    = AC_SPEED_AIR_20C;
    c.medium_density    = AC_AIR_DENSITY;
    c.boundary          = AC_BC_ABSORBING;
    c.absorption_coeff  = 0.001f;
    c.enable_doppler    = false;
    /* CFL-safe dt: dt = dx / (c * sqrt(dim)) * safety */
    c.dt = 0.01f / (343.0f * 1.5f);    /* ~1.94e-5 s */
    return c;
}

/* ============================================================================
 * Create / Destroy
 * ============================================================================ */

acoustics_sim_t* ac_create(const ac_config_t* config) {
    ac_config_t cfg = config ? *config : ac_default_config();
    if (cfg.grid_nx > AC_MAX_GRID_DIM) cfg.grid_nx = AC_MAX_GRID_DIM;
    if (cfg.grid_ny > AC_MAX_GRID_DIM) cfg.grid_ny = AC_MAX_GRID_DIM;
    if (cfg.dimension == AC_DIM_1D) cfg.grid_ny = 1;

    acoustics_sim_t* sim = nimcp_calloc(1, sizeof(acoustics_sim_t));
    if (!sim) return NULL;

    sim->config = cfg;
    if (!alloc_field(&sim->field, cfg.grid_nx, cfg.grid_ny, cfg.cell_size, cfg.cell_size)) {
        ac_destroy(sim);
        return NULL;
    }

    /* Default medium */
    sim->media[0].speed     = cfg.speed_of_sound;
    sim->media[0].density   = cfg.medium_density;
    sim->media[0].absorption = cfg.absorption_coeff;
    snprintf(sim->media[0].name, sizeof(sim->media[0].name), "Air");
    sim->num_media = 1;

    /* Allocate medium map */
    uint32_t total = cfg.grid_nx * cfg.grid_ny;
    sim->medium_map = nimcp_calloc(total, sizeof(uint8_t));
    if (!sim->medium_map) { ac_destroy(sim); return NULL; }

    sim->initialized = true;
    LOG_INFO(LOG_TAG, "Created %s acoustics sim %ux%u (c=%.0f m/s)",
             cfg.dimension == AC_DIM_1D ? "1D" : "2D", cfg.grid_nx, cfg.grid_ny,
             cfg.speed_of_sound);
    return sim;
}

void ac_destroy(acoustics_sim_t* sim) {
    if (!sim) return;
    free_field(&sim->field);
    if (sim->medium_map) nimcp_free(sim->medium_map);
    nimcp_free(sim);
}

/* ============================================================================
 * Source Injection
 * ============================================================================ */

static float source_value(const ac_source_t* src, float t) {
    float omega = 2.0f * (float)M_PI * src->frequency;
    switch (src->waveform) {
        case AC_WAVE_SINE:
            return src->amplitude * sinf(omega * t + src->phase);
        case AC_WAVE_GAUSSIAN: {
            /* Gaussian pulse centered at t = 4/f with width 1/f */
            float t0 = 4.0f / (src->frequency + 1e-10f);
            float sigma = 1.0f / (src->frequency + 1e-10f);
            float arg = (t - t0) / sigma;
            return src->amplitude * expf(-0.5f * arg * arg) * sinf(omega * t);
        }
        case AC_WAVE_IMPULSE:
            return (t < 1.0f / (src->frequency + 1e-10f)) ? src->amplitude : 0.0f;
        default:
            return 0.0f;
    }
}

/* ============================================================================
 * Wave Equation Step (Leapfrog FDTD)
 * ============================================================================ */

int ac_step(acoustics_sim_t* sim, float dt) {
    if (!sim || !sim->initialized) return -1;

    uint32_t nx = sim->config.grid_nx;
    uint32_t ny = sim->config.grid_ny;
    float dx = sim->config.cell_size;
    float* pc = sim->field.p_curr;
    float* pp = sim->field.p_prev;

    if (dt <= 0.0f) dt = sim->config.dt;

    /* Allocate p_next */
    uint32_t total = nx * ny;
    float* pn = nimcp_calloc(total, sizeof(float));
    if (!pn) return -1;

    if (ny == 1) {
        /* 1D wave equation: p_next = 2*p_curr - p_prev + (c*dt/dx)^2 * (p[i+1] - 2*p[i] + p[i-1]) */
        for (uint32_t ix = 1; ix < nx - 1; ix++) {
            uint8_t mi = sim->medium_map[ix];
            float c = sim->media[mi].speed;
            float alpha = sim->media[mi].absorption;
            float courant2 = (c * dt / dx) * (c * dt / dx);
            float lap = pc[ix+1] - 2.0f * pc[ix] + pc[ix-1];
            float damping = 1.0f / (1.0f + alpha * dt);
            pn[ix] = damping * (2.0f * pc[ix] - pp[ix] * (1.0f - alpha * dt) + courant2 * lap);
        }
    } else {
        /* 2D wave equation */
        for (uint32_t iy = 1; iy < ny - 1; iy++) {
            for (uint32_t ix = 1; ix < nx - 1; ix++) {
                uint32_t ci = idx2(nx, ix, iy);
                uint8_t mi = sim->medium_map[ci];
                float c = sim->media[mi].speed;
                float alpha = sim->media[mi].absorption;
                float courant2 = (c * dt / dx) * (c * dt / dx);
                float lap = pc[idx2(nx,ix+1,iy)] + pc[idx2(nx,ix-1,iy)]
                          + pc[idx2(nx,ix,iy+1)] + pc[idx2(nx,ix,iy-1)]
                          - 4.0f * pc[ci];
                float damping = 1.0f / (1.0f + alpha * dt);
                pn[ci] = damping * (2.0f * pc[ci] - pp[ci] * (1.0f - alpha * dt) + courant2 * lap);
            }
        }
    }

    /* Boundary conditions */
    switch (sim->config.boundary) {
        case AC_BC_RIGID:
            /* dp/dn = 0: ghost = interior (already zero at boundary from calloc) */
            if (ny == 1) {
                pn[0] = pn[1]; pn[nx-1] = pn[nx-2];
            } else {
                for (uint32_t iy = 0; iy < ny; iy++) {
                    pn[idx2(nx,0,iy)] = pn[idx2(nx,1,iy)];
                    pn[idx2(nx,nx-1,iy)] = pn[idx2(nx,nx-2,iy)];
                }
                for (uint32_t ix = 0; ix < nx; ix++) {
                    pn[idx2(nx,ix,0)] = pn[idx2(nx,ix,1)];
                    pn[idx2(nx,ix,ny-1)] = pn[idx2(nx,ix,ny-2)];
                }
            }
            break;
        case AC_BC_OPEN:
            /* p = 0 at boundary (pressure release) — already zero from calloc */
            break;
        case AC_BC_ABSORBING:
            /* Mur first-order: p_boundary = p_prev_interior + (c*dt-dx)/(c*dt+dx)*(p_next_interior - p_prev_boundary) */
            if (ny == 1) {
                float c = sim->config.speed_of_sound;
                float r = (c * dt - dx) / (c * dt + dx);
                pn[0] = pp[1] + r * (pn[1] - pp[0]);
                pn[nx-1] = pp[nx-2] + r * (pn[nx-2] - pp[nx-1]);
            } else {
                float c = sim->config.speed_of_sound;
                float r = (c * dt - dx) / (c * dt + dx);
                for (uint32_t iy = 0; iy < ny; iy++) {
                    pn[idx2(nx,0,iy)] = pp[idx2(nx,1,iy)] + r * (pn[idx2(nx,1,iy)] - pp[idx2(nx,0,iy)]);
                    pn[idx2(nx,nx-1,iy)] = pp[idx2(nx,nx-2,iy)] + r * (pn[idx2(nx,nx-2,iy)] - pp[idx2(nx,nx-1,iy)]);
                }
                for (uint32_t ix = 0; ix < nx; ix++) {
                    pn[idx2(nx,ix,0)] = pp[idx2(nx,ix,1)] + r * (pn[idx2(nx,ix,1)] - pp[idx2(nx,ix,0)]);
                    pn[idx2(nx,ix,ny-1)] = pp[idx2(nx,ix,ny-2)] + r * (pn[idx2(nx,ix,ny-2)] - pp[idx2(nx,ix,ny-1)]);
                }
            }
            break;
        case AC_BC_PERIODIC:
            if (ny == 1) {
                pn[0] = pn[nx-2]; pn[nx-1] = pn[1];
            } else {
                for (uint32_t iy = 0; iy < ny; iy++) {
                    pn[idx2(nx,0,iy)] = pn[idx2(nx,nx-2,iy)];
                    pn[idx2(nx,nx-1,iy)] = pn[idx2(nx,1,iy)];
                }
                for (uint32_t ix = 0; ix < nx; ix++) {
                    pn[idx2(nx,ix,0)] = pn[idx2(nx,ix,ny-2)];
                    pn[idx2(nx,ix,ny-1)] = pn[idx2(nx,ix,1)];
                }
            }
            break;
    }

    /* Inject sources (soft source: add to pressure) */
    float t = sim->time + dt;
    for (uint32_t si = 0; si < sim->num_sources; si++) {
        ac_source_t* src = &sim->sources[si];
        if (!src->active) continue;

        /* Doppler: shift source position if moving */
        float sx = src->x, sy = src->y;
        if (sim->config.enable_doppler) {
            sx += src->velocity_x * sim->time / dx;
            sy += src->velocity_y * sim->time / dx;
        }

        float val = source_value(src, t);

        if (src->type == AC_SRC_POINT) {
            uint32_t ix = (uint32_t)(sx + 0.5f);
            uint32_t iy = (ny > 1) ? (uint32_t)(sy + 0.5f) : 0;
            if (ix < nx && iy < ny) {
                pn[idx2(nx, ix, iy)] += val;
            }
        } else if (src->type == AC_SRC_LINE && ny > 1) {
            uint32_t ix = (uint32_t)(sx + 0.5f);
            if (ix < nx) {
                for (uint32_t iy = 0; iy < ny; iy++)
                    pn[idx2(nx, ix, iy)] += val;
            }
        } else if (src->type == AC_SRC_PLANE) {
            for (uint32_t i = 0; i < total; i++)
                pn[i] += val;
        }
    }

    /* Rotate buffers: prev <- curr, curr <- next */
    memcpy(pp, pc, total * sizeof(float));
    memcpy(pc, pn, total * sizeof(float));
    nimcp_free(pn);

    /* Statistics */
    sim->time += dt;
    sim->stats.step_count++;
    sim->stats.time = sim->time;
    float max_p = -1e30f, min_p = 1e30f, sum_p2 = 0.0f, sum_e = 0.0f;
    float rho_c2 = sim->config.medium_density * sim->config.speed_of_sound * sim->config.speed_of_sound;
    for (uint32_t i = 0; i < total; i++) {
        if (pc[i] > max_p) max_p = pc[i];
        if (pc[i] < min_p) min_p = pc[i];
        sum_p2 += pc[i] * pc[i];
        sum_e += pc[i] * pc[i] / (rho_c2 + 1e-20f);
    }
    sim->stats.max_pressure = max_p;
    sim->stats.min_pressure = min_p;
    sim->stats.rms_pressure = sqrtf(sum_p2 / (float)total);
    sim->stats.total_energy = sum_e * dx * dx;
    sim->stats.max_spl_db = ac_pressure_to_db(max_p);

    return 0;
}

ac_stats_t ac_get_stats(const acoustics_sim_t* sim) {
    if (!sim) { ac_stats_t s; memset(&s, 0, sizeof(s)); return s; }
    return sim->stats;
}

/* ============================================================================
 * Setup
 * ============================================================================ */

uint32_t ac_add_source(acoustics_sim_t* sim, const ac_source_t* src) {
    if (!sim || sim->num_sources >= AC_MAX_SOURCES) return UINT32_MAX;
    uint32_t idx = sim->num_sources;
    sim->sources[idx] = *src;
    sim->sources[idx].active = true;
    sim->num_sources++;
    return idx;
}

uint32_t ac_add_medium(acoustics_sim_t* sim, const ac_medium_t* medium) {
    if (!sim || sim->num_media >= AC_MAX_MEDIA) return UINT32_MAX;
    uint32_t idx = sim->num_media;
    sim->media[idx] = *medium;
    sim->num_media++;
    return idx;
}

void ac_init_vibrating_string(acoustics_sim_t* sim, float tension,
                               float linear_density, float length) {
    if (!sim || !sim->initialized) return;
    sim->config.dimension = AC_DIM_1D;
    float c = sqrtf(tension / (linear_density + 1e-20f));
    sim->config.speed_of_sound = c;
    sim->media[0].speed = c;
    /* Initialize with first harmonic: p(x,0) = A*sin(pi*x/L) */
    float L = (float)sim->config.grid_nx * sim->config.cell_size;
    (void)length;   /* grid defines actual length */
    for (uint32_t ix = 0; ix < sim->config.grid_nx; ix++) {
        float x = (float)ix * sim->config.cell_size;
        sim->field.p_curr[ix] = sinf((float)M_PI * x / L);
    }
    LOG_INFO(LOG_TAG, "Vibrating string: c=%.1f m/s, L=%.3f m", c, L);
}

void ac_init_organ_pipe(acoustics_sim_t* sim, float pipe_length, bool open_open) {
    if (!sim || !sim->initialized) return;
    sim->config.dimension = AC_DIM_1D;
    if (open_open) {
        sim->config.boundary = AC_BC_OPEN;
    } else {
        /* open-closed: one end open (p=0), one end rigid (dp/dx=0) */
        sim->config.boundary = AC_BC_RIGID;
    }
    /* Inject a broadband source at one end */
    ac_source_t src;
    memset(&src, 0, sizeof(src));
    src.type = AC_SRC_POINT;
    src.waveform = AC_WAVE_GAUSSIAN;
    src.x = 1.0f;
    src.frequency = sim->config.speed_of_sound / (2.0f * pipe_length);
    src.amplitude = 1.0f;
    src.active = true;
    ac_add_source(sim, &src);
    LOG_INFO(LOG_TAG, "Organ pipe: L=%.3f m, f1=%.1f Hz, %s",
             pipe_length, src.frequency, open_open ? "open-open" : "open-closed");
}

void ac_init_room(acoustics_sim_t* sim, float width, float height,
                   float wall_absorption) {
    if (!sim || !sim->initialized) return;
    sim->config.dimension = AC_DIM_2D;
    sim->config.absorption_coeff = wall_absorption;
    sim->config.boundary = AC_BC_RIGID;
    /* Place a source at center */
    ac_source_t src;
    memset(&src, 0, sizeof(src));
    src.type = AC_SRC_POINT;
    src.waveform = AC_WAVE_GAUSSIAN;
    src.x = (float)sim->config.grid_nx * 0.5f;
    src.y = (float)sim->config.grid_ny * 0.5f;
    src.frequency = 1000.0f;
    src.amplitude = 1.0f;
    src.active = true;
    ac_add_source(sim, &src);
    (void)width; (void)height;
    LOG_INFO(LOG_TAG, "Room acoustics: absorption=%.3f", wall_absorption);
}

float ac_get_pressure(const acoustics_sim_t* sim, uint32_t ix, uint32_t iy) {
    if (!sim || !sim->initialized) return 0.0f;
    if (ix >= sim->config.grid_nx || iy >= sim->config.grid_ny) return 0.0f;
    return sim->field.p_curr[idx2(sim->config.grid_nx, ix, iy)];
}

/* ============================================================================
 * Physics Computations
 * ============================================================================ */

float ac_sound_intensity(float pressure_rms, float density, float speed) {
    float Z = density * speed;
    if (Z < 1e-20f) return 0.0f;
    return pressure_rms * pressure_rms / Z;
}

float ac_pressure_to_db(float pressure) {
    float p_abs = fabsf(pressure);
    if (p_abs < 1e-30f) return -999.0f;
    return 20.0f * log10f(p_abs / AC_REF_PRESSURE);
}

float ac_intensity_to_db(float intensity) {
    if (intensity < 1e-30f) return -999.0f;
    return 10.0f * log10f(intensity / AC_REF_INTENSITY);
}

float ac_doppler_shift(float frequency, float speed_of_sound,
                        float v_observer, float v_source) {
    /* f' = f * (c + v_obs) / (c + v_src)
     * Convention: positive v = moving toward source for observer,
     *             positive v = moving away from observer for source */
    float denom = speed_of_sound + v_source;
    if (fabsf(denom) < 1e-10f) return frequency;
    return frequency * (speed_of_sound + v_observer) / denom;
}

float ac_pipe_resonance(float speed_of_sound, float length,
                         uint32_t harmonic, bool open_both_ends) {
    if (length < 1e-10f) return 0.0f;
    if (open_both_ends) {
        /* Open-open: f_n = n * c / (2L) */
        return (float)harmonic * speed_of_sound / (2.0f * length);
    } else {
        /* Open-closed: f_n = (2n-1) * c / (4L), only odd harmonics */
        return (float)(2 * harmonic - 1) * speed_of_sound / (4.0f * length);
    }
}

float ac_string_resonance(float tension, float linear_density,
                           float length, uint32_t harmonic) {
    if (length < 1e-10f || linear_density < 1e-20f) return 0.0f;
    float c = sqrtf(tension / linear_density);
    return (float)harmonic * c / (2.0f * length);
}

uint32_t ac_standing_wave_nodes(float length, uint32_t harmonic,
                                 float* node_positions, uint32_t max_nodes) {
    /* For a string fixed at both ends, nodes at x = k*L/n, k=0..n */
    uint32_t n_nodes = harmonic + 1;
    if (n_nodes > max_nodes) n_nodes = max_nodes;
    for (uint32_t k = 0; k < n_nodes; k++) {
        node_positions[k] = (float)k * length / (float)harmonic;
    }
    return n_nodes;
}

float ac_impedance(float density, float speed) {
    return density * speed;
}

float ac_reflection_coefficient(float Z1, float Z2) {
    float denom = Z2 + Z1;
    if (fabsf(denom) < 1e-20f) return 0.0f;
    return (Z2 - Z1) / denom;
}

float ac_transmission_coefficient(float Z1, float Z2) {
    float denom = Z2 + Z1;
    if (fabsf(denom) < 1e-20f) return 0.0f;
    return 2.0f * Z2 / denom;
}

/* ============================================================================
 * Legacy API
 * ============================================================================ */

acoustics_sim_t* acoustics_create(const acoustics_config_t* c) { return ac_create(c); }
void acoustics_destroy(acoustics_sim_t* s) { ac_destroy(s); }
int acoustics_step(acoustics_sim_t* s, float dt) { return ac_step(s, dt); }
acoustics_config_t acoustics_default_config(void) { return ac_default_config(); }
acoustics_stats_t acoustics_get_stats(const acoustics_sim_t* s) { return ac_get_stats(s); }
