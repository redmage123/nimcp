/**
 * @file test_reasoning_visuospatial_e2e.cpp
 * @brief End-to-end tests for visuospatial reasoning with a full brain
 *
 * WHAT: Tests complete visuospatial reasoning pipeline from scene creation to queries
 * WHY:  Verify the full spatial inference pipeline works with a live brain
 * HOW:  Create brain, build spatial scenes, run queries, validate results
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "cognitive/reasoning/nimcp_reasoning_visuospatial.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "core/brain/nimcp_brain.h"
}

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class VisuospatialE2ETest : public ::testing::Test {
protected:
    brain_t brain = NULL;

    void SetUp() override {
        brain = brain_create("vs_e2e", BRAIN_SIZE_SMALL,
                             BRAIN_TASK_CLASSIFICATION, 4, 2);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = NULL;
        }
    }
};

/*=============================================================================
 * E2E TESTS
 *===========================================================================*/

TEST_F(VisuospatialE2ETest, SpatialReasoningFullPipeline) {
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    /*
     * Full pipeline:
     * 1. Create visuospatial engine
     * 2. Build a room scene with multiple objects
     * 3. Infer relations from geometry
     * 4. Run multiple query types
     * 5. Build reasoning chain with visuospatial steps
     * 6. Verify correct spatial identification
     */

    /* Step 1: Create with default config */
    reasoning_visuospatial_t* vs = reasoning_visuospatial_create(NULL);
    ASSERT_NE(vs, nullptr);

    /* Step 2: Build a kitchen scene */
    vs_point_t room_pos = {5.0f, 5.0f, 0.0f};
    vs_bounds_t room_bounds = {{0.0f, 0.0f, 0.0f}, {10.0f, 10.0f, 0.0f}};
    int room = reasoning_visuospatial_add_object_with_bounds(vs, "kitchen", room_pos, room_bounds);
    ASSERT_GE(room, 0);

    vs_point_t table_pos = {3.0f, 3.0f, 0.0f};
    int table = reasoning_visuospatial_add_object(vs, "table", table_pos);
    ASSERT_GE(table, 0);

    vs_point_t lamp_pos = {3.0f, 8.0f, 0.0f};
    int lamp = reasoning_visuospatial_add_object(vs, "lamp", lamp_pos);
    ASSERT_GE(lamp, 0);

    vs_point_t chair_pos = {3.5f, 3.5f, 0.0f};
    int chair = reasoning_visuospatial_add_object(vs, "chair", chair_pos);
    ASSERT_GE(chair, 0);

    vs_point_t fridge_pos = {9.0f, 9.0f, 0.0f};
    int fridge = reasoning_visuospatial_add_object(vs, "fridge", fridge_pos);
    ASSERT_GE(fridge, 0);

    /* Step 3: Infer relations */
    int n_rels = reasoning_visuospatial_infer_relations(vs);
    EXPECT_GT(n_rels, 0);

    /* Step 4: Run queries */
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    /* Query 1: Is table inside kitchen? */
    vs_query_t q;
    memset(&q, 0, sizeof(q));
    q.type = VS_QUERY_CONTAINS;
    q.object_a_id = (uint32_t)table;
    q.object_b_id = (uint32_t)room;

    vs_result_t r;
    int rc = reasoning_visuospatial_query(vs, &q, &r);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(r.holds);

    /* Step 5: Add containment result as reasoning step */
    reasoning_step_t step;
    memset(&step, 0, sizeof(step));
    step.step_id = 0;
    step.type = REASONING_STEP_VISUOSPATIAL;
    step.confidence = r.confidence;
    step.relevance = 0.9f;
    snprintf(step.description, REASONING_STEP_DESC_LEN,
             "Spatial: %s", r.explanation);
    reasoning_chain_add_step(&chain, &step);

    /* Query 2: Distance from table to fridge */
    q.type = VS_QUERY_DISTANCE;
    q.object_a_id = (uint32_t)table;
    q.object_b_id = (uint32_t)fridge;
    rc = reasoning_visuospatial_query(vs, &q, &r);
    EXPECT_EQ(rc, 0);
    EXPECT_GT(r.distance, 0.0f);

    step.step_id = 1;
    step.confidence = r.confidence;
    snprintf(step.description, REASONING_STEP_DESC_LEN,
             "Spatial: distance = %.2f", r.distance);
    reasoning_chain_add_step(&chain, &step);

    /* Query 3: Nearest to chair */
    q.type = VS_QUERY_NEAREST;
    q.object_a_id = (uint32_t)chair;
    rc = reasoning_visuospatial_query(vs, &q, &r);
    EXPECT_EQ(rc, 0);
    /* Chair is at (3.5, 3.5), table at (3.0, 3.0) -> nearest should be table */
    EXPECT_EQ(r.nearest_id, (uint32_t)table);

    step.step_id = 2;
    step.confidence = r.confidence;
    snprintf(step.description, REASONING_STEP_DESC_LEN,
             "Spatial: nearest object to chair is id=%u", r.nearest_id);
    reasoning_chain_add_step(&chain, &step);

    /* Step 6: Verify chain */
    EXPECT_EQ(chain.num_steps, 3u);
    for (uint32_t i = 0; i < chain.num_steps; i++) {
        const reasoning_step_t* s = reasoning_chain_get_step(&chain, i);
        EXPECT_EQ(s->type, REASONING_STEP_VISUOSPATIAL);
        EXPECT_GT(s->confidence, 0.0f);
    }

    /* Verify stats */
    visuospatial_stats_t stats;
    reasoning_visuospatial_get_stats(vs, &stats);
    EXPECT_EQ(stats.num_objects, 5u);
    EXPECT_GT(stats.num_relations, 0u);
    EXPECT_GE(stats.num_queries, 3u);

    reasoning_chain_cleanup(&chain);
    reasoning_visuospatial_destroy(vs);
}

