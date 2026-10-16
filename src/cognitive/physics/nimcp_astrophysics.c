/**
 * @file nimcp_astrophysics.c
 * @brief Astrophysics simulator — N-body gravity, stellar evolution, orbital mechanics
 *
 * Implements N-body gravitational dynamics (direct O(n^2) summation), Kepler
 * orbital period, orbital/escape velocity, Schwarzschild radius, stellar
 * luminosity, main sequence lifetime, Hubble velocity, redshift, apparent
 * magnitude, Hill sphere, Roche limit, tidal force, Solar System loading,
 * binary star loading, stellar evolution staging, and energy drift tracking.
 */

#include "cognitive/physics/nimcp_astrophysics.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>
#include <math.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define LOG_TAG "ASTRO"

/** Softening to prevent singularities in gravity (default 1 AU) */
#define DEFAULT_SOFTENING   (1.0e9f)

/** Solar main sequence lifetime ~10 Gyr */
#define SOLAR_LIFETIME      (1.0e10f)

/** Pi */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * Default config
 * ============================================================================ */

astro_config_t astrophysics_default_config(void)
{
    astro_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.dt = 86400.0f;  /* 1 day in seconds */
    cfg.softening_length = DEFAULT_SOFTENING;
    cfg.enable_stellar_evolution = true;
    cfg.enable_cosmology = false;
    cfg.hubble_constant = ASTRO_HUBBLE;
    return cfg;
}

/* ============================================================================
 * Create / Destroy
 * ============================================================================ */

astrophysics_sim_t* astrophysics_create(const astro_config_t* config)
{
    astrophysics_sim_t* sim =
        (astrophysics_sim_t*)nimcp_calloc(1, sizeof(astrophysics_sim_t));
    if (!sim) {
        LOG_ERROR(LOG_TAG, "Failed to allocate astrophysics_sim_t");
        return NULL;
    }
    sim->config = config ? *config : astrophysics_default_config();
    sim->stats.initial_total_energy = 0.0;
    sim->initialized = true;
    LOG_INFO(LOG_TAG, "Astrophysics sim created (dt=%.0fs)", sim->config.dt);
    return sim;
}

void astrophysics_destroy(astrophysics_sim_t* sim)
{
    if (!sim) return;
    LOG_INFO(LOG_TAG, "Destroying astrophysics sim (steps=%lu, bodies=%u)",
             (unsigned long)sim->stats.step_count, sim->num_bodies);
    nimcp_free(sim);
}

/* ============================================================================
 * Add body
 * ============================================================================ */

uint32_t astrophysics_add_body(astrophysics_sim_t* sim, const astro_body_t* body)
{
    if (!sim || !body) return UINT32_MAX;
    if (sim->num_bodies >= ASTRO_MAX_BODIES) {
        LOG_WARN(LOG_TAG, "Max bodies reached (%d)", ASTRO_MAX_BODIES);
        return UINT32_MAX;
    }
    uint32_t idx = sim->num_bodies++;
    sim->bodies[idx] = *body;
    sim->bodies[idx].id = idx;
    sim->bodies[idx].active = true;
    return idx;
}

/* ============================================================================
 * Analytical functions
 * ============================================================================ */

/**
 * Kepler's third law: T = 2*pi*sqrt(a^3 / (G*M))
 */
float astro_orbital_period(float semi_major_axis, float central_mass)
{
    if (central_mass <= 0.0f || semi_major_axis <= 0.0f) return 0.0f;
    float a3 = semi_major_axis * semi_major_axis * semi_major_axis;
    return 2.0f * (float)M_PI * sqrtf(a3 / (ASTRO_G * central_mass));
}

/**
 * Circular orbital velocity: v = sqrt(G*M / r)
 */
float astro_orbital_velocity(float central_mass, float radius)
{
    if (central_mass <= 0.0f || radius <= 0.0f) return 0.0f;
    return sqrtf(ASTRO_G * central_mass / radius);
}

/**
 * Escape velocity: v_esc = sqrt(2*G*M / r)
 */
float astro_escape_velocity(float mass, float radius)
{
    if (mass <= 0.0f || radius <= 0.0f) return 0.0f;
    return sqrtf(2.0f * ASTRO_G * mass / radius);
}

/**
 * Schwarzschild radius: r_s = 2*G*M / c^2
 */
float astro_schwarzschild_radius(float mass)
{
    return 2.0f * ASTRO_G * mass / (ASTRO_C * ASTRO_C);
}

/**
 * Gravitational time dilation: dt_proper/dt_coord = sqrt(1 - r_s/r)
 */
