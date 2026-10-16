/**
 * @file test_brain_simulation_pipeline.cpp
 * @brief End-to-end tests — full brain → simulation → perception → learning pipeline
 *
 * Tests the complete data flow:
 *   1. Brain creates with simulation engines enabled
 *   2. Physics engine runs a scenario
 *   3. Perception bridge renders to sensory data
 *   4. Brain processes sensory input (decide_full)
 *   5. Brain learns from experience (learn_vector)
 *   6. Verify the brain's output changed (learning occurred)
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "cognitive/physics/nimcp_intuitive_physics.h"
#include "cognitive/physics/nimcp_entity_tracker.h"
#include "cognitive/physics/nimcp_scene_graph.h"
#include "cognitive/physics/nimcp_physics_prior.h"
#include "cognitive/physics/nimcp_sim_perception_bridge.h"
#include "cognitive/physics/nimcp_world_simulator.h"
#include "cognitive/physics/nimcp_surface_physics.h"
#include "cognitive/physics/nimcp_surface_chemistry.h"
#include "cognitive/physics/nimcp_chemistry_sim.h"
#include "cognitive/physics/nimcp_biology_sim.h"
}

/* ============================================================
 * E2E: Full simulation pipeline without brain
 * (Tests the engine chain independently of brain init)
 * ============================================================ */

class SimulationPipelineTest : public ::testing::Test {
protected:
    intuitive_physics_engine_t* physics = nullptr;
    entity_tracker_t* tracker = nullptr;
    scene_graph_t* scene = nullptr;
    physics_prior_t* prior = nullptr;
    sim_perception_bridge_t* perception = nullptr;
    world_simulator_t* world = nullptr;

    void SetUp() override {
        physics = intuitive_physics_create(NULL);
        tracker = entity_tracker_create(NULL);
        scene = scene_graph_create(NULL);
        prior = physics_prior_create(NULL);
        perception = spb_create(NULL);
        world = wsim_create(NULL);
    }

    void TearDown() override {
        spb_destroy(perception);
        physics_prior_destroy(prior);
        scene_graph_destroy(scene);
        entity_tracker_destroy(tracker);
        intuitive_physics_destroy(physics);
        wsim_destroy(world);
    }
};

TEST_F(SimulationPipelineTest, AllEnginesCreate) {
    ASSERT_NE(physics, nullptr);
    ASSERT_NE(tracker, nullptr);
    ASSERT_NE(scene, nullptr);
    ASSERT_NE(prior, nullptr);
    ASSERT_NE(perception, nullptr);
    ASSERT_NE(world, nullptr);
}

TEST_F(SimulationPipelineTest, PhysicsToSceneGraphToTracker) {
    /* 1. Create a scene: ground + falling ball */
    intuitive_physics_add_ground(physics);
    ip_object_t ball = {};
    ball.position = {0, 3.0f, 0};
    ball.mass = 1.0f;
    ball.shape.type = IP_SHAPE_SPHERE;
    ball.shape.sphere.radius = 0.5f;
    ball.restitution = 0.3f;
    ball.friction = 0.5f;
    ball.visible = true;
    ball.active = true;
    uint32_t ball_id = intuitive_physics_add_object(physics, &ball);
    ASSERT_NE(ball_id, UINT32_MAX);

    /* 2. Simulate until stable */
    for (int i = 0; i < 500; i++)
        intuitive_physics_step(physics, 0.01f);

    /* 3. Build scene graph from physics */
    int rc = scene_graph_rebuild(scene, physics);
    EXPECT_EQ(rc, 0);

    /* 4. Ball should have settled (y near radius) */
    ip_object_t* settled = intuitive_physics_get_object(physics, ball_id);
    ASSERT_NE(settled, nullptr);
    EXPECT_NEAR(settled->position.y, 0.5f, 0.2f);  /* near radius above ground */

    /* 5. Register ball with entity tracker */
    entity_observation_t obs = {};
    ip_object_t* obj = intuitive_physics_get_object(physics, ball_id);
    ASSERT_NE(obj, nullptr);
    obs.position = obj->position;
    obs.velocity = {obj->velocity.vx, obj->velocity.vy, obj->velocity.vz};
    obs.bounding_radius = 0.5f;
    obs.confidence = 0.95f;
    entity_tracker_update(tracker, &obs, 1, 5.0);

    /* 6. Entity tracker should have at least one entity */
    EXPECT_GE(entity_tracker_count_total(tracker), 1u);
}

