/**
 * @file test_reasoning_visuospatial.cpp
 * @brief Unit tests for the visuospatial reasoning module
 *
 * WHAT: Tests spatial object management, relation inference, geometric queries
 * WHY:  Verify visuospatial reasoning components work correctly in isolation
 * HOW:  GTest suite testing each component independently
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "cognitive/reasoning/nimcp_reasoning_visuospatial.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
}

class VisuospatialTest : public ::testing::Test {
protected:
    reasoning_visuospatial_t* vs = nullptr;

    void SetUp() override {
        vs = reasoning_visuospatial_create(nullptr);
        ASSERT_NE(vs, nullptr);
    }

    void TearDown() override {
        reasoning_visuospatial_destroy(vs);
        vs = nullptr;
    }
};

/* ===== Lifecycle Tests ===== */

TEST_F(VisuospatialTest, CreateDestroy) {
    /* vs is created in SetUp and destroyed in TearDown */
    EXPECT_NE(vs, nullptr);
}

TEST(VisuospatialLifecycle, CreateNull) {
    /* NULL config should use defaults */
    reasoning_visuospatial_t* v = reasoning_visuospatial_create(nullptr);
    ASSERT_NE(v, nullptr);
    reasoning_visuospatial_destroy(v);
}

TEST(VisuospatialLifecycle, DestroyNull) {
    /* Should not crash */
    reasoning_visuospatial_destroy(nullptr);
}

TEST(VisuospatialLifecycle, DefaultConfig) {
    visuospatial_config_t cfg = reasoning_visuospatial_default_config();
    EXPECT_EQ(cfg.max_objects, (uint32_t)VS_MAX_OBJECTS);
    EXPECT_FLOAT_EQ(cfg.proximity_threshold, VS_DEFAULT_PROXIMITY_THRESHOLD);
    EXPECT_FALSE(cfg.enable_3d);
}

TEST(VisuospatialLifecycle, CreateWithConfig) {
    visuospatial_config_t cfg = reasoning_visuospatial_default_config();
    cfg.enable_3d = true;
    cfg.proximity_threshold = 2.0f;
    reasoning_visuospatial_t* v = reasoning_visuospatial_create(&cfg);
    ASSERT_NE(v, nullptr);
    reasoning_visuospatial_destroy(v);
}

/* ===== Object Management Tests ===== */

TEST_F(VisuospatialTest, AddObject) {
    vs_point_t pos = {1.0f, 2.0f, 0.0f};
    int id = reasoning_visuospatial_add_object(vs, "chair", pos);
    EXPECT_GE(id, 0);
}

TEST_F(VisuospatialTest, AddObjectWithBounds) {
    vs_point_t pos = {5.0f, 5.0f, 0.0f};
    vs_bounds_t bounds = {{0.0f, 0.0f, 0.0f}, {10.0f, 10.0f, 0.0f}};
    int id = reasoning_visuospatial_add_object_with_bounds(vs, "room", pos, bounds);
    EXPECT_GE(id, 0);

    vs_object_t obj;
    EXPECT_EQ(reasoning_visuospatial_get_object(vs, (uint32_t)id, &obj), 0);
    EXPECT_TRUE(obj.has_bounds);
}

TEST_F(VisuospatialTest, AddMaxObjects) {
    for (uint32_t i = 0; i < VS_MAX_OBJECTS; i++) {
        vs_point_t pos = {(float)i, 0.0f, 0.0f};
        char name[32];
        snprintf(name, sizeof(name), "obj_%u", i);
        int id = reasoning_visuospatial_add_object(vs, name, pos);
        EXPECT_GE(id, 0) << "Failed to add object " << i;
    }
    /* Scene should be full now */
    vs_point_t pos = {999.0f, 0.0f, 0.0f};
    int id = reasoning_visuospatial_add_object(vs, "overflow", pos);
    EXPECT_EQ(id, -1);
}

TEST_F(VisuospatialTest, RemoveObject) {
    vs_point_t pos = {1.0f, 2.0f, 0.0f};
    int id = reasoning_visuospatial_add_object(vs, "table", pos);
    ASSERT_GE(id, 0);

    EXPECT_EQ(reasoning_visuospatial_remove_object(vs, (uint32_t)id), 0);

    /* Should not be retrievable after removal */
    vs_object_t obj;
    EXPECT_EQ(reasoning_visuospatial_get_object(vs, (uint32_t)id, &obj), -1);
}

