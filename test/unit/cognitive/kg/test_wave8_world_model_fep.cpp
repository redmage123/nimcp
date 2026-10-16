/**
 * @file test_wave8_world_model_fep.cpp
 * @brief Unit test for KG-integration Wave W8 (world-model / imagination /
 *        FEP / predictive / salience family).
 *
 * W8 wires the 10 modules that collectively form the "simulated world" the
 * brain reasons over. This test exercises the shared write/query helpers in
 * `cognitive/world_model/nimcp_world_model_kg_events.h`:
 *
 *   1. omni_wm         (cog_omni_wm_world_model)
 *   2. intuitive_phys  (cog_physics_intuitive)
 *   3. scene_graph     (cog_physics_scene_graph)
 *   4. entity_tracker  (cog_physics_entity_tracker)
 *   5. world_sim       (cog_physics_world_simulator)
 *   6. free_energy     (cog_fep_free_energy)
 *   7. predictive      (cog_predictive)
 *   8. salience        (cog_salience)
 *   9. imag_workspace  (cog_imagination_workspace)
 *  10. jepa_bridges    (cog_jepa_bridges)
 *
 * Strategy: spin up a minimal brain (internal_kg always-on), call
 * `world_model_kg_init_roots()`, then exercise each emit + at least one
 * read-path helper. Read-path queries must not crash even when the KG has
 * never been populated, and must return the expected counts after emit.
 */

#include <gtest/gtest.h>
#include <cstring>

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"
#include "cognitive/world_model/nimcp_world_model_kg_events.h"

//-----------------------------------------------------------------------------
// Fixture
//-----------------------------------------------------------------------------

class Wave8WorldModelFepTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        brain = brain_create_minimal("wave8_kg_test",
                                     BRAIN_SIZE_MICRO,
                                     BRAIN_TASK_CLASSIFICATION,
                                     4, 2);
        ASSERT_NE(brain, nullptr) << "brain_create_minimal returned NULL";
        ASSERT_TRUE(brain->internal_kg_enabled)
            << "internal_kg_enabled must be true post-creation";
        ASSERT_NE(brain->internal_kg, nullptr)
            << "brain->internal_kg must be allocated";
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    void expect_node(const char* name) {
        brain_kg_node_id_t id =
            brain_kg_find_node(brain->internal_kg, name);
        EXPECT_NE(id, BRAIN_KG_INVALID_NODE)
            << "expected KG node '" << name << "' to be present";
    }

    bool any_node_with_prefix(const char* prefix) {
        const size_t plen = strlen(prefix);
        for (uint32_t t = 0; t < BRAIN_KG_NODE_TYPE_COUNT; ++t) {
            brain_kg_node_list_t* list =
                brain_kg_get_nodes_by_type(brain->internal_kg,
                                           (brain_kg_node_type_t)t);
            if (!list) continue;
            bool found = false;
            for (uint32_t i = 0; i < list->count && !found; ++i) {
                const brain_kg_node_t* n = list->nodes[i];
                if (n && strncmp(n->name, prefix, plen) == 0) {
                    found = true;
                }
            }
            brain_kg_node_list_destroy(list);
            if (found) return true;
        }
        return false;
    }
};

//-----------------------------------------------------------------------------
// Structural init: 10 root nodes
//-----------------------------------------------------------------------------

TEST_F(Wave8WorldModelFepTest, StructuralRootsRegistered) {
    /* Idempotent — brain_init may have run it already. */
    EXPECT_EQ(world_model_kg_init_roots(brain), 0);

    expect_node("cog_omni_wm_world_model");
    expect_node("cog_physics_intuitive");
    expect_node("cog_physics_scene_graph");
    expect_node("cog_physics_entity_tracker");
    expect_node("cog_physics_world_simulator");
    expect_node("cog_fep_free_energy");
    expect_node("cog_predictive");
    expect_node("cog_salience");
    expect_node("cog_imagination_workspace");
    expect_node("cog_jepa_bridges");

    /* Idempotent re-init must not fail. */
    EXPECT_EQ(world_model_kg_init_roots(brain), 0);
    expect_node("cog_omni_wm_world_model");
}

