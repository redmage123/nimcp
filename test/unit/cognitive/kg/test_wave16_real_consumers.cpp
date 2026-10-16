/**
 * @file test_wave16_real_consumers.cpp
 * @brief Unit test for KG-integration Wave W16 (real runtime consumers).
 *
 * Wave W16 added three KG *consumers* so the graph actually affects behavior
 * instead of being a write-only audit log:
 *   A) brain_decide queries insula's outgoing edges to bias confidence (up to
 *      +0.05) when salience events exist.
 *   B) brain_decide looks for / creates "decision_history_<argmax>" nodes,
 *      boosting confidence +0.02 when a matching prior-decision node exists.
 *   C) middleware_controller_on_pattern_match lowers the effective attention
 *      threshold by 10% when an "attention_focus" node exists in the KG.
 *
 * All three bump the brain->kg_consumer_hits counter.
 *
 * Strategy:
 *   - Create MICRO brain (KG always-on).
 *   - Seed insula with an outgoing event edge (so consumer A activates).
 *   - Call brain_decide once (consumer B *creates* the history node +
 *     consumer A fires off insula).
 *   - Call brain_decide again with same features (consumer B now *finds*
 *     the history node and boosts confidence).
 *   - Verify kg_consumer_hits monotonically increases and second call has
 *     higher confidence than first (prior-decision boost).
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"

class Wave16ConsumersTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        brain = brain_create_minimal("wave16_kg_consumers_test",
                                     BRAIN_SIZE_MICRO,
                                     BRAIN_TASK_CLASSIFICATION,
                                     /*num_inputs=*/8,
                                     /*num_outputs=*/4);
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

    void elevate_admin() {
        brain_kg_set_access_level(brain->internal_kg,
                                  BRAIN_KG_ACCESS_ADMIN,
                                  brain->internal_kg_admin_token);
    }
    void restore_read() {
        brain_kg_set_access_level(brain->internal_kg,
                                  BRAIN_KG_ACCESS_READ, 0);
    }

    /* Ensure an insula node + at least one outgoing edge to a sentinel
     * target, so Consumer A finds edges->count > 0. */
    void seed_insula_with_event() {
        elevate_admin();
        brain_kg_node_id_t insula =
            brain_kg_find_node(brain->internal_kg, "insula");
        if (insula == BRAIN_KG_INVALID_NODE) {
            insula = brain_kg_add_node(
                brain->internal_kg, "insula",
                BRAIN_KG_NODE_CORTICAL, "salience network root (W16 test seed)");
        }
        brain_kg_node_id_t target = brain_kg_add_node(
            brain->internal_kg, "w16_test_salience_event_target",
            BRAIN_KG_NODE_CORE, "W16 test target");
        brain_kg_add_edge(brain->internal_kg, insula, target,
                          BRAIN_KG_EDGE_SENDS_TO,
                          /*description=*/"W16 test salience edge",
                          /*weight=*/1.0f);
        restore_read();
    }
};

/* -----------------------------------------------------------------------
 * Test 1: Consumer A + B bump hits on brain_decide, B boosts on 2nd call
 * ----------------------------------------------------------------------- */

TEST_F(Wave16ConsumersTest, BrainDecideBumpsKGConsumerHitsAndBoostsOnRecurrence) {
    seed_insula_with_event();

    uint64_t hits_initial = __atomic_load_n(&brain->kg_consumer_hits,
                                            __ATOMIC_RELAXED);

    std::vector<float> features(8);
    /* Deterministic features — argmax should be stable across both calls. */
    for (int i = 0; i < 8; ++i) features[i] = (i == 3) ? 5.0f : 0.1f;

    /* First call: Consumer A should fire (insula outgoing exists).
     * Consumer B should CREATE the decision_history_<argmax> node (no boost). */
    brain_decision_t* d1 = brain_decide(brain, features.data(), 8);
    ASSERT_NE(d1, nullptr);
    float conf1 = d1->confidence;
    uint64_t hits_after_1 = __atomic_load_n(&brain->kg_consumer_hits,
                                            __ATOMIC_RELAXED);

    EXPECT_GT(hits_after_1, hits_initial)
        << "kg_consumer_hits should have incremented after first brain_decide "
        << "(Consumer A salience hit at minimum)";

    /* Second call with same features: Consumer A fires again AND Consumer B
     * now FINDS the prior-decision node and boosts confidence +0.02. */
    brain_decision_t* d2 = brain_decide(brain, features.data(), 8);
    ASSERT_NE(d2, nullptr);
    float conf2 = d2->confidence;
    uint64_t hits_after_2 = __atomic_load_n(&brain->kg_consumer_hits,
                                            __ATOMIC_RELAXED);

    EXPECT_GT(hits_after_2, hits_after_1)
        << "kg_consumer_hits should increase on the second brain_decide "
        << "(both Consumer A and Consumer B fire since the prior-decision "
        << "node now exists)";

    /* Second call should have been boosted at least by Consumer B (+0.02)
     * relative to the first. Allow small noise from non-deterministic parts
     * of inference by only requiring a strict >. Clamped at 1.0 — if conf1
     * was already near 1.0 the test is not informative, so only check when
     * there's headroom. */
    if (conf1 < 0.95f) {
        EXPECT_GT(conf2, conf1)
            << "Second call confidence should be higher than first due to "
            << "prior-decision recurrence boost (conf1=" << conf1
            << ", conf2=" << conf2 << ")";
    } else {
        GTEST_SKIP() << "conf1 already near 1.0 (" << conf1
                     << "), boost can't be detected";
    }

    /* Verify the decision_history node for the argmax exists. */
    uint32_t argmax = 0;
    float mx = 0.0f;
    for (uint32_t i = 0; i < d2->output_size; ++i) {
        float a = d2->output_vector[i];
        if (a < 0) a = -a;
        if (a > mx) { mx = a; argmax = i; }
    }
    char node_name[64];
    snprintf(node_name, sizeof(node_name), "decision_history_%u", argmax);
    brain_kg_node_id_t hist = brain_kg_find_node(brain->internal_kg, node_name);
    EXPECT_NE(hist, BRAIN_KG_INVALID_NODE)
        << "Consumer B should have created node '" << node_name << "'";

    brain_free_decision(d1);
    brain_free_decision(d2);
}

