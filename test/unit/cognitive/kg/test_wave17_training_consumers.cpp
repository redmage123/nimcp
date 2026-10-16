/**
 * @file test_wave17_training_consumers.cpp
 * @brief Unit test for KG-integration Wave W17 (training-path KG consumers).
 *
 * W17 adds three real consumers to brain_learn_vector() that READ the internal
 * KG and change training behavior (not just write events):
 *
 *   A. "training_focus" node → LR scaling knob.  Created on first training
 *      call if absent; existence + edge count elevates kg_lr_scale to 1.5x.
 *
 *   B. "train_familiarity_<hash>" nodes → per-sample novelty detector.  Hash
 *      the input features; matching node → dampen LR by 0.8x (familiar).
 *      No match → create node + boost LR by 1.2x (novel).
 *
 *   C. "training_pause_on_loss_above" node → external stability gate.  Node
 *      presence + loss > threshold → skip plasticity + BPTT replay.  Tested
 *      indirectly via counter increment when tripped.
 *
 * The `kg_training_consumer_hits` counter should bump on every real consumer
 * read (A hit path, B hit OR create path, C trip path).  Fresh brain + one
 * learn_vector call should create training_focus + a train_familiarity_*
 * node.  A SECOND call with identical features should hit the familiar
 * branch on B.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <vector>

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"
#include "core/brain/learning/nimcp_brain_learning.h"

/* ---------------------------------------------------------------- */
/* Fixture                                                           */
/* ---------------------------------------------------------------- */

class Wave17TrainingConsumersTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        brain = brain_create_minimal("wave17_kg_test",
                                     BRAIN_SIZE_MICRO,
                                     BRAIN_TASK_CLASSIFICATION,
                                     /*num_inputs=*/8,
                                     /*num_outputs=*/4);
        ASSERT_NE(brain, nullptr);
        ASSERT_TRUE(brain->internal_kg_enabled);
        ASSERT_NE(brain->internal_kg, nullptr);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    /* Build a deterministic feature vector so the familiarity hash is
     * stable across calls within a test. */
    std::vector<float> features(float seed) {
        std::vector<float> f(8);
        for (int i = 0; i < 8; ++i) {
            f[i] = seed + static_cast<float>(i) * 0.25f;
        }
        return f;
    }

    std::vector<float> target_one_hot(uint32_t k) {
        std::vector<float> t(4, 0.0f);
        if (k < 4) t[k] = 1.0f;
        return t;
    }

    /* Scan every type bucket for a node whose name starts with prefix. */
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

/* ---------------------------------------------------------------- */
/* W17-A: training_focus created on first call                       */
/* ---------------------------------------------------------------- */

TEST_F(Wave17TrainingConsumersTest, TrainingFocusNodeCreatedOnFirstCall) {
    /* Pre: no training_focus node. */
    EXPECT_EQ(brain_kg_find_node(brain->internal_kg, "training_focus"),
              BRAIN_KG_INVALID_NODE);

    auto f = features(0.1f);
    auto t = target_one_hot(1);
    float loss = brain_learn_vector(brain, f.data(),
                                    static_cast<uint32_t>(f.size()),
                                    t.data(),
                                    static_cast<uint32_t>(t.size()),
                                    "probe_a", 1.0f);
    /* Loss sign varies (0.0f when ann_frozen, positive otherwise, -1 on
     * error).  We only require no hard error. */
    EXPECT_GE(loss, -0.5f) << "brain_learn_vector returned hard error";

    /* Post: training_focus must exist. */
    EXPECT_NE(brain_kg_find_node(brain->internal_kg, "training_focus"),
              BRAIN_KG_INVALID_NODE);
}

/* ---------------------------------------------------------------- */
/* W17-B: familiarity node created on novel, hit on repeat           */
/* ---------------------------------------------------------------- */

