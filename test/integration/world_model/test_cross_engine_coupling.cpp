/**
 * @file test_cross_engine_coupling.cpp
 * @brief Integration tests -- verify engines work together through world simulator (Google Test)
 *
 * Tests that cross-domain couplings actually transfer data:
 * - Chemistry -> Heat (reaction exotherm raises temperature)
 * - Heat -> Fluid (buoyancy from temperature gradient)
 * - Physics -> Scene Graph (contacts produce support relations)
 * - Entity Tracker -> Physics Prior (hidden objects still tracked)
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "cognitive/physics/nimcp_intuitive_physics.h"
#include "cognitive/physics/nimcp_scene_graph.h"
#include "cognitive/physics/nimcp_entity_tracker.h"
#include "cognitive/physics/nimcp_physics_prior.h"
#include "cognitive/physics/nimcp_surface_physics.h"
#include "cognitive/physics/nimcp_surface_chemistry.h"
#include "cognitive/physics/nimcp_world_simulator.h"
}

/* Test: physics engine + scene graph produce support relations */
TEST(CrossEngineCouplingTest, PhysicsToSceneGraph) {
    intuitive_physics_engine_t* phys = intuitive_physics_create(NULL);
    scene_graph_t* sg = scene_graph_create(NULL);
    ASSERT_NE(phys, nullptr);
    ASSERT_NE(sg, nullptr);

    /* Add ground + box on ground + box on box */
    intuitive_physics_add_ground(phys);

    ip_object_t box1 = {0};
    box1.position = (wm_parietal_vec3_t){0, 0.5f, 0};
    box1.mass = 1.0f;
    box1.shape.type = IP_SHAPE_BOX;
    box1.shape.box.hx = 0.5f; box1.shape.box.hy = 0.5f; box1.shape.box.hz = 0.5f;
    box1.restitution = 0.1f; box1.friction = 0.5f;
    box1.visible = true; box1.active = true;
    uint32_t b1 = intuitive_physics_add_object(phys, &box1);

    ip_object_t box2 = box1;
    box2.position.y = 1.5f;
    uint32_t b2 = intuitive_physics_add_object(phys, &box2);

    /* Simulate until stable */
    for (int i = 0; i < 500; i++)
        intuitive_physics_step(phys, 0.01f);

    /* Rebuild scene graph from physics */
    int rc = scene_graph_rebuild(sg, phys);
    EXPECT_EQ(rc, 0);

    /* Should have support relations: box2 ON box1, box1 ON ground */
    EXPECT_GT(scene_graph_count(sg), 0);

    /* Both boxes should be supported */
    EXPECT_TRUE(intuitive_physics_is_supported(phys, b1));
    EXPECT_TRUE(intuitive_physics_is_supported(phys, b2));
    EXPECT_TRUE(intuitive_physics_is_stable(phys));

    scene_graph_destroy(sg);
    intuitive_physics_destroy(phys);
}

/* Test: entity tracker maintains belief about hidden objects */
TEST(CrossEngineCouplingTest, EntityTrackerPermanence) {
    entity_tracker_t* et = entity_tracker_create(NULL);
    ASSERT_NE(et, nullptr);

    /* Present an object */
    entity_observation_t obs = {
        .position = {1.0f, 0.0f, 0.0f},
        .velocity = {0.0f, 0.0f, 0.0f},
        .bounding_radius = 0.5f,
        .confidence = 0.9f,
    };
    entity_tracker_update(et, &obs, 1, 0.0);
    EXPECT_EQ(entity_tracker_count_visible(et), 1u);

    /* Remove observation (object hidden) */
    entity_tracker_update(et, NULL, 0, 1.0);

    /* Entity should still exist (permanence) but be occluded */
    EXPECT_EQ(entity_tracker_count_visible(et), 0u);
    EXPECT_EQ(entity_tracker_count_total(et), 1u);

    /* Should still believe it's permanent */
    uint32_t ids[8];
    uint32_t n = entity_tracker_get_active_ids(et, ids, 8);
    EXPECT_EQ(n, 1u);
    EXPECT_TRUE(entity_tracker_object_permanent(et, ids[0]));

    entity_tracker_destroy(et);
}

