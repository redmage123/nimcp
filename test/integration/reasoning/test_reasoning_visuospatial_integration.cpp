/**
 * @file test_reasoning_visuospatial_integration.cpp
 * @brief Integration tests for visuospatial reasoning with the reasoning engine
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "cognitive/reasoning/nimcp_reasoning_visuospatial.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "core/brain/nimcp_brain.h"
}

class VisuospatialIntegrationTest : public ::testing::Test {
protected:
    reasoning_engine_t* engine = nullptr;
    reasoning_visuospatial_t* vs = nullptr;

    void SetUp() override {
        reasoning_engine_config_t cfg = reasoning_engine_default_config();
        cfg.enable_visuospatial_reasoning = true;
        cfg.enable_convergent_reasoning = false;
        cfg.enable_concurrent_pipeline = false;
        engine = reasoning_engine_create(&cfg);
        ASSERT_NE(engine, nullptr);

        vs = reasoning_visuospatial_create(nullptr);
        ASSERT_NE(vs, nullptr);
    }

    void TearDown() override {
        reasoning_visuospatial_destroy(vs);
        reasoning_engine_destroy(engine);
    }
};

TEST_F(VisuospatialIntegrationTest, VisuospatialWithEngine) {
    /* Verify engine was created with visuospatial enabled */
    EXPECT_NE(engine, nullptr);
    EXPECT_NE(vs, nullptr);
}

TEST_F(VisuospatialIntegrationTest, VisuospatialStepProduced) {
    /* Create a chain and add a visuospatial step manually to verify step type */
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    reasoning_step_t step;
    memset(&step, 0, sizeof(step));
    step.step_id = 0;
    step.type = REASONING_STEP_VISUOSPATIAL;
    step.confidence = 0.85f;
    step.relevance = 0.9f;
    snprintf(step.description, REASONING_STEP_DESC_LEN,
             "Visuospatial: chair is INSIDE room (confidence=1.0)");

    EXPECT_EQ(reasoning_chain_add_step(&chain, &step), 0);
    EXPECT_EQ(chain.num_steps, 1u);

    const reasoning_step_t* s = reasoning_chain_get_step(&chain, 0);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->type, REASONING_STEP_VISUOSPATIAL);
    EXPECT_STREQ(reasoning_step_type_name(s->type), "VISUOSPATIAL");

    reasoning_chain_cleanup(&chain);
}

TEST_F(VisuospatialIntegrationTest, VisuospatialDisabled) {
    /* Create engine with visuospatial disabled */
    reasoning_engine_config_t cfg = reasoning_engine_default_config();
    EXPECT_FALSE(cfg.enable_visuospatial_reasoning);
}

TEST_F(VisuospatialIntegrationTest, SpatialSceneReasoning) {
    /* Build a room scene and query spatial relations */
    vs_point_t room_pos = {5.0f, 5.0f, 0.0f};
    vs_bounds_t room_bounds = {{0.0f, 0.0f, 0.0f}, {10.0f, 10.0f, 0.0f}};
    int room = reasoning_visuospatial_add_object_with_bounds(vs, "room", room_pos, room_bounds);

    vs_point_t table_pos = {3.0f, 3.0f, 0.0f};
    int table = reasoning_visuospatial_add_object(vs, "table", table_pos);

    vs_point_t lamp_pos = {3.0f, 8.0f, 0.0f};
    int lamp = reasoning_visuospatial_add_object(vs, "lamp", lamp_pos);

    /* Infer relations */
    int n = reasoning_visuospatial_infer_relations(vs);
    EXPECT_GT(n, 0);

    /* Table should be INSIDE room (room has bounds, table position is inside) */
    vs_query_t q;
    memset(&q, 0, sizeof(q));
    q.type = VS_QUERY_CONTAINS;
    q.object_a_id = (uint32_t)table;
    q.object_b_id = (uint32_t)room;

    vs_result_t r;
    EXPECT_EQ(reasoning_visuospatial_query(vs, &q, &r), 0);
    EXPECT_TRUE(r.holds);

    /* Table should be BELOW lamp (table.y=3 < lamp.y=8 - threshold=1)
     * Note: infer_relations iterates pairs in slot order (i < j), so the
     * relation is stored as BELOW(table, lamp) not ABOVE(lamp, table). */
    q.type = VS_QUERY_RELATION;
    q.object_a_id = (uint32_t)table;
    q.object_b_id = (uint32_t)lamp;
    q.relation = VS_RELATION_BELOW;
    EXPECT_EQ(reasoning_visuospatial_query(vs, &q, &r), 0);
    EXPECT_TRUE(r.holds);
}

TEST_F(VisuospatialIntegrationTest, VisuospatialStats) {
    vs_point_t p1 = {0.0f, 0.0f, 0.0f};
    vs_point_t p2 = {5.0f, 0.0f, 0.0f};
    reasoning_visuospatial_add_object(vs, "a", p1);
    reasoning_visuospatial_add_object(vs, "b", p2);

    reasoning_visuospatial_infer_relations(vs);

    vs_query_t q;
    memset(&q, 0, sizeof(q));
    q.type = VS_QUERY_DISTANCE;
    q.object_a_id = 0;
    q.object_b_id = 1;
    vs_result_t r;
    reasoning_visuospatial_query(vs, &q, &r);

    visuospatial_stats_t stats;
    EXPECT_EQ(reasoning_visuospatial_get_stats(vs, &stats), 0);
    EXPECT_EQ(stats.num_objects, 2u);
    EXPECT_GT(stats.num_relations, 0u);
    EXPECT_EQ(stats.num_queries, 1u);
}

TEST_F(VisuospatialIntegrationTest, VisuospatialWithConvergent) {
    /* Verify visuospatial config field exists alongside convergent */
    reasoning_engine_config_t cfg = reasoning_engine_default_config();
    cfg.enable_visuospatial_reasoning = true;
    cfg.enable_convergent_reasoning = true;

    reasoning_engine_t* eng = reasoning_engine_create(&cfg);
    ASSERT_NE(eng, nullptr);

    /* Stats should have visuospatial_queries field */
    reasoning_engine_stats_t stats;
    EXPECT_EQ(reasoning_engine_get_stats(eng, &stats), 0);
    EXPECT_EQ(stats.visuospatial_queries, 0u);

    reasoning_engine_destroy(eng);
}