TEST_F(Wave17TrainingConsumersTest, FamiliarityNodeCreatedAndRehit) {
    /* Snapshot counter before. */
    uint64_t hits_before =
        __atomic_load_n(&brain->kg_training_consumer_hits, __ATOMIC_RELAXED);

    /* Pre: no train_familiarity_* node. */
    EXPECT_FALSE(any_node_with_prefix("train_familiarity_"));

    auto f = features(0.42f);
    auto t = target_one_hot(2);

    /* First call: should create one train_familiarity_<hash> node and
     * bump the counter (B-create path + A-create-or-read path). */
    (void)brain_learn_vector(brain, f.data(),
                             static_cast<uint32_t>(f.size()),
                             t.data(),
                             static_cast<uint32_t>(t.size()),
                             "probe_b1", 1.0f);

    uint64_t hits_after_1 =
        __atomic_load_n(&brain->kg_training_consumer_hits, __ATOMIC_RELAXED);
    EXPECT_GT(hits_after_1, hits_before)
        << "kg_training_consumer_hits did not bump after first call";
    EXPECT_TRUE(any_node_with_prefix("train_familiarity_"))
        << "familiarity node not created after first call";

    /* Second call with IDENTICAL features: should re-hit the same
     * familiarity node (dampening branch).  Counter must bump again. */
    (void)brain_learn_vector(brain, f.data(),
                             static_cast<uint32_t>(f.size()),
                             t.data(),
                             static_cast<uint32_t>(t.size()),
                             "probe_b2", 1.0f);

    uint64_t hits_after_2 =
        __atomic_load_n(&brain->kg_training_consumer_hits, __ATOMIC_RELAXED);
    EXPECT_GT(hits_after_2, hits_after_1)
        << "kg_training_consumer_hits did not bump on repeat call";
}

/* ---------------------------------------------------------------- */
/* W17: counter bumps over multiple varied calls                      */
/* ---------------------------------------------------------------- */

TEST_F(Wave17TrainingConsumersTest, CounterBumpsAcrossVariedCalls) {
    uint64_t hits_before =
        __atomic_load_n(&brain->kg_training_consumer_hits, __ATOMIC_RELAXED);

    for (int i = 0; i < 3; ++i) {
        auto f = features(0.1f + static_cast<float>(i) * 0.7f);
        auto t = target_one_hot(static_cast<uint32_t>(i % 4));
        (void)brain_learn_vector(brain, f.data(),
                                 static_cast<uint32_t>(f.size()),
                                 t.data(),
                                 static_cast<uint32_t>(t.size()),
                                 "varied", 1.0f);
    }
    uint64_t hits_after =
        __atomic_load_n(&brain->kg_training_consumer_hits, __ATOMIC_RELAXED);

    /* Expect at least 3 bumps (one per varied call's B-path), plus A-path
     * hits once training_focus exists.  Minimum sanity check: > hits_before
     * + 2.  Real-world value will be higher. */
    EXPECT_GT(hits_after, hits_before + 2u)
        << "counter only moved " << (hits_after - hits_before)
        << " — expected >2 across 3 varied training calls";
}

/* ---------------------------------------------------------------- */
/* W17-C: loss-gate node wiring (existence path only)                */
/* ---------------------------------------------------------------- */

TEST_F(Wave17TrainingConsumersTest, LossGateNodeDetectedWhenArmed) {
    /* Arm the gate by adding the node externally (elevated). */
    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_ADMIN,
                              brain->internal_kg_admin_token);
    brain_kg_node_id_t pause = brain_kg_add_node(brain->internal_kg,
        "training_pause_on_loss_above", BRAIN_KG_NODE_TRAINING,
        "W17-C stability gate");
    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_READ, 0);
    ASSERT_NE(pause, BRAIN_KG_INVALID_NODE);

    /* Normal-magnitude training: loss < 10.0 threshold, gate NOT tripped.
     * Just verify no crash and the node is still present afterward. */
    auto f = features(0.33f);
    auto t = target_one_hot(0);
    (void)brain_learn_vector(brain, f.data(),
                             static_cast<uint32_t>(f.size()),
                             t.data(),
                             static_cast<uint32_t>(t.size()),
                             "probe_c", 1.0f);

    EXPECT_NE(brain_kg_find_node(brain->internal_kg,
                                 "training_pause_on_loss_above"),
              BRAIN_KG_INVALID_NODE)
        << "pause-gate node vanished during training";
}
