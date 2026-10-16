/**
 * @file test_astrophysics.cpp
 * @brief Tests for the Astrophysics Engine -- orbital mechanics, stellar physics, black holes
 *
 * Verifies: Kepler T^2 ~ a^3, escape velocity, Schwarzschild radius,
 * luminosity, main sequence lifetime, Solar System loader, N-body energy conservation.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "cognitive/physics/nimcp_astrophysics.h"
}

/* ---- Fixture ---- */

class AstrophysicsTest : public ::testing::Test {
protected:
    void SetUp() override {
        cfg = astrophysics_default_config();
        sim = astrophysics_create(&cfg);
    }
    void TearDown() override {
        if (sim) astrophysics_destroy(sim);
    }
    astro_config_t cfg;
    astrophysics_sim_t* sim;
};

/* ---- Tests ---- */

TEST(AstrophysicsBasic, CreateDestroy) {
    astro_config_t cfg = astrophysics_default_config();
    astrophysics_sim_t* sim = astrophysics_create(&cfg);
    ASSERT_NE(sim, nullptr);
    EXPECT_TRUE(sim->initialized);
    EXPECT_EQ(sim->num_bodies, 0);
    astrophysics_destroy(sim);
}

TEST(AstrophysicsKepler, ThirdLaw) {
    /* T^2 = 4*pi^2 * a^3 / (G*M)
     * For Earth: a = 1 AU = 1.496e11 m, M_sun = 1.989e30 kg
     * T = 1 year = 3.156e7 s */
    float a = ASTRO_AU;
    float T = astro_orbital_period(a, ASTRO_SOLAR_MASS);
    float T_year = 3.156e7f;
    EXPECT_NEAR(T, T_year, T_year * 0.02f);  /* 2% tolerance */
}

TEST(AstrophysicsKepler, Proportionality) {
    /* T^2 proportional to a^3 => doubling a multiplies T by 2*sqrt(2) ~ 2.828 */
    float T1 = astro_orbital_period(1.0e11f, ASTRO_SOLAR_MASS);
    float T2 = astro_orbital_period(2.0e11f, ASTRO_SOLAR_MASS);
    float ratio = T2 / T1;
    float expected = powf(2.0f, 1.5f);  /* 2^(3/2) = 2.828 */
    EXPECT_NEAR(ratio, expected, 0.05f);
}

TEST(AstrophysicsEscape, EscapeVelocityEarth) {
    /* v_esc = sqrt(2*G*M/R)
     * Earth: M = 5.972e24 kg, R = 6.371e6 m
     * v_esc ~ 11,186 m/s ~ 11.2 km/s */
    float M_earth = 5.972e24f;
    float R_earth = 6.371e6f;
    float v_esc = astro_escape_velocity(M_earth, R_earth);
    EXPECT_NEAR(v_esc, 11186.0f, 200.0f);  /* ~11.2 km/s */
}

TEST(AstrophysicsBlackHole, SchwarzschildSun) {
    /* r_s = 2*G*M/c^2
     * Sun: r_s = 2 * 6.674e-11 * 1.989e30 / (3e8)^2 ~ 2954 m ~ 2.95 km */
    float r_s = astro_schwarzschild_radius(ASTRO_SOLAR_MASS);
    EXPECT_NEAR(r_s, 2954.0f, 50.0f);
}

TEST(AstrophysicsStellar, LuminositySun) {
    /* L = 4*pi*R^2 * sigma * T^4
     * Sun: R = 6.957e8 m, T = 5778 K
     * L ~ 3.828e26 W */
    float L = astro_luminosity(ASTRO_SOLAR_RADIUS, ASTRO_SOLAR_TEMP);
    EXPECT_NEAR(L, ASTRO_SOLAR_LUMINOSITY, ASTRO_SOLAR_LUMINOSITY * 0.05f);
}

TEST(AstrophysicsStellar, MainSequenceLifetimeSun) {
    /* t_MS ~ t_sun * (M/M_sun)^(-2.5)
     * For the Sun itself, t = t_sun ~ 10 Gyr = 1e10 years */
    float t = astro_main_sequence_lifetime(ASTRO_SOLAR_MASS);
    float t_10Gyr = 1.0e10f * 3.156e7f;  /* 10 Gyr in seconds */
    /* Very rough -- within factor of 2 */
    EXPECT_NEAR(t, t_10Gyr, t_10Gyr * 0.5f);
}