TEST_F(Wave8WorldModelFepTest, RegisteredBrainRoundTrip) {
    EXPECT_EQ(world_model_kg_init_roots(brain), 0);
    EXPECT_EQ(world_model_kg_events_get_registered_brain(), brain);

    world_model_kg_events_set_registered_brain(nullptr);
    EXPECT_EQ(world_model_kg_events_get_registered_brain(), nullptr);
    world_model_kg_events_set_registered_brain(brain);
    EXPECT_EQ(world_model_kg_events_get_registered_brain(), brain);
}

//-----------------------------------------------------------------------------
// Write-path: one emit per module
//-----------------------------------------------------------------------------

TEST_F(Wave8WorldModelFepTest, OmniWmEmit) {
    ASSERT_EQ(world_model_kg_init_roots(brain), 0);
    world_model_kg_emit_wm_step(brain, /*step_id*/11u, /*reward*/0.5f,
                                /*surprise*/0.7f);
    EXPECT_TRUE(any_node_with_prefix("cog_omni_wm_world_model_event_step_11_"));
}

TEST_F(Wave8WorldModelFepTest, PhysicsStepEmit) {
    ASSERT_EQ(world_model_kg_init_roots(brain), 0);
    world_model_kg_emit_physics_step(brain, /*step_count*/3u,
                                      /*active*/4, /*contacts*/2,
                                      /*drift*/0.01f);
    EXPECT_TRUE(any_node_with_prefix("cog_physics_intuitive_event_step_3_"));
}

TEST_F(Wave8WorldModelFepTest, SceneGraphEmit) {
    ASSERT_EQ(world_model_kg_init_roots(brain), 0);
    world_model_kg_emit_scene_rebuild(brain, /*num_relations*/12u,
                                      /*rebuild_count*/5u);
    EXPECT_TRUE(any_node_with_prefix("cog_physics_scene_graph_event_rebuild_5_"));
}

TEST_F(Wave8WorldModelFepTest, EntityTrackerEmit) {
    ASSERT_EQ(world_model_kg_init_roots(brain), 0);
    world_model_kg_emit_entity_event(brain, /*entity_id*/17u,
                                     /*spawn*/0, 0.9f);
    EXPECT_TRUE(any_node_with_prefix("cog_physics_entity_tracker_event_spawn_17_"));
    world_model_kg_emit_entity_event(brain, 17u, /*occlude*/1, 0.4f);
    EXPECT_TRUE(any_node_with_prefix("cog_physics_entity_tracker_event_occlude_17_"));
}

TEST_F(Wave8WorldModelFepTest, WorldSimEmit) {
    ASSERT_EQ(world_model_kg_init_roots(brain), 0);
    world_model_kg_emit_sim_step(brain, /*step_count*/9u,
                                 /*dt*/0.01f, /*num_engines*/36u);
    EXPECT_TRUE(any_node_with_prefix("cog_physics_world_simulator_event_step_9_"));
}

TEST_F(Wave8WorldModelFepTest, SurpriseEmit) {
    ASSERT_EQ(world_model_kg_init_roots(brain), 0);
    /* Low surprise goes to *_event_step_* */
    world_model_kg_emit_surprise(brain, /*F*/1.2f, /*surprise*/0.5f,
                                 /*precision*/1.0f);
    EXPECT_TRUE(any_node_with_prefix("cog_fep_free_energy_event_step_"));
    /* High surprise goes to *_event_high_surprise_* */
    world_model_kg_emit_surprise(brain, /*F*/5.0f, /*surprise*/3.0f,
                                 /*precision*/1.0f);
    EXPECT_TRUE(any_node_with_prefix("cog_fep_free_energy_event_high_surprise_"));
}

TEST_F(Wave8WorldModelFepTest, PredictionErrorEmit) {
    ASSERT_EQ(world_model_kg_init_roots(brain), 0);
    world_model_kg_emit_prediction_error(brain, /*layer*/2u,
                                         /*err*/0.8f, /*F*/1.1f);
    EXPECT_TRUE(any_node_with_prefix("cog_predictive_event_step_L2_"));
    world_model_kg_emit_prediction_error(brain, /*layer*/3u,
                                         /*err*/20.0f, /*F*/50.0f);
    EXPECT_TRUE(any_node_with_prefix("cog_predictive_event_high_error_L3_"));
}

TEST_F(Wave8WorldModelFepTest, SalienceTransitionEmit) {
    ASSERT_EQ(world_model_kg_init_roots(brain), 0);
    world_model_kg_emit_salience_transition(brain, /*old*/0, /*new*/1, 0.85f);
    EXPECT_TRUE(any_node_with_prefix("cog_salience_event_transition_0_to_1_"));
}

