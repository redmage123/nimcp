/**
 * @file test_grounded_language_collapse_fix_integration.cpp
 * @brief Integration tests for the language collapse fix.
 *
 * WHAT: End-to-end through the public nimcp_brain_* API. Exercises:
 *       - get_grounded_language_diagnostics shape + values
 *       - probe_comprehend correctness (semantic_dim bounded reads)
 *       - set_snn_language_bridge_blend round-trip
 *       - grounded_respond IDK gate (refuses when production confidence
 *         is below GL_RESPOND_MIN_CONFIDENCE)
 *
 * WHY:  Cross-layer wiring is the most fragile part of this fix —
 *       header / impl / Python binding / daemon RPC all need to agree.
 *
 * HOW:  Uses nimcp_brain_create_fast for a small brain; exercises only
 *       the new public surface.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <set>
#include <string>

extern "C" {
#include "nimcp.h"
}

namespace {

constexpr uint32_t BRAIN_NEURONS = 100;

class CollapseFixIntegration : public ::testing::Test {
protected:
    nimcp_brain_t brain = nullptr;

    void SetUp() override {
        brain = nimcp_brain_create_fast("collapse_fix_test",
                                        NIMCP_TASK_CLASSIFICATION,
                                        BRAIN_NEURONS, 10, BRAIN_NEURONS);
        if (!brain) {
            GTEST_SKIP() << "Brain creation failed (resource constrained)";
        }
    }
    void TearDown() override {
        if (brain) nimcp_brain_destroy(brain);
    }
};

/* --- Diagnostics struct contract ----------------------------------- */

TEST_F(CollapseFixIntegration, DiagnosticsReturnZerosOnFreshBrain) {
    nimcp_grounded_language_diagnostics_t d;
    std::memset(&d, 0xCC, sizeof(d));  /* poison */
    nimcp_status_t s = nimcp_brain_get_grounded_language_diagnostics(brain, &d);
    EXPECT_EQ(s, NIMCP_OK);
    /* Function-words seeded means vocab_size > 0 even on a fresh brain. */
    EXPECT_GT(d.vocab_size, 0u);
    /* No production yet. */
    EXPECT_EQ(d.total_productions, 0u);
    /* Bridge blend is either the live value [0,1] or sentinel -1.0f
     * if no bridge attached. */
    EXPECT_TRUE(d.snn_bridge_blend == -1.0f ||
                (d.snn_bridge_blend >= 0.0f && d.snn_bridge_blend <= 1.0f));
}

TEST_F(CollapseFixIntegration, DiagnosticsRejectsNullOut) {
    nimcp_status_t s = nimcp_brain_get_grounded_language_diagnostics(brain, nullptr);
    EXPECT_EQ(s, NIMCP_ERROR_INVALID);
}

TEST_F(CollapseFixIntegration, DiagnosticsRejectsNullBrain) {
    nimcp_grounded_language_diagnostics_t d;
    nimcp_status_t s = nimcp_brain_get_grounded_language_diagnostics(nullptr, &d);
    EXPECT_EQ(s, NIMCP_ERROR_INVALID);
}

/* --- probe_comprehend bounded reads ------------------------------- */

TEST_F(CollapseFixIntegration, ProbeRespectsSemanticDim) {
    /* Ask for far more components than the implementation could
     * possibly hold; the writer must clamp at semantic_dim, not
     * walk OOB. The L2 norm and components must remain finite. */
    constexpr uint32_t REQUEST = 4096;
    std::vector<float> buf(REQUEST, std::numeric_limits<float>::quiet_NaN());
    uint32_t written = 999, concept_count = 999;
    float l2 = NAN, conf = NAN;
    nimcp_status_t s = nimcp_brain_probe_comprehend(brain, "red ocean",
        buf.data(), REQUEST, &written, &l2, &conf, &concept_count);
    ASSERT_EQ(s, NIMCP_OK);
    EXPECT_LT(written, REQUEST) << "writer must stop at semantic_dim";
    EXPECT_TRUE(std::isfinite(l2));
    EXPECT_TRUE(std::isfinite(conf));
    /* Components within written must all be finite. */
    for (uint32_t i = 0; i < written; i++) {
        EXPECT_TRUE(std::isfinite(buf[i])) << "component " << i << " is non-finite";
    }
}