TEST(AstrophysicsStellar, MassiveStarShorterLife) {
    /* More massive stars have shorter lifetimes */
    float t_sun = astro_main_sequence_lifetime(ASTRO_SOLAR_MASS);
    float t_heavy = astro_main_sequence_lifetime(10.0f * ASTRO_SOLAR_MASS);
    EXPECT_GT(t_sun, t_heavy);
}

TEST_F(AstrophysicsTest, LoadSolarSystem) {
    astrophysics_load_solar_system(sim);
    /* Sun + 8 planets = 9 bodies minimum */
    ASSERT_GE(sim->num_bodies, 9u);

    /* Sun should be the most massive body */
    float max_mass = 0.0f;
    uint32_t sun_idx = 0;
    for (uint32_t i = 0; i < sim->num_bodies; i++) {
        if (sim->bodies[i].mass > max_mass) {
            max_mass = sim->bodies[i].mass;
            sun_idx = i;
        }
    }
    EXPECT_NEAR(max_mass, ASTRO_SOLAR_MASS, ASTRO_SOLAR_MASS * 0.01f);
    EXPECT_EQ(sim->bodies[sun_idx].type, ASTRO_TYPE_STAR);
}

TEST(AstrophysicsNBody, EnergyConservation) {
    astro_config_t cfg = astrophysics_default_config();
    cfg.dt = 86400.0f;  /* 1 day in seconds */
    cfg.softening_length = 1.0e6f;
    astrophysics_sim_t* sim = astrophysics_create(&cfg);
    ASSERT_NE(sim, nullptr);

    /* Simple two-body: star + planet in circular orbit */
    astro_body_t star;
    memset(&star, 0, sizeof(star));
    star.type = ASTRO_TYPE_STAR;
    star.mass = ASTRO_SOLAR_MASS;
    star.position = (wm_parietal_vec3_t){0, 0, 0};
    star.velocity = (wm_parietal_vec3_t){0, 0, 0};
    star.active = true;
    astrophysics_add_body(sim, &star);

    float orbital_r = ASTRO_AU;
    float v_circ = astro_orbital_velocity(ASTRO_SOLAR_MASS, orbital_r);
    astro_body_t planet;
    memset(&planet, 0, sizeof(planet));
    planet.type = ASTRO_TYPE_PLANET;
    planet.mass = 5.972e24f;  /* Earth mass */
    planet.position = (wm_parietal_vec3_t){orbital_r, 0, 0};
    planet.velocity = (wm_parietal_vec3_t){0, v_circ, 0};
    planet.active = true;
    astrophysics_add_body(sim, &planet);

    /* Step for 100 days */
    for (int i = 0; i < 100; i++) {
        astrophysics_step(sim, cfg.dt);
    }

    astro_stats_t stats = astrophysics_get_stats(sim);
    /* Energy drift should be small for a symplectic integrator */
    if (stats.initial_total_energy != 0.0) {
        float drift = (float)fabs((double)(stats.total_energy - (float)stats.initial_total_energy)
                                  / stats.initial_total_energy);
        EXPECT_LT(drift, 0.05f);  /* < 5% drift */
    }

    astrophysics_destroy(sim);
}

TEST(AstrophysicsOrbital, OrbitalVelocity) {
    /* v = sqrt(G*M/r) for Earth orbit ~ 29,783 m/s ~ 29.8 km/s */
    float v = astro_orbital_velocity(ASTRO_SOLAR_MASS, ASTRO_AU);
    EXPECT_NEAR(v, 29783.0f, 500.0f);
}

TEST(AstrophysicsGR, GravitationalTimeDilation) {
    /* At Earth's surface: dt_proper/dt_coord = sqrt(1 - 2GM/(rc^2))
     * This is a very small effect: ~1 - 7e-10 */
    float factor = astro_gravitational_time_dilation(5.972e24f, 6.371e6f);
    EXPECT_LT(factor, 1.0f);
    EXPECT_GT(factor, 0.999f);  /* extremely close to 1 */
}

TEST(AstrophysicsOrbital, HillRadius) {
    /* Earth's Hill sphere: r_H = a * (m/(3M))^(1/3)
     * = 1.496e11 * (5.972e24 / (3 * 1.989e30))^(1/3)
     * ~ 1.5e9 m ~ 0.01 AU */
    float r_H = astro_hill_radius(ASTRO_AU, 5.972e24f, ASTRO_SOLAR_MASS);
    EXPECT_NEAR(r_H, 1.5e9f, 0.5e9f);
}