float astro_gravitational_time_dilation(float mass, float distance)
{
    if (distance <= 0.0f) return 0.0f;
    float rs = astro_schwarzschild_radius(mass);
    float ratio = rs / distance;
    if (ratio >= 1.0f) return 0.0f;  /* at or inside event horizon */
    return sqrtf(1.0f - ratio);
}

/**
 * Tidal force: F_tidal = 2*G*M*m*r / d^3
 */
float astro_tidal_force(float M, float m, float r, float d)
{
    if (d <= 0.0f) return 0.0f;
    return 2.0f * ASTRO_G * M * m * r / (d * d * d);
}

/**
 * Stefan-Boltzmann luminosity: L = 4*pi*R^2*sigma*T^4
 */
float astro_luminosity(float radius, float temperature)
{
    if (radius <= 0.0f || temperature <= 0.0f) return 0.0f;
    float T4 = temperature * temperature * temperature * temperature;
    return 4.0f * (float)M_PI * radius * radius * ASTRO_STEFAN_BOLTZMANN * T4;
}

/**
 * Main sequence lifetime: t = t_sun * (M/M_sun)^(-2.5)
 * Higher mass stars burn faster.
 */
float astro_main_sequence_lifetime(float mass)
{
    if (mass <= 0.0f) return 0.0f;
    float ratio = mass / ASTRO_SOLAR_MASS;
    if (ratio <= 0.0f) return 0.0f;
    return SOLAR_LIFETIME * powf(ratio, -2.5f);
}

/**
 * Hubble's law: v = H0 * d
 */
float astro_hubble_velocity(float distance, float H0)
{
    return H0 * distance;
}

/**
 * Cosmological redshift from recession velocity (non-relativistic):
 * z = v/c (for v << c)
 */
float astro_redshift_from_velocity(float velocity)
{
    return velocity / ASTRO_C;
}

/**
 * Distance modulus: m = M + 5*log10(d/10pc)
 * @param absolute_mag  Absolute magnitude M
 * @param distance_pc   Distance in parsecs
 */
float astro_apparent_magnitude(float absolute_mag, float distance_pc)
{
    if (distance_pc <= 0.0f) return absolute_mag;
    return absolute_mag + 5.0f * log10f(distance_pc / 10.0f);
}

/**
 * Hill sphere radius: r_H = a * (m / (3*M))^(1/3)
 */
float astro_hill_radius(float semi_major, float m_body, float M_central)
{
    if (M_central <= 0.0f || semi_major <= 0.0f) return 0.0f;
    return semi_major * cbrtf(m_body / (3.0f * M_central));
}

/**
 * Roche limit: d = R * (2 * rho_primary / rho_secondary)^(1/3)
 */
float astro_roche_limit(float R_primary, float rho_primary, float rho_secondary)
{
    if (rho_secondary <= 0.0f) return 0.0f;
    return R_primary * cbrtf(2.0f * rho_primary / rho_secondary);
}

/* ============================================================================
 * Internal: compute total energy
 * ============================================================================ */

static double compute_total_energy(const astrophysics_sim_t* sim)
{
    double KE = 0.0, PE = 0.0;
    for (uint32_t i = 0; i < sim->num_bodies; i++) {
        if (!sim->bodies[i].active) continue;
        const astro_body_t* bi = &sim->bodies[i];
        float vx = bi->velocity.x, vy = bi->velocity.y, vz = bi->velocity.z;
        float v2 = vx*vx + vy*vy + vz*vz;
        KE += 0.5 * (double)bi->mass * (double)v2;

        for (uint32_t j = i + 1; j < sim->num_bodies; j++) {
            if (!sim->bodies[j].active) continue;
            const astro_body_t* bj = &sim->bodies[j];
            float dx = bj->position.x - bi->position.x;
            float dy = bj->position.y - bi->position.y;
            float dz = bj->position.z - bi->position.z;
            float r = sqrtf(dx*dx + dy*dy + dz*dz + sim->config.softening_length *
                            sim->config.softening_length);
            PE -= (double)ASTRO_G * (double)bi->mass * (double)bj->mass / (double)r;
        }
    }
    return KE + PE;
}

/* ============================================================================
 * N-body step — leapfrog/kick-drift-kick integrator
 * ============================================================================ */