TEST_F(VisuospatialE2ETest, NavigationQuery) {
    /*
     * Simulate a navigation scenario: can an agent get from start to goal
     * by hopping through NEAR objects?
     *
     * Layout: A -- B -- C    (each within NEAR threshold of next)
     *                   |
     *                   D    (NEAR to C)
     *
     * Path A->D should exist: A -> B -> C -> D
     */
    visuospatial_config_t cfg = reasoning_visuospatial_default_config();
    cfg.proximity_threshold = 2.0f;  /* Objects within 2 units are NEAR */
    reasoning_visuospatial_t* vs = reasoning_visuospatial_create(&cfg);
    ASSERT_NE(vs, nullptr);

    /* Place objects along a navigable path */
    vs_point_t pA = {0.0f, 0.0f, 0.0f};
    vs_point_t pB = {1.5f, 0.0f, 0.0f};
    vs_point_t pC = {3.0f, 0.0f, 0.0f};
    vs_point_t pD = {3.0f, 1.5f, 0.0f};

    int a = reasoning_visuospatial_add_object(vs, "start", pA);
    int b = reasoning_visuospatial_add_object(vs, "waypoint1", pB);
    int c = reasoning_visuospatial_add_object(vs, "waypoint2", pC);
    int d = reasoning_visuospatial_add_object(vs, "goal", pD);

    ASSERT_GE(a, 0);
    ASSERT_GE(d, 0);

    /* Infer NEAR relations */
    int n = reasoning_visuospatial_infer_relations(vs);
    EXPECT_GT(n, 0);

    /* Query path: A -> D */
    vs_query_t q;
    memset(&q, 0, sizeof(q));
    q.type = VS_QUERY_PATH;
    q.object_a_id = (uint32_t)a;
    q.object_b_id = (uint32_t)d;

    vs_result_t r;
    int rc = reasoning_visuospatial_query(vs, &q, &r);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(r.holds);  /* Path exists */

    /* Place an isolated object far from the path */
    vs_point_t pE = {100.0f, 100.0f, 0.0f};
    int e = reasoning_visuospatial_add_object(vs, "isolated", pE);
    ASSERT_GE(e, 0);

    /* Re-infer relations (new object is isolated) */
    reasoning_visuospatial_infer_relations(vs);

    /* Query path: A -> E (should fail: E is isolated) */
    q.object_b_id = (uint32_t)e;
    rc = reasoning_visuospatial_query(vs, &q, &r);
    EXPECT_EQ(rc, 0);
    EXPECT_FALSE(r.holds);  /* No path exists */

    reasoning_visuospatial_destroy(vs);
}