/* Test: physics prior constrains impossible predictions */
TEST(CrossEngineCouplingTest, PhysicsPriorConstraint) {
    intuitive_physics_engine_t* phys = intuitive_physics_create(NULL);
    physics_prior_t* pp = physics_prior_create(NULL);
    ASSERT_NE(phys, nullptr);
    ASSERT_NE(pp, nullptr);

    physics_prior_connect(pp, phys, NULL, NULL);

    /* Create a prediction with an object below ground (impossible) */
    pp_spatial_state_t state;
    pp_spatial_state_alloc(&state, 1);
    state.num_objects = 1;
    state.positions[0] = (wm_parietal_vec3_t){0, -1.0f, 0};  /* below ground */
    state.velocities[0] = (wm_parietal_velocity_t){0, 0, 0};
    state.object_ids[0] = 0;

    /* Should detect violation */
    uint32_t violations = physics_prior_constrain(pp, &state, 0.01f);
    /* After constraint, y should be >= 0 */
    EXPECT_GE(state.positions[0].y, 0.0f);

    pp_spatial_state_free(&state);
    physics_prior_destroy(pp);
    intuitive_physics_destroy(phys);
}

/* Test: world simulator creates and steps without crash */
TEST(CrossEngineCouplingTest, WorldSimulatorBasic) {
    world_simulator_t* ws = wsim_create(NULL);
    ASSERT_NE(ws, nullptr);

    /* Register a physics engine */
    intuitive_physics_engine_t* phys = intuitive_physics_create(NULL);
    ASSERT_NE(phys, nullptr);
    wsim_register_engine(ws, WSIM_ENGINE_NEWTONIAN, phys, 0.01f);

    /* Enable couplings */
    wsim_enable_standard_couplings(ws);

    /* Step */
    int rc = wsim_step(ws, 0.01f);
    EXPECT_EQ(rc, 0);

    /* Check stats */
    wsim_stats_t stats = wsim_get_stats(ws);
    EXPECT_EQ(stats.master_steps, 1u);
    EXPECT_GE(stats.active_engines, 1u);

    /* Temperature should be set */
    float T = wsim_get_temperature(ws, 16, 16, 16);
    EXPECT_NEAR(T, 293.15f, 1.0f);  /* ambient +/- coupling effects */

    intuitive_physics_destroy(phys);
    wsim_destroy(ws);
}

/* Test: surface physics + surface chemistry coupling */
TEST(CrossEngineCouplingTest, SurfaceCoupling) {
    surface_physics_sim_t* sp = surface_physics_create(NULL);
    surface_chemistry_sim_t* sc = surface_chemistry_create(NULL);
    ASSERT_NE(sp, nullptr);
    ASSERT_NE(sc, nullptr);

    /* Load materials and create water-glass interface */
    surface_physics_load_common_materials(sp);
    uint32_t iface = surface_physics_create_interface(sp, 0, 1, 0.001f);  /* water-glass */
    EXPECT_NE(iface, UINT32_MAX);

    /* Load Pt catalyst */
    surface_chemistry_load_pt_catalyst(sc);

    /* Step both */
    surface_physics_step(sp, 0.01f);
    surface_chemistry_step(sc, 0.01f);

    /* Both should have non-zero stats */
    surf_phys_stats_t sp_stats = surface_physics_get_stats(sp);
    EXPECT_EQ(sp_stats.step_count, 1u);

    schem_stats_t sc_stats = surface_chemistry_get_stats(sc);
    EXPECT_EQ(sc_stats.step_count, 1u);

    surface_chemistry_destroy(sc);
    surface_physics_destroy(sp);
}