int astrophysics_step(astrophysics_sim_t* sim, float dt)
{
    if (!sim || !sim->initialized) return -1;
    if (dt <= 0.0f) dt = sim->config.dt;
    if (dt <= 0.0f) dt = 86400.0f;

    uint32_t N = sim->num_bodies;
    float eps2 = sim->config.softening_length * sim->config.softening_length;
    float half_dt = dt * 0.5f;
    float max_vel = 0.0f;

    /* --- Kick (half step): compute accelerations and update velocities by dt/2 --- */
    for (uint32_t i = 0; i < N; i++) {
        if (!sim->bodies[i].active) continue;
        astro_body_t* bi = &sim->bodies[i];
        float ax = 0.0f, ay = 0.0f, az = 0.0f;

        for (uint32_t j = 0; j < N; j++) {
            if (j == i || !sim->bodies[j].active) continue;
            const astro_body_t* bj = &sim->bodies[j];
            float dx = bj->position.x - bi->position.x;
            float dy = bj->position.y - bi->position.y;
            float dz = bj->position.z - bi->position.z;
            float r2 = dx*dx + dy*dy + dz*dz + eps2;
            float r = sqrtf(r2);
            float r3 = r2 * r;
            if (r3 < 1e-30f) continue;
            float f = ASTRO_G * bj->mass / r3;
            ax += f * dx;
            ay += f * dy;
            az += f * dz;
        }

        bi->velocity.x += ax * half_dt;
        bi->velocity.y += ay * half_dt;
        bi->velocity.z += az * half_dt;
    }

    /* --- Drift: update positions by full dt --- */
    for (uint32_t i = 0; i < N; i++) {
        if (!sim->bodies[i].active) continue;
        astro_body_t* bi = &sim->bodies[i];
        bi->position.x += bi->velocity.x * dt;
        bi->position.y += bi->velocity.y * dt;
        bi->position.z += bi->velocity.z * dt;
    }

    /* --- Kick (second half): recompute accelerations and update velocities by dt/2 --- */
    for (uint32_t i = 0; i < N; i++) {
        if (!sim->bodies[i].active) continue;
        astro_body_t* bi = &sim->bodies[i];
        float ax = 0.0f, ay = 0.0f, az = 0.0f;

        for (uint32_t j = 0; j < N; j++) {
            if (j == i || !sim->bodies[j].active) continue;
            const astro_body_t* bj = &sim->bodies[j];
            float dx = bj->position.x - bi->position.x;
            float dy = bj->position.y - bi->position.y;
            float dz = bj->position.z - bi->position.z;
            float r2 = dx*dx + dy*dy + dz*dz + eps2;
            float r = sqrtf(r2);
            float r3 = r2 * r;
            if (r3 < 1e-30f) continue;
            float f = ASTRO_G * bj->mass / r3;
            ax += f * dx;
            ay += f * dy;
            az += f * dz;
        }

        bi->velocity.x += ax * half_dt;
        bi->velocity.y += ay * half_dt;
        bi->velocity.z += az * half_dt;

        float v2 = bi->velocity.x * bi->velocity.x +
                    bi->velocity.y * bi->velocity.y +
                    bi->velocity.z * bi->velocity.z;
        float v = sqrtf(v2);
        if (v > max_vel) max_vel = v;
    }

    /* --- Stellar evolution --- */
    if (sim->config.enable_stellar_evolution) {
        float dt_years = dt / (365.25f * 86400.0f);
        for (uint32_t i = 0; i < N; i++) {
            astro_body_t* b = &sim->bodies[i];
            if (!b->active || b->type != ASTRO_TYPE_STAR) continue;
            b->age += dt_years;

            /* Advance stellar stage based on age vs main_sequence_lifetime */
            if (b->stage == ASTRO_STAGE_MAIN_SEQUENCE &&
                b->age > b->main_sequence_lifetime) {
                float mass_ratio = b->mass / ASTRO_SOLAR_MASS;
                if (mass_ratio > 8.0f) {
                    b->stage = ASTRO_STAGE_SUPERNOVA;
                    LOG_INFO(LOG_TAG, "Star '%s' goes supernova (mass=%.1f Msun)",
                             b->name, mass_ratio);
                } else {
                    b->stage = ASTRO_STAGE_RED_GIANT;
                    b->radius *= 100.0f;
                    b->temperature *= 0.5f;
                    LOG_INFO(LOG_TAG, "Star '%s' enters red giant phase", b->name);
                }
            } else if (b->stage == ASTRO_STAGE_RED_GIANT &&
                       b->age > b->main_sequence_lifetime * 1.1f) {
                float mass_ratio = b->mass / ASTRO_SOLAR_MASS;
                if (mass_ratio < 8.0f) {
                    b->stage = ASTRO_STAGE_PLANETARY_NEBULA;
                } else {
                    b->stage = ASTRO_STAGE_SUPERNOVA;
                }
            } else if (b->stage == ASTRO_STAGE_PLANETARY_NEBULA &&
                       b->age > b->main_sequence_lifetime * 1.2f) {
                b->stage = ASTRO_STAGE_WHITE_DWARF;
                b->type = ASTRO_TYPE_WHITE_DWARF;
                b->radius = ASTRO_SOLAR_RADIUS * 0.01f;
                b->temperature = 10000.0f;
            } else if (b->stage == ASTRO_STAGE_SUPERNOVA &&
                       b->age > b->main_sequence_lifetime * 1.01f) {
                float mass_ratio = b->mass / ASTRO_SOLAR_MASS;
                if (mass_ratio > 25.0f) {
                    b->stage = ASTRO_STAGE_BLACK_HOLE;
                    b->type = ASTRO_TYPE_BLACK_HOLE;
                    b->schwarzschild_radius = astro_schwarzschild_radius(b->mass);
                    b->radius = b->schwarzschild_radius;
                    LOG_INFO(LOG_TAG, "Star '%s' collapses to black hole (rs=%.0fm)",
                             b->name, b->schwarzschild_radius);
                } else {
                    b->stage = ASTRO_STAGE_NEUTRON_STAR;
                    b->type = ASTRO_TYPE_NEUTRON_STAR;
                    b->radius = 10000.0f;  /* ~10 km */
                }
            }

            /* Update luminosity */
            b->luminosity = astro_luminosity(b->radius, b->temperature);
        }
    }

    /* --- Energy tracking (double precision) --- */
    double total_energy = compute_total_energy(sim);
    if (sim->stats.step_count == 0) {
        sim->stats.initial_total_energy = total_energy;
    }
    float KE = 0.0f, PE = 0.0f;
    for (uint32_t i = 0; i < N; i++) {
        if (!sim->bodies[i].active) continue;
        float v2 = sim->bodies[i].velocity.x * sim->bodies[i].velocity.x +
                    sim->bodies[i].velocity.y * sim->bodies[i].velocity.y +
                    sim->bodies[i].velocity.z * sim->bodies[i].velocity.z;
        KE += 0.5f * sim->bodies[i].mass * v2;
    }
    PE = (float)(total_energy - (double)KE);

    sim->stats.step_count++;
    sim->stats.total_kinetic_energy = KE;
    sim->stats.total_potential_energy = PE;
    sim->stats.total_energy = (float)total_energy;
    sim->stats.energy_drift = (sim->stats.initial_total_energy != 0.0)
        ? (float)((total_energy - sim->stats.initial_total_energy) /
                  fabs(sim->stats.initial_total_energy))
        : 0.0f;
    sim->stats.active_bodies = 0;
    for (uint32_t i = 0; i < N; i++) {
        if (sim->bodies[i].active) sim->stats.active_bodies++;
    }
    sim->stats.max_velocity = max_vel;
    sim->time += (double)dt;

    return 0;
}