TEST_F(Wave8WorldModelFepTest, ImaginationWorkspaceEmit) {
    ASSERT_EQ(world_model_kg_init_roots(brain), 0);
    world_model_kg_emit_workspace_scenario(brain, /*id*/123u,
                                           /*steps*/4u, /*vivid*/0.9f);
    EXPECT_TRUE(any_node_with_prefix(
        "cog_imagination_workspace_event_scenario_123_"));
}

TEST_F(Wave8WorldModelFepTest, JepaBridgesTickEmit) {
    ASSERT_EQ(world_model_kg_init_roots(brain), 0);
    world_model_kg_emit_jepa_tick(brain, /*bridges_active*/4u,
                                  /*avg_loss*/0.05f);
    EXPECT_TRUE(any_node_with_prefix("cog_jepa_bridges_event_tick_"));
}

//-----------------------------------------------------------------------------
// Read-path queries
//-----------------------------------------------------------------------------

TEST_F(Wave8WorldModelFepTest, RootEdgeCountBeforeAndAfterEmit) {
    ASSERT_EQ(world_model_kg_init_roots(brain), 0);

    uint32_t before =
        world_model_kg_root_edge_count(brain, "cog_fep_free_energy");
    world_model_kg_emit_surprise(brain, 2.0f, 1.5f, 1.0f);
    uint32_t after =
        world_model_kg_root_edge_count(brain, "cog_fep_free_energy");
    EXPECT_GE(after, before + 1u)
        << "emit should have added an event-edge from the FEP root";
}

TEST_F(Wave8WorldModelFepTest, HasPartnerQueries) {
    ASSERT_EQ(world_model_kg_init_roots(brain), 0);
    /* Our own roots must be visible via partner query */
    EXPECT_TRUE(world_model_kg_has_partner(brain, "cog_omni_wm_world_model"));
    EXPECT_TRUE(world_model_kg_has_partner(brain, "cog_physics_intuitive"));
    EXPECT_TRUE(world_model_kg_has_partner(brain, "cog_predictive"));
    /* Made-up name should return false */
    EXPECT_FALSE(world_model_kg_has_partner(brain,
                                            "definitely_not_a_real_node"));
}

TEST_F(Wave8WorldModelFepTest, RecentSurpriseDetection) {
    ASSERT_EQ(world_model_kg_init_roots(brain), 0);
    /* No surprise yet */
    EXPECT_FALSE(world_model_kg_has_recent_surprise(brain));
    /* After high-surprise emit, should be detected */
    world_model_kg_emit_surprise(brain, 5.0f, 3.0f, 1.0f);
    EXPECT_TRUE(world_model_kg_has_recent_surprise(brain));
}

//-----------------------------------------------------------------------------
// NULL safety
//-----------------------------------------------------------------------------

TEST_F(Wave8WorldModelFepTest, NullBrainIsSafe) {
    /* Every emit must be NULL-safe: no crash, no throw. */
    world_model_kg_emit_wm_step(nullptr, 1, 0.0f, 0.0f);
    world_model_kg_emit_physics_step(nullptr, 1, 0, 0, 0.0f);
    world_model_kg_emit_scene_rebuild(nullptr, 0, 0);
    world_model_kg_emit_entity_event(nullptr, 0, 0, 0.0f);
    world_model_kg_emit_sim_step(nullptr, 1, 0.01f, 0);
    world_model_kg_emit_surprise(nullptr, 0.0f, 0.0f, 1.0f);
    world_model_kg_emit_prediction_error(nullptr, 0, 0.0f, 0.0f);
    world_model_kg_emit_salience_transition(nullptr, 0, 0, 0.0f);
    world_model_kg_emit_workspace_scenario(nullptr, 0, 0, 0.0f);
    world_model_kg_emit_jepa_tick(nullptr, 0, 0.0f);
    EXPECT_EQ(world_model_kg_root_edge_count(nullptr, "x"), 0u);
    EXPECT_FALSE(world_model_kg_has_recent_surprise(nullptr));
    EXPECT_FALSE(world_model_kg_has_partner(nullptr, "x"));
    EXPECT_EQ(world_model_kg_init_roots(nullptr), -1);
}