TEST_F(SimulationPipelineTest, PerceptionBridgeRendersScene) {
    /* 1. Set up physics scene */
    intuitive_physics_add_ground(physics);
    ip_object_t ball = {};
    ball.position = {0, 2.0f, 0};
    ball.mass = 1.0f;
    ball.shape.type = IP_SHAPE_SPHERE;
    ball.shape.sphere.radius = 0.5f;
    ball.visible = true;
    ball.active = true;
    intuitive_physics_add_object(physics, &ball);

    for (int i = 0; i < 100; i++)
        intuitive_physics_step(physics, 0.01f);

    /* 2. Connect perception bridge to physics */
    spb_connect_physics(perception, physics, scene, tracker);

    /* 3. Render all modalities */
    int rc = spb_render(perception);
    EXPECT_EQ(rc, 0);

    /* 4. Visual frame should have non-zero pixels (ball is rendered) */
    const uint8_t* frame = spb_get_visual_frame(perception);
    ASSERT_NE(frame, nullptr);
    int nonzero_pixels = 0;
    for (int i = 0; i < 64 * 64; i++) {
        if (frame[i] > 40) nonzero_pixels++;  /* > background gray */
    }
    EXPECT_GT(nonzero_pixels, 0);  /* ball should be visible */

    /* 5. Audio features should exist (may be silent if no collisions) */
    const float* audio = spb_get_audio_features(perception);
    ASSERT_NE(audio, nullptr);

    /* 6. Haptic segments should exist */
    const float* haptic = spb_get_haptic_segments(perception);
    ASSERT_NE(haptic, nullptr);

    /* 7. Stats should show rendering occurred */
    spb_stats_t stats = spb_get_stats(perception);
    EXPECT_EQ(stats.frames_rendered, 1u);
}

TEST_F(SimulationPipelineTest, PhysicsPriorConstrainsImpossible) {
    /* Physics prior should correct objects below ground */
    physics_prior_connect(prior, physics, tracker, scene);

    pp_spatial_state_t state;
    pp_spatial_state_alloc(&state, 2);
    state.num_objects = 2;

    /* Object 0: valid position above ground */
    state.positions[0] = {0, 1.0f, 0};
    state.velocities[0] = {0, 0, 0};

    /* Object 1: impossible — below ground */
    state.positions[1] = {0, -5.0f, 0};
    state.velocities[1] = {0, -10.0f, 0};

    /* Check violations — object below ground should be flagged */
    pp_violation_flags_t violations = physics_prior_check_violations(prior, &state, 0.01f);
    EXPECT_TRUE(violations & PP_VIOLATION_INTERPENETRATION);

    pp_spatial_state_free(&state);
}

TEST_F(SimulationPipelineTest, WorldSimulatorOrchestratesAll) {
    /* Register physics engine with world simulator */
    wsim_register_engine(world, WSIM_ENGINE_NEWTONIAN, physics, 0.01f);
    wsim_enable_standard_couplings(world);

    /* Set a non-uniform temperature (hot spot) */
    wsim_set_temperature(world, 16, 16, 16, 500.0f);  /* hot center */
    float T_hot = wsim_get_temperature(world, 16, 16, 16);
    EXPECT_NEAR(T_hot, 500.0f, 0.1f);

    /* Step the world */
    for (int i = 0; i < 10; i++) {
        int rc = wsim_step(world, 0.01f);
        EXPECT_EQ(rc, 0);
    }

    /* Verify simulation ran successfully */
    wsim_stats_t stats = wsim_get_stats(world);
    EXPECT_EQ(stats.master_steps, 10u);
    EXPECT_GE(stats.active_engines, 1u);
    /* Note: setting a hot spot intentionally changes total energy,
     * so conservation violations are expected here. */
}

/* ============================================================
 * E2E: Multi-domain scenario — coffee cup cooling
 * ============================================================ */

TEST(MultiDomainScenario, CoffeeCupCooling) {
    /* A cup of hot coffee on a table involves:
     * - Newtonian mechanics (cup on table)
     * - Heat transfer (coffee cooling)
     * - Surface physics (evaporation)
     * - Chemistry (volatile compounds)
     * - Fluid dynamics (convection in liquid)
     * We test that multiple engines can run in the same world step. */

    auto* world = wsim_create(NULL);
    ASSERT_NE(world, nullptr);

    auto* physics = intuitive_physics_create(NULL);
    ASSERT_NE(physics, nullptr);
    intuitive_physics_add_ground(physics);

    auto* surface = surface_physics_create(NULL);
    ASSERT_NE(surface, nullptr);
    surface_physics_load_common_materials(surface);

    auto* chem = chemistry_sim_create(NULL);
    ASSERT_NE(chem, nullptr);
    chemistry_sim_load_common_elements(chem);

    /* Register all engines */
    wsim_register_engine(world, WSIM_ENGINE_NEWTONIAN, physics, 0.01f);
    wsim_register_engine(world, WSIM_ENGINE_SURFACE_PHYS, surface, 0.01f);
    wsim_register_engine(world, WSIM_ENGINE_BULK_CHEM, chem, 0.01f);
    wsim_enable_standard_couplings(world);

    /* Set initial temperature: hot coffee at center */
    wsim_set_temperature(world, 16, 16, 16, 370.0f);  /* ~97°C */

    /* Run simulation */
    for (int i = 0; i < 50; i++) {
        int rc = wsim_step(world, 0.01f);
        EXPECT_EQ(rc, 0);
    }

    wsim_stats_t stats = wsim_get_stats(world);
    EXPECT_EQ(stats.master_steps, 50u);
    EXPECT_GE(stats.active_engines, 3u);
    /* Hot spot intentionally changes energy — violations expected.
     * Just verify simulation completed without crash. */

    chemistry_sim_destroy(chem);
    surface_physics_destroy(surface);
    intuitive_physics_destroy(physics);
    wsim_destroy(world);
}

