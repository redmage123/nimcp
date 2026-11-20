/**
 * @file test_nlp_immune_coupling.cpp
 * @brief Cytokine clearance, the NLP-immune bridge's outcome window and
 *        capacity, and grounded_language's language capacity.
 */
#include <gtest/gtest.h>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

extern "C" {
#include "cognitive/immune/nimcp_brain_immune.h"
#include "nlp/immune/nimcp_nlp_immune_bridge.h"
#include "language/nimcp_grounded_language.h"
}

// Both flags are read once per process: set them before anything runs.
static const int kEnv = (setenv("NIMCP_IMMUNE_CYTOKINE_CLEARANCE", "1", 1),
                         setenv("NIMCP_IMMUNE_CYTOKINE_HALF_LIFE_MS", "200", 1), 0);

class ImmuneFixture : public ::testing::Test {
protected:
    brain_immune_system_t* im = nullptr;
    void SetUp() override {
        ASSERT_EQ(kEnv, 0);
        brain_immune_config_t cfg;
        brain_immune_default_config(&cfg);
        im = brain_immune_create(&cfg);
        ASSERT_NE(im, nullptr);
    }
    void TearDown() override { brain_immune_destroy(im); }
    brain_immune_stats_t stats() { brain_immune_stats_t s; brain_immune_get_stats(im, &s); return s; }
};

TEST_F(ImmuneFixture, ClearanceDecaysAndFreesThePool) {
    ASSERT_TRUE(brain_immune_cytokine_clearance_enabled());
    uint32_t id = 0;
    ASSERT_EQ(brain_immune_release_cytokine(im, BRAIN_CYTOKINE_IL1, 0, 0.8f, 0, &id), 0);
    float before = stats().cytokine_il1;
    EXPECT_GT(before, 0.5f);
    usleep(400 * 1000);                       // two half-lives
    float after = stats().cytokine_il1;
    EXPECT_LT(after, before * 0.4f);
    EXPECT_GT(after, 0.0f);
    usleep(1500 * 1000);                      // well below the 0.01 floor
    EXPECT_FLOAT_EQ(stats().cytokine_il1, 0.0f);
    // Without clearance the 128-slot pool fills and releases fail; with it,
    // spaced releases keep succeeding.
    for (int i = 0; i < 300; i++) {
        ASSERT_EQ(brain_immune_release_cytokine(im, BRAIN_CYTOKINE_IL6, 0, 0.02f, 0, &id), 0)
            << "release " << i;
        if (i % 50 == 49) usleep(250 * 1000);
    }
}

TEST_F(ImmuneFixture, OutcomeWindowSignalsAgainstOwnBaseline) {
    nlp_immune_bridge_t* b = nlp_immune_bridge_create(nullptr, im, nullptr);  // NLP net optional
    ASSERT_NE(b, nullptr);
    EXPECT_FLOAT_EQ(nlp_immune_compute_capacity(b), 1.0f);   // full before any update
    // Window 1 sets the baseline (60% errors) and signals nothing.
    EXPECT_EQ(nlp_immune_record_outcomes(b, 20, 30), 0);
    EXPECT_NEAR(b->baseline_error, 0.6f, 1e-5f);
    // Same error rate: no signal, even though 60% is above the old fixed
    // 30% threshold (a learner is not punished for being imperfect).
    EXPECT_EQ(nlp_immune_record_outcomes(b, 20, 30), 0);
    // Much worse than its own norm -> IL-1.
    EXPECT_EQ(nlp_immune_record_outcomes(b, 5, 45), 1);
    EXPECT_EQ(b->error_triggers, 1u);
    EXPECT_GT(stats().cytokine_il1, 0.0f);
    // Much better -> IL-10.
    EXPECT_EQ(nlp_immune_record_outcomes(b, 50, 0), 2);
    EXPECT_EQ(b->success_boosts, 1u);
    EXPECT_GT(stats().cytokine_il10, 0.0f);
    // Partial windows accumulate.
    EXPECT_EQ(nlp_immune_record_outcomes(b, 10, 10), 0);
    EXPECT_EQ(b->window_successes + b->window_failures, 20u);
    EXPECT_EQ(nlp_immune_record_outcomes(nullptr, 1, 1), -1);
    nlp_immune_bridge_destroy(b);
}

TEST_F(ImmuneFixture, CapacityFallsWithIL1AndRecoversAsItClears) {
    nlp_immune_bridge_t* b = nlp_immune_bridge_create(nullptr, im, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_EQ(nlp_immune_bridge_update(b, 10), 0);
    EXPECT_FLOAT_EQ(nlp_immune_compute_capacity(b), 1.0f);    // no cytokines
    uint32_t id;
    for (int i = 0; i < 3; i++)
        brain_immune_release_cytokine(im, BRAIN_CYTOKINE_IL1, 0, 1.0f, 0, &id);
    ASSERT_EQ(nlp_immune_bridge_update(b, 10), 0);
    float inflamed = nlp_immune_compute_capacity(b);
    EXPECT_LT(inflamed, 0.8f);
    EXPECT_GT(inflamed, 0.0f);
    // IL-10 offsets (it used to add to the loss).
    brain_immune_release_cytokine(im, BRAIN_CYTOKINE_IL10, 0, 1.0f, 0, &id);
    nlp_immune_bridge_update(b, 10);
    EXPECT_GE(nlp_immune_compute_capacity(b), inflamed);
    usleep(2500 * 1000);                                       // all cleared
    nlp_immune_bridge_update(b, 10);
    EXPECT_FLOAT_EQ(nlp_immune_compute_capacity(b), 1.0f);
    nlp_immune_bridge_destroy(b);
}

TEST(GroundedLanguageCapacity, ScalesComprehensionAndClamps) {
    grounded_language_t* gl = grounded_language_create(128, nullptr);
    ASSERT_NE(gl, nullptr);
    EXPECT_FLOAT_EQ(grounded_language_get_language_capacity(gl), 1.0f);  // default
    for (int i = 0; i < 20; i++)
        grounded_language_learn_from_text(gl, "the dog runs to the red ball and the cat sleeps");
    gl_comprehension_result_t r1, r2;
    memset(&r1, 0, sizeof r1); memset(&r2, 0, sizeof r2);
    ASSERT_EQ(grounded_language_comprehend(gl, "the dog runs", &r1), 0);
    grounded_language_set_language_capacity(gl, 0.5f);
    ASSERT_EQ(grounded_language_comprehend(gl, "the dog runs", &r2), 0);
    if (r1.comprehension_confidence > 0.0f) {
        EXPECT_LT(r2.comprehension_confidence, r1.comprehension_confidence);
    }
    gl_comprehension_result_cleanup(&r1);
    gl_comprehension_result_cleanup(&r2);
    grounded_language_set_language_capacity(gl, 5.0f);
    EXPECT_FLOAT_EQ(grounded_language_get_language_capacity(gl), 1.0f);
    grounded_language_set_language_capacity(gl, 0.0f);
    EXPECT_FLOAT_EQ(grounded_language_get_language_capacity(gl), 0.05f);
    EXPECT_FLOAT_EQ(grounded_language_get_language_capacity(nullptr), 1.0f);
    grounded_language_destroy(gl);
}