TEST_F(VisuospatialTest, MoveObject) {
    vs_point_t pos = {1.0f, 2.0f, 0.0f};
    int id = reasoning_visuospatial_add_object(vs, "lamp", pos);
    ASSERT_GE(id, 0);

    vs_point_t new_pos = {5.0f, 6.0f, 0.0f};
    EXPECT_EQ(reasoning_visuospatial_move_object(vs, (uint32_t)id, new_pos), 0);

    vs_object_t obj;
    EXPECT_EQ(reasoning_visuospatial_get_object(vs, (uint32_t)id, &obj), 0);
    EXPECT_FLOAT_EQ(obj.position.x, 5.0f);
    EXPECT_FLOAT_EQ(obj.position.y, 6.0f);
}

TEST_F(VisuospatialTest, GetObject) {
    vs_point_t pos = {3.0f, 4.0f, 0.0f};
    int id = reasoning_visuospatial_add_object(vs, "bookshelf", pos);
    ASSERT_GE(id, 0);

    vs_object_t obj;
    EXPECT_EQ(reasoning_visuospatial_get_object(vs, (uint32_t)id, &obj), 0);
    EXPECT_EQ(obj.id, (uint32_t)id);
    EXPECT_STREQ(obj.name, "bookshelf");
    EXPECT_FLOAT_EQ(obj.position.x, 3.0f);
    EXPECT_FLOAT_EQ(obj.position.y, 4.0f);
}

/* ===== Relation Tests ===== */

TEST_F(VisuospatialTest, AddRelation) {
    vs_point_t p1 = {0.0f, 0.0f, 0.0f}, p2 = {5.0f, 5.0f, 0.0f};
    int id1 = reasoning_visuospatial_add_object(vs, "a", p1);
    int id2 = reasoning_visuospatial_add_object(vs, "b", p2);
    ASSERT_GE(id1, 0);
    ASSERT_GE(id2, 0);

    EXPECT_EQ(reasoning_visuospatial_add_relation(vs, VS_RELATION_NEAR,
        (uint32_t)id1, (uint32_t)id2, 0.9f), 0);
}

/* ===== Distance Tests ===== */

TEST_F(VisuospatialTest, Distance2D) {
    vs_point_t p1 = {0.0f, 0.0f, 0.0f};
    vs_point_t p2 = {3.0f, 4.0f, 0.0f};
    int id1 = reasoning_visuospatial_add_object(vs, "a", p1);
    int id2 = reasoning_visuospatial_add_object(vs, "b", p2);

    float d = reasoning_visuospatial_distance(vs, (uint32_t)id1, (uint32_t)id2);
    EXPECT_NEAR(d, 5.0f, 1e-5f); /* 3-4-5 triangle */
}

TEST_F(VisuospatialTest, Distance3D) {
    visuospatial_config_t cfg = reasoning_visuospatial_default_config();
    cfg.enable_3d = true;
    reasoning_visuospatial_destroy(vs);
    vs = reasoning_visuospatial_create(&cfg);
    ASSERT_NE(vs, nullptr);

    vs_point_t p1 = {0.0f, 0.0f, 0.0f};
    vs_point_t p2 = {1.0f, 2.0f, 2.0f};
    int id1 = reasoning_visuospatial_add_object(vs, "a", p1);
    int id2 = reasoning_visuospatial_add_object(vs, "b", p2);

    float d = reasoning_visuospatial_distance(vs, (uint32_t)id1, (uint32_t)id2);
    EXPECT_NEAR(d, 3.0f, 1e-5f); /* sqrt(1+4+4) = 3 */
}

/* ===== Relation Inference Tests ===== */

TEST_F(VisuospatialTest, InferRelationsAboveBelow) {
    /* threshold = 1.0, so y diff must be > 1.0 for ABOVE/BELOW */
    vs_point_t top = {0.0f, 5.0f, 0.0f};
    vs_point_t bottom = {0.0f, 0.0f, 0.0f};
    int id_top = reasoning_visuospatial_add_object(vs, "top", top);
    int id_bot = reasoning_visuospatial_add_object(vs, "bottom", bottom);

    int n = reasoning_visuospatial_infer_relations(vs);
    EXPECT_GT(n, 0);

    /* Check that ABOVE relation exists for top -> bottom */
    vs_query_t q;
    memset(&q, 0, sizeof(q));
    q.type = VS_QUERY_RELATION;
    q.object_a_id = (uint32_t)id_top;
    q.object_b_id = (uint32_t)id_bot;
    q.relation = VS_RELATION_ABOVE;

    vs_result_t r;
    EXPECT_EQ(reasoning_visuospatial_query(vs, &q, &r), 0);
    EXPECT_TRUE(r.holds);
}

