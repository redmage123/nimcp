/**
 * @file test_intuitive_physics.cpp
 * @brief Tests for the Intuitive Physics Engine -- rigid body simulation
 *
 * Verifies: create/destroy, ground plane, ball drop settling, support detection,
 * energy conservation, trajectory prediction, sphere-sphere collision, scene stability.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <cstdlib>

extern "C" {
#include "cognitive/physics/nimcp_intuitive_physics.h"
}

/* Helper: create a sphere object template */
static ip_object_t make_sphere(float x, float y, float z, float radius, float mass) {
    ip_object_t obj;
    memset(&obj, 0, sizeof(obj));
    obj.position = (wm_parietal_vec3_t){x, y, z};
    obj.velocity = (wm_parietal_velocity_t){0, 0, 0};
    obj.orientation = (wm_parietal_quaternion_t){1, 0, 0, 0};
    obj.angular_velocity = (wm_parietal_vec3_t){0, 0, 0};
    obj.mass = mass;
    obj.inv_mass = (mass > 0.0f) ? 1.0f / mass : 0.0f;
    obj.restitution = IP_RESTITUTION_DEFAULT;
    obj.friction = IP_FRICTION_DEFAULT;
    obj.shape.type = IP_SHAPE_SPHERE;
    obj.shape.sphere.radius = radius;
    obj.inv_inertia = (wm_parietal_vec3_t){0, 0, 0};
    if (mass > 0.0f) {
        float I_inv = 2.5f / (mass * radius * radius);
        obj.inv_inertia = (wm_parietal_vec3_t){I_inv, I_inv, I_inv};
    }
    obj.supported_by = UINT32_MAX;
    obj.contained_in = UINT32_MAX;
    obj.is_static = (mass == 0.0f);
    obj.visible = true;
    obj.active = true;
    return obj;
}

/* ---- Tests using fixture for repeated create/destroy ---- */

class IntuitivePhysicsTest : public ::testing::Test {
protected:
    void SetUp() override {
        cfg = intuitive_physics_default_config();
        engine = intuitive_physics_create(&cfg);
    }
    void TearDown() override {
        if (engine) intuitive_physics_destroy(engine);
    }
    ip_config_t cfg;
    intuitive_physics_engine_t* engine;
};

TEST(IntuitivePhysicsBasic, CreateDestroy) {
    ip_config_t cfg = intuitive_physics_default_config();
    intuitive_physics_engine_t* engine = intuitive_physics_create(&cfg);
    ASSERT_NE(engine, nullptr);
    EXPECT_TRUE(engine->initialized);
    EXPECT_EQ(engine->scene.num_objects, 0);
    intuitive_physics_destroy(engine);
}

TEST(IntuitivePhysicsBasic, CreateNullConfig) {
    /* Should use defaults when given NULL */
    intuitive_physics_engine_t* engine = intuitive_physics_create(NULL);
    ASSERT_NE(engine, nullptr);
    EXPECT_TRUE(engine->initialized);
    intuitive_physics_destroy(engine);
}

TEST_F(IntuitivePhysicsTest, AddGround) {
    uint32_t ground_id = intuitive_physics_add_ground(engine);
    ASSERT_NE(ground_id, UINT32_MAX);
    EXPECT_EQ(engine->scene.num_objects, 1);

    ip_object_t* ground = intuitive_physics_get_object(engine, ground_id);
    ASSERT_NE(ground, nullptr);
    EXPECT_EQ(ground->shape.type, IP_SHAPE_PLANE);
    EXPECT_TRUE(ground->is_static);
}