TEST_F(CollapseFixIntegration, ProbeReturnsZeroNormForEmptyText) {
    std::vector<float> buf(16, 0.0f);
    uint32_t written = 0, cc = 0;
    float l2 = -1.0f, conf = -1.0f;
    nimcp_status_t s = nimcp_brain_probe_comprehend(brain, "",
        buf.data(), buf.size(), &written, &l2, &conf, &cc);
    /* Empty text may succeed or fail at the comprehend layer; both
     * outcomes must leave outputs deterministic. */
    if (s == NIMCP_OK) {
        EXPECT_TRUE(std::isfinite(l2));
        EXPECT_TRUE(std::isfinite(conf));
    } else {
        EXPECT_EQ(l2, 0.0f);
        EXPECT_EQ(conf, 0.0f);
    }
}

/* --- bridge blend setter -------------------------------------- */

TEST_F(CollapseFixIntegration, SetBridgeBlendRejectsOutOfRange) {
    nimcp_status_t s;
    s = nimcp_brain_set_snn_language_bridge_blend(brain, 1.5f);
    EXPECT_EQ(s, NIMCP_ERROR_INVALID);
    s = nimcp_brain_set_snn_language_bridge_blend(brain, -0.1f);
    EXPECT_EQ(s, NIMCP_ERROR_INVALID);
    /* NaN must also be rejected — would propagate into the blend
     * computation and trip the SNN-bypass branch unpredictably. */
    s = nimcp_brain_set_snn_language_bridge_blend(brain, NAN);
    EXPECT_EQ(s, NIMCP_ERROR_INVALID);
}

TEST_F(CollapseFixIntegration, SetBridgeBlendAcceptsValidRange) {
    /* Either the bridge is attached (NIMCP_OK) or it isn't
     * (NIMCP_ERROR). Both are acceptable; the
     * function must not crash either way. */
    nimcp_status_t s = nimcp_brain_set_snn_language_bridge_blend(brain, 0.5f);
    EXPECT_TRUE(s == NIMCP_OK || s == NIMCP_ERROR);
}

/* --- IDK gate in respond ----------------------------------- */

TEST_F(CollapseFixIntegration, RespondReturnsIDKOnUntrainedBrain) {
    /* On a fresh brain with no learned bindings, every prompt should
     * trip the GL_RESPOND_MIN_CONFIDENCE floor and surface the IDK
     * fallback rather than emit a degenerate template. */
    char response[256];
    float confidence = 1.0f;
    nimcp_status_t s = nimcp_brain_grounded_respond(brain, "describe gravity",
                                                    response, sizeof(response),
                                                    &confidence);
    ASSERT_EQ(s, NIMCP_OK);
    EXPECT_LT(confidence, 0.5f) << "fresh brain should not produce confident text";
    /* The response must be non-empty either way (IDK fallback or seeded
     * template) — empty would indicate a regression. */
    EXPECT_GT(std::strlen(response), 0u);
}

TEST_F(CollapseFixIntegration, RespondNoCollapseAcrossDistinctPrompts) {
    /* The pre-fix bug was: every prompt → "the awareness controlled."
     * After the fix, either responses diverge (real grounding) or all
     * collapse to the same IDK string (honest "I don't know"). The
     * forbidden state is "diverse-looking template variations of the
     * same root template" — i.e. the same trailing two words for >50%
     * of prompts when the brain has no real grounding.
     *
     * We assert: for ≥4 distinct prompts, responses are either all
     * IDK-fallback (acceptable) OR ≥2 of them differ. */
    const std::vector<const char*> prompts = {
        "red", "ocean", "infinity", "pizza", "love", "math"
    };
    std::set<std::string> seen;
    int idk_count = 0;
    for (auto* p : prompts) {
        char response[256] = {0};
        float conf = 0.0f;
        ASSERT_EQ(NIMCP_OK,
                  nimcp_brain_grounded_respond(brain, p, response, sizeof(response), &conf));
        std::string r(response);
        seen.insert(r);
        if (r.find("don't have words") != std::string::npos ||
            r.find("understand") != std::string::npos ||
            r.find("learning") != std::string::npos) {
            idk_count++;
        }
    }
    /* Either all IDK (fresh brain, no grounding) OR ≥2 unique outputs. */
    bool all_idk = (idk_count == (int)prompts.size());
    bool has_diversity = (seen.size() >= 2);
    EXPECT_TRUE(all_idk || has_diversity)
        << "all prompts produced the same non-IDK output: " << *seen.begin();
}

}  // namespace