TEST_F(VisuospatialTest, InferRelationsLeftRight) {
    vs_point_t left = {-5.0f, 0.0f, 0.0f};
    vs_point_t right = {5.0f, 0.0f, 0.0f};
    int id_l = reasoning_visuospatial_add_object(vs, "left", left);
    int id_r = reasoning_visuospatial_add_object(vs, "right", right);

    reasoning_visuospatial_infer_relations(vs);

    vs_query_t q;
    memset(&q, 0, sizeof(q));
    q.type = VS_QUERY_RELATION;
    q.object_a_id = (uint32_t)id_l;
    q.object_b_id = (uint32_t)id_r;
    q.relation = VS_RELATION_LEFT_OF;

    vs_result_t r;
    EXPECT_EQ(reasoning_visuospatial_query(vs, &q, &r), 0);
    EXPECT_TRUE(r.holds);
}

TEST_F(VisuospatialTest, InferRelationsNearFar) {
    /* Default threshold = 1.0 */
    vs_point_t near1 = {0.0f, 0.0f, 0.0f};
    vs_point_t near2 = {0.3f, 0.3f, 0.0f}; /* distance ~0.42, < 1.0 = NEAR */
    vs_point_t far1 = {100.0f, 100.0f, 0.0f}; /* distance >> 3.0 = FAR */

    int id1 = reasoning_visuospatial_add_object(vs, "a", near1);
    int id2 = reasoning_visuospatial_add_object(vs, "b", near2);
    int id3 = reasoning_visuospatial_add_object(vs, "c", far1);

    reasoning_visuospatial_infer_relations(vs);

    /* Check NEAR between a and b */
    vs_query_t q;
    memset(&q, 0, sizeof(q));
    q.type = VS_QUERY_RELATION;
    q.object_a_id = (uint32_t)id1;
    q.object_b_id = (uint32_t)id2;
    q.relation = VS_RELATION_NEAR;

    vs_result_t r;
    EXPECT_EQ(reasoning_visuospatial_query(vs, &q, &r), 0);
    EXPECT_TRUE(r.holds);

    /* Check FAR between a and c */
    q.object_a_id = (uint32_t)id1;
    q.object_b_id = (uint32_t)id3;
    q.relation = VS_RELATION_FAR;
    EXPECT_EQ(reasoning_visuospatial_query(vs, &q, &r), 0);
    EXPECT_TRUE(r.holds);
}

TEST_F(VisuospatialTest, InferRelationsInside) {
    /* Object A at center of room B's bounds */
    vs_point_t room_pos = {5.0f, 5.0f, 0.0f};
    vs_bounds_t room_bounds = {{0.0f, 0.0f, 0.0f}, {10.0f, 10.0f, 0.0f}};
    int room_id = reasoning_visuospatial_add_object_with_bounds(vs, "room", room_pos, room_bounds);

    vs_point_t chair_pos = {3.0f, 3.0f, 0.0f};
    vs_bounds_t chair_bounds = {{2.5f, 2.5f, 0.0f}, {3.5f, 3.5f, 0.0f}};
    int chair_id = reasoning_visuospatial_add_object_with_bounds(vs, "chair", chair_pos, chair_bounds);

    reasoning_visuospatial_infer_relations(vs);

    vs_query_t q;
    memset(&q, 0, sizeof(q));
    q.type = VS_QUERY_RELATION;
    q.object_a_id = (uint32_t)chair_id;
    q.object_b_id = (uint32_t)room_id;
    q.relation = VS_RELATION_INSIDE;

    vs_result_t r;
    EXPECT_EQ(reasoning_visuospatial_query(vs, &q, &r), 0);
    EXPECT_TRUE(r.holds);
}

TEST_F(VisuospatialTest, InferRelationsOverlapping) {
    vs_point_t p1 = {0.0f, 0.0f, 0.0f};
    vs_bounds_t b1 = {{-1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}};
    vs_point_t p2 = {0.5f, 0.5f, 0.0f};
    vs_bounds_t b2 = {{-0.5f, -0.5f, 0.0f}, {1.5f, 1.5f, 0.0f}};

    int id1 = reasoning_visuospatial_add_object_with_bounds(vs, "a", p1, b1);
    int id2 = reasoning_visuospatial_add_object_with_bounds(vs, "b", p2, b2);

    reasoning_visuospatial_infer_relations(vs);

    vs_query_t q;
    memset(&q, 0, sizeof(q));
    q.type = VS_QUERY_RELATION;
    q.object_a_id = (uint32_t)id1;
    q.object_b_id = (uint32_t)id2;
    q.relation = VS_RELATION_OVERLAPPING;

    vs_result_t r;
    EXPECT_EQ(reasoning_visuospatial_query(vs, &q, &r), 0);
    EXPECT_TRUE(r.holds);
}