TEST(IntuitivePhysicsSim, BallDropSettles) {
    ip_config_t cfg = intuitive_physics_default_config();
    cfg.dt = 0.01f;
    intuitive_physics_engine_t* engine = intuitive_physics_create(&cfg);
    ASSERT_NE(engine, nullptr);

    intuitive_physics_add_ground(engine);

    /* Drop a ball from y=5.0 with radius=0.5 */
    ip_object_t ball = make_sphere(0.0f, 5.0f, 0.0f, 0.5f, 1.0f);
    uint32_t ball_id = intuitive_physics_add_object(engine, &ball);
    ASSERT_NE(ball_id, UINT32_MAX);

    /* Simulate 500 steps (5 seconds) -- ball should settle near y=radius */
    for (int i = 0; i < 500; i++) {
        int rc = intuitive_physics_step(engine, cfg.dt);
        ASSERT_EQ(rc, 0);
    }

    ip_object_t* settled = intuitive_physics_get_object(engine, ball_id);
    ASSERT_NE(settled, nullptr);
    /* Ball center should be near radius (0.5) above ground (y=0) */
    EXPECT_NEAR(settled->position.y, 0.5f, 0.5f);

    intuitive_physics_destroy(engine);
}

TEST_F(IntuitivePhysicsTest, SupportDetection) {
    intuitive_physics_add_ground(engine);

    /* Place a ball resting on ground */
    ip_object_t ball = make_sphere(0.0f, 0.5f, 0.0f, 0.5f, 1.0f);
    uint32_t ball_id = intuitive_physics_add_object(engine, &ball);

    /* Step a few times to let contact solver establish support */
    for (int i = 0; i < 20; i++) {
        intuitive_physics_step(engine, 0.01f);
    }

    bool supported = intuitive_physics_is_supported(engine, ball_id);
    EXPECT_TRUE(supported);
}

TEST(IntuitivePhysicsSim, EnergyConservation) {
    ip_config_t cfg = intuitive_physics_default_config();
    cfg.dt = 0.001f;  /* small dt for accuracy */
    cfg.restitution_default = 1.0f;  /* perfectly elastic */
    intuitive_physics_engine_t* engine = intuitive_physics_create(&cfg);
    ASSERT_NE(engine, nullptr);

    intuitive_physics_add_ground(engine);

    /* Drop ball from y=2.0, elastic bouncing */
    ip_object_t ball = make_sphere(0.0f, 2.0f, 0.0f, 0.2f, 1.0f);
    ball.restitution = 1.0f;
    intuitive_physics_add_object(engine, &ball);

    /* Step 100 times and check energy drift */
    for (int i = 0; i < 100; i++) {
        intuitive_physics_step(engine, cfg.dt);
    }

    ip_stats_t stats = intuitive_physics_get_stats(engine);
    float total = stats.total_kinetic_energy + stats.total_potential_energy;
    /* Energy should not drift more than 10% for symplectic integrator */
    if (stats.initial_total_energy > 0.0f) {
        float drift = fabsf(total - stats.initial_total_energy) / stats.initial_total_energy;
        EXPECT_LT(drift, 0.10f);
    }

    intuitive_physics_destroy(engine);
}

TEST_F(IntuitivePhysicsTest, PredictTrajectory) {
    ip_object_t ball = make_sphere(0.0f, 10.0f, 0.0f, 0.3f, 1.0f);
    uint32_t ball_id = intuitive_physics_add_object(engine, &ball);

    wm_parietal_trajectory_t traj;
    memset(&traj, 0, sizeof(traj));
    int rc = intuitive_physics_predict_trajectory(engine, ball_id, 1.0f, &traj);
    ASSERT_EQ(rc, 0);
    EXPECT_GT(traj.length, 0);

    /* Last predicted y should be lower than initial y (falling under gravity) */
    if (traj.states && traj.length > 0) {
        float final_y = traj.states[traj.length - 1].position.y;
        EXPECT_LT(final_y, 10.0f);
        free(traj.states);
    }
}