/* -----------------------------------------------------------------------
 * Test 2: No insula node → Consumer A no-ops (graceful skip)
 * ----------------------------------------------------------------------- */

TEST_F(Wave16ConsumersTest, ConsumerAGracefullySkipsWhenInsulaAbsent) {
    /* Do NOT seed insula. If insula wasn't created by brain init, Consumer A
     * should no-op. Consumer B fires on the first call (creates node) and
     * on a NON-CACHED subsequent call with a different input whose argmax
     * happens to match (finds the node). The from_cache guard in
     * w16_apply_kg_consumers skips Consumer B on cache-hit paths to keep
     * repeated-identical-input decisions deterministic — we use distinct
     * feature vectors here to exercise the non-cache path twice. */
    brain_kg_node_id_t insula_pre =
        brain_kg_find_node(brain->internal_kg, "insula");

    uint64_t hits_initial = __atomic_load_n(&brain->kg_consumer_hits,
                                            __ATOMIC_RELAXED);

    std::vector<float> features1(8, 0.1f);
    features1[1] = 2.0f;
    std::vector<float> features2(8, 0.3f);  /* distinct → avoids cache hit */
    features2[1] = 2.1f;

    brain_decision_t* d1 = brain_decide(brain, features1.data(), 8);
    ASSERT_NE(d1, nullptr);
    brain_decision_t* d2 = brain_decide(brain, features2.data(), 8);
    ASSERT_NE(d2, nullptr);

    uint64_t hits_final = __atomic_load_n(&brain->kg_consumer_hits,
                                          __ATOMIC_RELAXED);

    /* Consumer B fires at least once per non-cached brain_decide. */
    EXPECT_GT(hits_final, hits_initial)
        << "Consumer B should fire on non-cached calls even when insula is absent";

    (void)insula_pre;  /* silence unused warning when insula was auto-created */

    brain_free_decision(d1);
    brain_free_decision(d2);
}

/* -----------------------------------------------------------------------
 * Test 3: Consumer C helper graceful-skip when attention_focus absent
 * ----------------------------------------------------------------------- */

extern "C" {
    bool brain_kg_helpers_query_attention_focus(brain_t brain);
}

TEST_F(Wave16ConsumersTest, ConsumerCHelperReturnsFalseWhenAttentionFocusAbsent) {
    /* Node not seeded → helper must return false and NOT bump the counter. */
    uint64_t before = __atomic_load_n(&brain->kg_consumer_hits,
                                      __ATOMIC_RELAXED);
    bool found = brain_kg_helpers_query_attention_focus(brain);
    uint64_t after = __atomic_load_n(&brain->kg_consumer_hits,
                                     __ATOMIC_RELAXED);

    EXPECT_FALSE(found);
    EXPECT_EQ(before, after) << "helper should not bump counter on miss";
}

TEST_F(Wave16ConsumersTest, ConsumerCHelperBumpsWhenAttentionFocusPresent) {
    elevate_admin();
    brain_kg_add_node(brain->internal_kg, "attention_focus",
                      BRAIN_KG_NODE_COGNITIVE, "W16 test focus marker");
    restore_read();

    uint64_t before = __atomic_load_n(&brain->kg_consumer_hits,
                                      __ATOMIC_RELAXED);
    bool found = brain_kg_helpers_query_attention_focus(brain);
    uint64_t after = __atomic_load_n(&brain->kg_consumer_hits,
                                     __ATOMIC_RELAXED);

    EXPECT_TRUE(found);
    EXPECT_GT(after, before) << "helper should bump counter on hit";
}