/* ===== Query Tests ===== */

TEST_F(VisuospatialTest, QueryRelation) {
    vs_point_t p1 = {0.0f, 0.0f, 0.0f}, p2 = {5.0f, 0.0f, 0.0f};
    int id1 = reasoning_visuospatial_add_object(vs, "a", p1);
    int id2 = reasoning_visuospatial_add_object(vs, "b", p2);
    reasoning_visuospatial_add_relation(vs, VS_RELATION_NEAR, (uint32_t)id1, (uint32_t)id2, 0.8f);

    vs_query_t q;
    memset(&q, 0, sizeof(q));
    q.type = VS_QUERY_RELATION;
    q.object_a_id = (uint32_t)id1;
    q.object_b_id = (uint32_t)id2;
    q.relation = VS_RELATION_NEAR;

    vs_result_t r;
    EXPECT_EQ(reasoning_visuospatial_query(vs, &q, &r), 0);
    EXPECT_TRUE(r.holds);
    EXPECT_NEAR(r.confidence, 0.8f, 1e-5f);
}

TEST_F(VisuospatialTest, QueryDistance) {
    vs_point_t p1 = {0.0f, 0.0f, 0.0f}, p2 = {3.0f, 4.0f, 0.0f};
    int id1 = reasoning_visuospatial_add_object(vs, "a", p1);
    int id2 = reasoning_visuospatial_add_object(vs, "b", p2);

    vs_query_t q;
    memset(&q, 0, sizeof(q));
    q.type = VS_QUERY_DISTANCE;
    q.object_a_id = (uint32_t)id1;
    q.object_b_id = (uint32_t)id2;

    vs_result_t r;
    EXPECT_EQ(reasoning_visuospatial_query(vs, &q, &r), 0);
    EXPECT_NEAR(r.distance, 5.0f, 1e-5f);
}

TEST_F(VisuospatialTest, QueryNearest) {
    vs_point_t p1 = {0.0f, 0.0f, 0.0f};
    vs_point_t p2 = {1.0f, 0.0f, 0.0f};
    vs_point_t p3 = {10.0f, 0.0f, 0.0f};
    int id1 = reasoning_visuospatial_add_object(vs, "origin", p1);
    int id2 = reasoning_visuospatial_add_object(vs, "near", p2);
    reasoning_visuospatial_add_object(vs, "far", p3);

    vs_query_t q;
    memset(&q, 0, sizeof(q));
    q.type = VS_QUERY_NEAREST;
    q.object_a_id = (uint32_t)id1;

    vs_result_t r;
    EXPECT_EQ(reasoning_visuospatial_query(vs, &q, &r), 0);
    EXPECT_EQ(r.nearest_id, (uint32_t)id2);
    EXPECT_NEAR(r.distance, 1.0f, 1e-5f);
}

TEST_F(VisuospatialTest, QueryContains) {
    vs_point_t room_pos = {5.0f, 5.0f, 0.0f};
    vs_bounds_t room_bounds = {{0.0f, 0.0f, 0.0f}, {10.0f, 10.0f, 0.0f}};
    int room_id = reasoning_visuospatial_add_object_with_bounds(vs, "room", room_pos, room_bounds);

    vs_point_t inside_pos = {3.0f, 3.0f, 0.0f};
    int inside_id = reasoning_visuospatial_add_object(vs, "inside_obj", inside_pos);

    vs_query_t q;
    memset(&q, 0, sizeof(q));
    q.type = VS_QUERY_CONTAINS;
    q.object_a_id = (uint32_t)inside_id;
    q.object_b_id = (uint32_t)room_id;

    vs_result_t r;
    EXPECT_EQ(reasoning_visuospatial_query(vs, &q, &r), 0);
    EXPECT_TRUE(r.holds);
}