TEST(IntuitivePhysicsSim, CollisionTwoSpheres) {
    ip_config_t cfg = intuitive_physics_default_config();
    cfg.gravity_magnitude = 0.0f;  /* no gravity -- test collision only */
    cfg.gravity_direction = (wm_parietal_vec3_t){0, 0, 0};
    intuitive_physics_engine_t* engine = intuitive_physics_create(&cfg);
    ASSERT_NE(engine, nullptr);

    /* Two spheres approaching each other head-on */
    ip_object_t a = make_sphere(-2.0f, 0.0f, 0.0f, 0.5f, 1.0f);
    a.velocity = (wm_parietal_velocity_t){5.0f, 0.0f, 0.0f};
    ip_object_t b = make_sphere(2.0f, 0.0f, 0.0f, 0.5f, 1.0f);
    b.velocity = (wm_parietal_velocity_t){-5.0f, 0.0f, 0.0f};

    uint32_t id_a = intuitive_physics_add_object(engine, &a);
    uint32_t id_b = intuitive_physics_add_object(engine, &b);
    ASSERT_NE(id_a, UINT32_MAX);
    ASSERT_NE(id_b, UINT32_MAX);

    /* Will they collide? */
    float col_time = 0.0f;
    bool will_collide = intuitive_physics_will_collide(engine, id_a, id_b, 1.0f, &col_time);
    EXPECT_TRUE(will_collide);
    /* Spheres 4m apart, closing at 10m/s, radii sum=1.0, so ~0.3s */
    EXPECT_NEAR(col_time, 0.3f, 0.15f);

    /* Step past collision */
    for (int i = 0; i < 100; i++) {
        intuitive_physics_step(engine, 0.01f);
    }

    /* After elastic collision of equal masses: velocities should swap */
    ip_object_t* pa = intuitive_physics_get_object(engine, id_a);
    ip_object_t* pb = intuitive_physics_get_object(engine, id_b);
    ASSERT_NE(pa, nullptr);
    ASSERT_NE(pb, nullptr);
    /* A should be moving left, B should be moving right (or stopped/swapped) */
    EXPECT_LE(pa->velocity.vx, 1.0f);  /* was +5, now should be <= 0 */

    intuitive_physics_destroy(engine);
}

TEST_F(IntuitivePhysicsTest, SceneStability) {
    intuitive_physics_add_ground(engine);

    /* Stack of resting objects */
    ip_object_t box1 = make_sphere(0.0f, 0.5f, 0.0f, 0.5f, 2.0f);
    ip_object_t box2 = make_sphere(0.0f, 1.5f, 0.0f, 0.5f, 1.0f);
    intuitive_physics_add_object(engine, &box1);
    intuitive_physics_add_object(engine, &box2);

    /* Let the stack settle */
    for (int i = 0; i < 200; i++) {
        intuitive_physics_step(engine, 0.01f);
    }

    bool stable = intuitive_physics_is_stable(engine);
    EXPECT_TRUE(stable);
}

TEST_F(IntuitivePhysicsTest, AddRemoveObject) {
    ip_object_t ball = make_sphere(0.0f, 5.0f, 0.0f, 0.5f, 1.0f);
    uint32_t id = intuitive_physics_add_object(engine, &ball);
    ASSERT_NE(id, UINT32_MAX);
    EXPECT_EQ(engine->scene.num_objects, 1);

    intuitive_physics_remove_object(engine, id);
    ip_object_t* removed = intuitive_physics_get_object(engine, id);
    /* After removal the object should be inactive or NULL */
    EXPECT_TRUE(removed == NULL || !removed->active);
}

TEST_F(IntuitivePhysicsTest, StepReturnsZero) {
    int rc = intuitive_physics_step(engine, 0.01f);
    EXPECT_EQ(rc, 0);
}

TEST_F(IntuitivePhysicsTest, StatsAccumulate) {
    intuitive_physics_add_ground(engine);
    ip_object_t ball = make_sphere(0.0f, 3.0f, 0.0f, 0.3f, 1.0f);
    intuitive_physics_add_object(engine, &ball);

    for (int i = 0; i < 50; i++) {
        intuitive_physics_step(engine, 0.01f);
    }

    ip_stats_t stats = intuitive_physics_get_stats(engine);
    EXPECT_EQ(stats.step_count, 50);
    EXPECT_GE(stats.active_objects, 1);
}