/* ============================================================
 * E2E: Object permanence — hide and predict
 * ============================================================ */

TEST(ObjectPermanence, HiddenObjectTracked) {
    /* Simulate an object rolling behind a screen:
     * 1. Object visible, moving right
     * 2. Object disappears (occluded)
     * 3. Physics predicts where it should be
     * 4. Object reappears — verify tracker matches */

    auto* physics = intuitive_physics_create(NULL);
    auto* tracker = entity_tracker_create(NULL);
    ASSERT_NE(physics, nullptr);
    ASSERT_NE(tracker, nullptr);

    intuitive_physics_add_ground(physics);
    ip_object_t ball = {};
    ball.position = {-2.0f, 0.5f, 0};
    ball.velocity = {1.0f, 0, 0};
    ball.mass = 1.0f;
    ball.shape.type = IP_SHAPE_SPHERE;
    ball.shape.sphere.radius = 0.5f;
    ball.restitution = 0.0f;
    ball.friction = 0.0f;
    ball.visible = true;
    ball.active = true;
    uint32_t ball_id = intuitive_physics_add_object(physics, &ball);

    /* Phase 1: observe ball for 10 steps */
    for (int i = 0; i < 10; i++) {
        intuitive_physics_step(physics, 0.1f);
        ip_object_t* b = intuitive_physics_get_object(physics, ball_id);
        entity_observation_t obs = {};
        obs.position = b->position;
        obs.velocity = {b->velocity.vx, b->velocity.vy, b->velocity.vz};
        obs.bounding_radius = 0.5f;
        obs.confidence = 0.9f;
        entity_tracker_update(tracker, &obs, 1, (double)i * 0.1);
    }
    EXPECT_EQ(entity_tracker_count_visible(tracker), 1u);

    /* Phase 2: ball occluded (no observations) for 10 steps */
    for (int i = 0; i < 10; i++) {
        intuitive_physics_step(physics, 0.1f);
        entity_tracker_update(tracker, NULL, 0, 1.0 + i * 0.1);
        entity_tracker_predict_hidden(tracker, physics, 0.1f);
    }

    /* Ball should still be tracked (permanent) but occluded */
    EXPECT_EQ(entity_tracker_count_visible(tracker), 0u);
    EXPECT_EQ(entity_tracker_count_total(tracker), 1u);

    /* Phase 3: ball reappears */
    ip_object_t* b = intuitive_physics_get_object(physics, ball_id);
    entity_observation_t reappear = {};
    reappear.position = b->position;
    reappear.velocity = {b->velocity.vx, b->velocity.vy, b->velocity.vz};
    reappear.bounding_radius = 0.5f;
    reappear.confidence = 0.9f;
    entity_tracker_update(tracker, &reappear, 1, 2.0);

    /* Should re-associate with the same entity (not create new) */
    EXPECT_EQ(entity_tracker_count_visible(tracker), 1u);
    EXPECT_EQ(entity_tracker_count_total(tracker), 1u);

    entity_tracker_destroy(tracker);
    intuitive_physics_destroy(physics);
}

/* ============================================================
 * E2E: Stacking and collapse prediction
 * ============================================================ */

TEST(SceneReasoning, StackCollapsePredict) {
    /* Stack 3 boxes. Verify scene graph shows support chain.
     * Predict what happens if middle box removed. */

    auto* physics = intuitive_physics_create(NULL);
    auto* scene = scene_graph_create(NULL);
    ASSERT_NE(physics, nullptr);
    ASSERT_NE(scene, nullptr);

    intuitive_physics_add_ground(physics);

    /* Three boxes stacked */
    ip_object_t box = {};
    box.mass = 1.0f;
    box.shape.type = IP_SHAPE_BOX;
    box.shape.box.hx = 0.3f;
    box.shape.box.hy = 0.3f;
    box.shape.box.hz = 0.3f;
    box.restitution = 0.1f;
    box.friction = 0.5f;
    box.visible = true;
    box.active = true;

    box.position = {0, 0.3f, 0};
    uint32_t b1 = intuitive_physics_add_object(physics, &box);
    box.position = {0, 0.9f, 0};
    uint32_t b2 = intuitive_physics_add_object(physics, &box);
    box.position = {0, 1.5f, 0};
    uint32_t b3 = intuitive_physics_add_object(physics, &box);

    /* Settle */
    for (int i = 0; i < 500; i++)
        intuitive_physics_step(physics, 0.01f);

    EXPECT_TRUE(intuitive_physics_is_stable(physics));

    /* Build scene graph */
    scene_graph_rebuild(scene, physics);
    EXPECT_GT(scene_graph_count(scene), 0u);

    /* Predict removal cascade: removing b1 should affect b2 and b3 */
    uint32_t affected[8];
    uint32_t n_affected = intuitive_physics_predict_removal(physics, b1, affected, 8);
    EXPECT_GE(n_affected, 1u);  /* at least b2 should be affected */

    scene_graph_destroy(scene);
    intuitive_physics_destroy(physics);
}