TEST_F(VisuospatialTest, QueryObjectsInRegion) {
    /* Place objects at various positions */
    vs_point_t p1 = {1.0f, 1.0f, 0.0f};
    vs_point_t p2 = {2.0f, 2.0f, 0.0f};
    vs_point_t p3 = {20.0f, 20.0f, 0.0f};
    int id1 = reasoning_visuospatial_add_object(vs, "a", p1);
    int id2 = reasoning_visuospatial_add_object(vs, "b", p2);
    reasoning_visuospatial_add_object(vs, "c", p3);

    vs_query_t q;
    memset(&q, 0, sizeof(q));
    q.type = VS_QUERY_OBJECTS_IN_REGION;
    q.region.min = {0.0f, 0.0f, 0.0f};
    q.region.max = {5.0f, 5.0f, 0.0f};

    vs_result_t r;
    EXPECT_EQ(reasoning_visuospatial_query(vs, &q, &r), 0);
    EXPECT_EQ(r.num_objects_in_region, 2u);
    /* Both id1 and id2 should be in the region */
    bool found1 = false, found2 = false;
    for (uint32_t i = 0; i < r.num_objects_in_region; i++) {
        if (r.objects_in_region[i] == (uint32_t)id1) found1 = true;
        if (r.objects_in_region[i] == (uint32_t)id2) found2 = true;
    }
    EXPECT_TRUE(found1);
    EXPECT_TRUE(found2);
}

TEST_F(VisuospatialTest, QueryPath) {
    /* Create a chain: A --NEAR-- B --NEAR-- C */
    vs_point_t pa = {0.0f, 0.0f, 0.0f};
    vs_point_t pb = {0.5f, 0.0f, 0.0f};
    vs_point_t pc = {1.0f, 0.0f, 0.0f};
    int a = reasoning_visuospatial_add_object(vs, "a", pa);
    int b = reasoning_visuospatial_add_object(vs, "b", pb);
    int c = reasoning_visuospatial_add_object(vs, "c", pc);

    reasoning_visuospatial_add_relation(vs, VS_RELATION_NEAR, (uint32_t)a, (uint32_t)b, 0.9f);
    reasoning_visuospatial_add_relation(vs, VS_RELATION_NEAR, (uint32_t)b, (uint32_t)c, 0.9f);

    vs_query_t q;
    memset(&q, 0, sizeof(q));
    q.type = VS_QUERY_PATH;
    q.object_a_id = (uint32_t)a;
    q.object_b_id = (uint32_t)c;

    vs_result_t r;
    EXPECT_EQ(reasoning_visuospatial_query(vs, &q, &r), 0);
    EXPECT_TRUE(r.holds);
}

TEST_F(VisuospatialTest, QueryPathNoPath) {
    /* Two disconnected objects */
    vs_point_t pa = {0.0f, 0.0f, 0.0f};
    vs_point_t pb = {100.0f, 100.0f, 0.0f};
    int a = reasoning_visuospatial_add_object(vs, "a", pa);
    int b = reasoning_visuospatial_add_object(vs, "b", pb);
    /* No relations added */

    vs_query_t q;
    memset(&q, 0, sizeof(q));
    q.type = VS_QUERY_PATH;
    q.object_a_id = (uint32_t)a;
    q.object_b_id = (uint32_t)b;

    vs_result_t r;
    EXPECT_EQ(reasoning_visuospatial_query(vs, &q, &r), 0);
    EXPECT_FALSE(r.holds);
}

/* ===== Utility Tests ===== */

TEST(VisuospatialUtility, GetRelationName) {
    EXPECT_STREQ(reasoning_visuospatial_get_relation_name(VS_RELATION_ABOVE), "ABOVE");
    EXPECT_STREQ(reasoning_visuospatial_get_relation_name(VS_RELATION_BELOW), "BELOW");
    EXPECT_STREQ(reasoning_visuospatial_get_relation_name(VS_RELATION_LEFT_OF), "LEFT_OF");
    EXPECT_STREQ(reasoning_visuospatial_get_relation_name(VS_RELATION_RIGHT_OF), "RIGHT_OF");
    EXPECT_STREQ(reasoning_visuospatial_get_relation_name(VS_RELATION_IN_FRONT), "IN_FRONT");
    EXPECT_STREQ(reasoning_visuospatial_get_relation_name(VS_RELATION_BEHIND), "BEHIND");
    EXPECT_STREQ(reasoning_visuospatial_get_relation_name(VS_RELATION_INSIDE), "INSIDE");
    EXPECT_STREQ(reasoning_visuospatial_get_relation_name(VS_RELATION_OUTSIDE), "OUTSIDE");
    EXPECT_STREQ(reasoning_visuospatial_get_relation_name(VS_RELATION_NEAR), "NEAR");
    EXPECT_STREQ(reasoning_visuospatial_get_relation_name(VS_RELATION_FAR), "FAR");
    EXPECT_STREQ(reasoning_visuospatial_get_relation_name(VS_RELATION_TOUCHING), "TOUCHING");
    EXPECT_STREQ(reasoning_visuospatial_get_relation_name(VS_RELATION_OVERLAPPING), "OVERLAPPING");
    EXPECT_STREQ(reasoning_visuospatial_get_relation_name(VS_RELATION_BETWEEN), "BETWEEN");
    /* Unknown type */
    EXPECT_STREQ(reasoning_visuospatial_get_relation_name((vs_relation_type_t)999), "UNKNOWN");
}