/* ============================================================================
 * Load Solar System: Sun + Mercury through Neptune
 * Real masses (kg), distances (m from Sun), orbital velocities (m/s).
 * Initial positions on +x axis, velocities on +y (circular orbits).
 * ============================================================================ */

void astrophysics_load_solar_system(astrophysics_sim_t* sim)
{
    if (!sim) return;
    LOG_INFO(LOG_TAG, "Loading Solar System (Sun + 8 planets)");

    struct {
        const char*       name;
        astro_body_type_t type;
        float mass;
        float radius;
        float distance;  /* from Sun */
        float velocity;  /* orbital */
        float temperature;
    } bodies[] = {
        { "Sun",     ASTRO_TYPE_STAR,   1.989e30f, 6.957e8f,  0.0f,         0.0f,     5778.0f },
        { "Mercury", ASTRO_TYPE_PLANET, 3.301e23f, 2.440e6f,  5.791e10f,    47360.0f, 440.0f  },
        { "Venus",   ASTRO_TYPE_PLANET, 4.867e24f, 6.052e6f,  1.082e11f,    35020.0f, 737.0f  },
        { "Earth",   ASTRO_TYPE_PLANET, 5.972e24f, 6.371e6f,  1.496e11f,    29780.0f, 288.0f  },
        { "Mars",    ASTRO_TYPE_PLANET, 6.417e23f, 3.390e6f,  2.279e11f,    24070.0f, 210.0f  },
        { "Jupiter", ASTRO_TYPE_PLANET, 1.898e27f, 6.991e7f,  7.786e11f,    13070.0f, 165.0f  },
        { "Saturn",  ASTRO_TYPE_PLANET, 5.683e26f, 5.823e7f,  1.434e12f,    9680.0f,  134.0f  },
        { "Uranus",  ASTRO_TYPE_PLANET, 8.681e25f, 2.536e7f,  2.871e12f,    6800.0f,  76.0f   },
        { "Neptune", ASTRO_TYPE_PLANET, 1.024e26f, 2.462e7f,  4.495e12f,    5430.0f,  72.0f   },
    };

    for (int i = 0; i < 9; i++) {
        astro_body_t b;
        memset(&b, 0, sizeof(b));
        strncpy(b.name, bodies[i].name, ASTRO_MAX_NAME - 1);
        b.type = bodies[i].type;
        b.mass = bodies[i].mass;
        b.radius = bodies[i].radius;
        b.temperature = bodies[i].temperature;

        /* Position on +x axis, velocity on +y (circular orbit) */
        b.position.x = bodies[i].distance;
        b.position.y = 0.0f;
        b.position.z = 0.0f;
        b.velocity.x = 0.0f;
        b.velocity.y = bodies[i].velocity;
        b.velocity.z = 0.0f;

        b.semi_major_axis = bodies[i].distance;
        b.parent_body = (i == 0) ? UINT32_MAX : 0;  /* planets orbit Sun */

        if (b.type == ASTRO_TYPE_STAR) {
            b.luminosity = ASTRO_SOLAR_LUMINOSITY;
            b.stage = ASTRO_STAGE_MAIN_SEQUENCE;
            b.spectral = ASTRO_SPECTRAL_G;
            b.main_sequence_lifetime = SOLAR_LIFETIME;
            b.age = 4.6e9f;
        }

        if (bodies[i].distance > 0.0f) {
            b.orbital_period = astro_orbital_period(bodies[i].distance, bodies[0].mass);
        }

        astrophysics_add_body(sim, &b);
    }

    LOG_INFO(LOG_TAG, "Solar System loaded: %u bodies", sim->num_bodies);
}