TEST_F(VisuospatialE2ETest, SpatialWithCognitive) {
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    /*
     * Verify visuospatial reasoning integrates with the reasoning engine:
     * - Engine created with visuospatial enabled
     * - Run a standard reasoning query (visuospatial doesn't inject steps
     *   unless the engine has a visuospatial scene attached, which the
     *   standard pipeline does not do automatically)
     * - Manually create visuospatial steps and add to chain
     * - Verify stats reflect visuospatial usage
     */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.enable_visuospatial_reasoning = true;
    config.enable_convergent_reasoning = false;
    config.enable_concurrent_pipeline = false;

    reasoning_engine_t* engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);
    reasoning_engine_connect_brain(engine, brain);

    /* Run standard reasoning */
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    int rc = reasoning_engine_reason(engine, "where is the cat?", &chain);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(chain.is_complete);

    /* The engine ran; verify it didn't crash with visuospatial enabled */
    EXPECT_GT(chain.num_steps, 0u);

    /* Now use visuospatial subsystem independently alongside the engine */
    reasoning_visuospatial_t* vs = reasoning_visuospatial_create(NULL);
    ASSERT_NE(vs, nullptr);

    vs_point_t cat_pos = {2.0f, 1.0f, 0.0f};
    vs_point_t mat_pos = {2.0f, 1.0f, 0.0f};
    vs_bounds_t mat_bounds = {{1.0f, 0.0f, 0.0f}, {3.0f, 2.0f, 0.0f}};

    int cat = reasoning_visuospatial_add_object(vs, "cat", cat_pos);
    int mat = reasoning_visuospatial_add_object_with_bounds(vs, "mat", mat_pos, mat_bounds);
    ASSERT_GE(cat, 0);
    ASSERT_GE(mat, 0);

    reasoning_visuospatial_infer_relations(vs);

    /* Check containment: cat inside mat bounds? */
    vs_query_t q;
    memset(&q, 0, sizeof(q));
    q.type = VS_QUERY_CONTAINS;
    q.object_a_id = (uint32_t)cat;
    q.object_b_id = (uint32_t)mat;

    vs_result_t r;
    rc = reasoning_visuospatial_query(vs, &q, &r);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(r.holds);  /* Cat is on the mat */

    /* Verify engine stats */
    reasoning_engine_stats_t stats;
    reasoning_engine_get_stats(engine, &stats);
    EXPECT_EQ(stats.visuospatial_queries, 0u);  /* Engine itself didn't do VS queries */
    EXPECT_GE(stats.total_queries, 1u);

    reasoning_visuospatial_destroy(vs);
    reasoning_chain_cleanup(&chain);
    reasoning_engine_destroy(engine);
}

TEST_F(VisuospatialE2ETest, RegionQueryScenario) {
    /*
     * Create a room with scattered objects and query which objects
     * are within a sub-region of the room.
     */
    reasoning_visuospatial_t* vs = reasoning_visuospatial_create(NULL);
    ASSERT_NE(vs, nullptr);

    /* Add objects at various positions */
    vs_point_t positions[] = {
        {1.0f, 1.0f, 0.0f},   /* desk */
        {2.0f, 2.0f, 0.0f},   /* monitor */
        {8.0f, 8.0f, 0.0f},   /* bed */
        {9.0f, 9.0f, 0.0f},   /* nightstand */
        {5.0f, 5.0f, 0.0f},   /* rug */
    };
    const char* names[] = {"desk", "monitor", "bed", "nightstand", "rug"};

    int ids[5];
    for (int i = 0; i < 5; i++) {
        ids[i] = reasoning_visuospatial_add_object(vs, names[i], positions[i]);
        ASSERT_GE(ids[i], 0);
    }

    reasoning_visuospatial_infer_relations(vs);

    /* Query: which objects are in the lower-left quadrant (0,0)-(5,5)? */
    vs_query_t q;
    memset(&q, 0, sizeof(q));
    q.type = VS_QUERY_OBJECTS_IN_REGION;
    q.region.min = (vs_point_t){0.0f, 0.0f, 0.0f};
    q.region.max = (vs_point_t){5.0f, 5.0f, 0.0f};

    vs_result_t r;
    int rc = reasoning_visuospatial_query(vs, &q, &r);
    EXPECT_EQ(rc, 0);

    /* desk(1,1), monitor(2,2), rug(5,5) should be in the region */
    /* rug at (5,5) is on the boundary of max (5,5) -- depends on <= vs < */
    EXPECT_GE(r.num_objects_in_region, 2u);  /* At least desk and monitor */
    EXPECT_LE(r.num_objects_in_region, 3u);  /* Possibly rug on boundary */

    /* bed(8,8) and nightstand(9,9) should NOT be in the region */
    bool found_bed = false;
    bool found_nightstand = false;
    for (uint32_t i = 0; i < r.num_objects_in_region; i++) {
        if (r.objects_in_region[i] == (uint32_t)ids[2]) found_bed = true;
        if (r.objects_in_region[i] == (uint32_t)ids[3]) found_nightstand = true;
    }
    EXPECT_FALSE(found_bed);
    EXPECT_FALSE(found_nightstand);

    reasoning_visuospatial_destroy(vs);
}