TEST_F(VisuospatialTest, GetStats) {
    visuospatial_stats_t stats;
    EXPECT_EQ(reasoning_visuospatial_get_stats(vs, &stats), 0);
    EXPECT_EQ(stats.num_objects, 0u);
    EXPECT_EQ(stats.num_relations, 0u);
    EXPECT_EQ(stats.num_queries, 0u);

    /* Add an object and check stats */
    vs_point_t p = {0.0f, 0.0f, 0.0f};
    reasoning_visuospatial_add_object(vs, "test", p);
    EXPECT_EQ(reasoning_visuospatial_get_stats(vs, &stats), 0);
    EXPECT_EQ(stats.num_objects, 1u);
}

/* ===== Null Input Tests ===== */

TEST(VisuospatialNullInputs, AddObjectNull) {
    vs_point_t pos = {0.0f, 0.0f, 0.0f};
    EXPECT_EQ(reasoning_visuospatial_add_object(nullptr, "test", pos), -1);
}

TEST(VisuospatialNullInputs, AddObjectNullName) {
    reasoning_visuospatial_t* v = reasoning_visuospatial_create(nullptr);
    ASSERT_NE(v, nullptr);
    vs_point_t pos = {0.0f, 0.0f, 0.0f};
    EXPECT_EQ(reasoning_visuospatial_add_object(v, nullptr, pos), -1);
    reasoning_visuospatial_destroy(v);
}

TEST(VisuospatialNullInputs, RemoveObjectNull) {
    EXPECT_EQ(reasoning_visuospatial_remove_object(nullptr, 0), -1);
}

TEST(VisuospatialNullInputs, MoveObjectNull) {
    vs_point_t pos = {0.0f, 0.0f, 0.0f};
    EXPECT_EQ(reasoning_visuospatial_move_object(nullptr, 0, pos), -1);
}

TEST(VisuospatialNullInputs, GetObjectNull) {
    vs_object_t obj;
    EXPECT_EQ(reasoning_visuospatial_get_object(nullptr, 0, &obj), -1);
}

TEST(VisuospatialNullInputs, AddRelationNull) {
    EXPECT_EQ(reasoning_visuospatial_add_relation(nullptr, VS_RELATION_NEAR, 0, 1, 1.0f), -1);
}

TEST(VisuospatialNullInputs, InferRelationsNull) {
    EXPECT_EQ(reasoning_visuospatial_infer_relations(nullptr), -1);
}

TEST(VisuospatialNullInputs, QueryNull) {
    vs_query_t q; vs_result_t r;
    memset(&q, 0, sizeof(q)); memset(&r, 0, sizeof(r));
    EXPECT_EQ(reasoning_visuospatial_query(nullptr, &q, &r), -1);
}

TEST(VisuospatialNullInputs, DistanceNull) {
    float d = reasoning_visuospatial_distance(nullptr, 0, 1);
    EXPECT_TRUE(std::isnan(d));
}

TEST(VisuospatialNullInputs, GetStatsNull) {
    visuospatial_stats_t stats;
    EXPECT_EQ(reasoning_visuospatial_get_stats(nullptr, &stats), -1);
}

/* ===== Reasoning Chain Integration ===== */

TEST(VisuospatialChainIntegration, StepTypeExists) {
    const char* name = reasoning_step_type_name(REASONING_STEP_VISUOSPATIAL);
    EXPECT_STREQ(name, "VISUOSPATIAL");
}

TEST(VisuospatialChainIntegration, DefaultConfigDisabled) {
    reasoning_engine_config_t cfg = reasoning_engine_default_config();
    EXPECT_FALSE(cfg.enable_visuospatial_reasoning);
}