/* ============================================================================
 * Load binary star system
 * ============================================================================ */

void astrophysics_load_binary_star(astrophysics_sim_t* sim, float m1, float m2,
                                    float separation)
{
    if (!sim) return;
    LOG_INFO(LOG_TAG, "Loading binary star (m1=%.2e, m2=%.2e, sep=%.2e)",
             m1, m2, separation);

    float total_mass = m1 + m2;
    float r1 = separation * m2 / total_mass;  /* distance of m1 from COM */
    float r2 = separation * m1 / total_mass;

    /* Orbital velocity for circular orbit */
    float v1 = sqrtf(ASTRO_G * m2 * m2 / (total_mass * separation));
    float v2 = sqrtf(ASTRO_G * m1 * m1 / (total_mass * separation));

    astro_body_t s1, s2;
    memset(&s1, 0, sizeof(s1));
    memset(&s2, 0, sizeof(s2));

    strncpy(s1.name, "Star_A", ASTRO_MAX_NAME - 1);
    s1.type = ASTRO_TYPE_STAR;
    s1.mass = m1;
    s1.radius = ASTRO_SOLAR_RADIUS * powf(m1 / ASTRO_SOLAR_MASS, 0.8f);
    s1.temperature = ASTRO_SOLAR_TEMP * powf(m1 / ASTRO_SOLAR_MASS, 0.5f);
    s1.position.x = -r1;
    s1.velocity.y = -v1;
    s1.stage = ASTRO_STAGE_MAIN_SEQUENCE;
    s1.main_sequence_lifetime = astro_main_sequence_lifetime(m1);

    strncpy(s2.name, "Star_B", ASTRO_MAX_NAME - 1);
    s2.type = ASTRO_TYPE_STAR;
    s2.mass = m2;
    s2.radius = ASTRO_SOLAR_RADIUS * powf(m2 / ASTRO_SOLAR_MASS, 0.8f);
    s2.temperature = ASTRO_SOLAR_TEMP * powf(m2 / ASTRO_SOLAR_MASS, 0.5f);
    s2.position.x = r2;
    s2.velocity.y = v2;
    s2.stage = ASTRO_STAGE_MAIN_SEQUENCE;
    s2.main_sequence_lifetime = astro_main_sequence_lifetime(m2);

    astrophysics_add_body(sim, &s1);
    astrophysics_add_body(sim, &s2);
}

/* ============================================================================
 * Stats
 * ============================================================================ */

astro_stats_t astrophysics_get_stats(const astrophysics_sim_t* sim)
{
    astro_stats_t zero;
    memset(&zero, 0, sizeof(zero));
    if (!sim) return zero;
    return sim->stats;
}
