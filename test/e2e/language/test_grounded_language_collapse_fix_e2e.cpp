/**
 * @file test_grounded_language_collapse_fix_e2e.cpp
 * @brief End-to-end test: diverse-prompt eval through full brain.
 *
 * WHAT: Drives N distinct prompts through nimcp_brain_grounded_respond
 *       on a freshly-created brain and asserts the user-visible
 *       collapse signature does NOT appear:
 *         - Either ≥60% unique outputs (the brain has real grounding), OR
 *         - All outputs are honest IDK fallbacks (no grounding yet).
 *       The forbidden state is "all prompts emit the same non-IDK
 *       template," which was the pre-fix bug.
 *
 * WHY:  This is the test that would have caught the original mode
 *       collapse. Run after every change to the language module.
 *
 * HOW:  Public NIMCP API only — no internal struct access.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <set>
#include <string>
#include <vector>

extern "C" {
#include "nimcp.h"
}

namespace {

constexpr uint32_t BRAIN_NEURONS = 100;

class CollapseFixE2E : public ::testing::Test {
protected:
    nimcp_brain_t brain = nullptr;
    void SetUp() override {
        brain = nimcp_brain_create_fast("collapse_e2e",
                                        NIMCP_TASK_CLASSIFICATION,
                                        BRAIN_NEURONS, 10, BRAIN_NEURONS);
        if (!brain) GTEST_SKIP() << "Brain creation failed";
    }
    void TearDown() override {
        if (brain) nimcp_brain_destroy(brain);
    }

    /* DRY: drives a prompt through grounded_respond and returns the
     * response/confidence pair. */
    struct RespResult { std::string response; float confidence; };
    RespResult respond(const char* text) {
        char buf[256] = {0};
        float conf = 0.0f;
        nimcp_status_t s = nimcp_brain_grounded_respond(brain, text,
                                                        buf, sizeof(buf),
                                                        &conf);
        EXPECT_EQ(s, NIMCP_OK);
        return {std::string(buf), conf};
    }

    /* "Looks like an IDK" — honest non-confident fallback. */
    static bool is_idk(const std::string& r) {
        return r.find("don't have words") != std::string::npos ||
               r.find("understand about") != std::string::npos ||
               r.find("am learning")      != std::string::npos ||
               r.find("do not understand") != std::string::npos;
    }
};

TEST_F(CollapseFixE2E, DiverseInputsDoNotAllCollapseToOneTemplate) {
    /* 12 deliberately diverse prompts spanning concrete nouns, verbs,
     * adjectives, abstractions, and emotional content. Pre-fix bug:
     * all 12 produced "the awareness controlled." or trivial variants. */
    const std::vector<const char*> prompts = {
        "red", "ocean", "infinity", "pizza", "love", "math",
        "running fast", "the cat sleeps", "warm sand",
        "tell me about gravity", "what is fear", "two plus two"
    };

    std::set<std::string> seen;
    int idk = 0;
    float conf_sum = 0.0f;
    for (auto* p : prompts) {
        auto r = respond(p);
        seen.insert(r.response);
        conf_sum += r.confidence;
        if (is_idk(r.response)) idk++;
    }

    /* Acceptance:
     *   - ALL responses are IDK fallbacks (untrained brain — honest), OR
     *   - At least 60% unique responses (trained brain has grounding).
     *
     * The forbidden state is the pre-fix "all 12 prompts → identical
     * non-IDK template" — exactly the case where seen.size() == 1 AND
     * idk < N. */
    bool all_idk = (idk == (int)prompts.size());
    bool diverse = (seen.size() * 100 >= prompts.size() * 60);
    EXPECT_TRUE(all_idk || diverse)
        << "Mode collapse: " << seen.size() << " unique out of "
        << prompts.size() << " prompts; idk=" << idk
        << "; representative output: " << *seen.begin();

    /* Confidence should also be low (untrained brain) — sanity check
     * that we aren't fabricating high confidence. */
    float mean_conf = conf_sum / (float)prompts.size();
    EXPECT_LT(mean_conf, 0.5f) << "untrained brain claiming high confidence";
}

TEST_F(CollapseFixE2E, IdenticalInputsProduceIdenticalResponses) {
    /* Determinism: the same prompt twice in a row must produce the same
     * answer. (Diversity injection is deterministic via input-hash seed.) */
    auto r1 = respond("the cat sleeps");
    auto r2 = respond("the cat sleeps");
    EXPECT_EQ(r1.response, r2.response);
}

TEST_F(CollapseFixE2E, DiagnosticsReachableThroughoutSession) {
    /* Make sure the diagnostics RPC remains responsive after several
     * respond calls — guards against stats-counter corruption. */
    for (int i = 0; i < 5; i++) {
        respond("hello");
    }
    nimcp_grounded_language_diagnostics_t d;
    nimcp_status_t s = nimcp_brain_get_grounded_language_diagnostics(brain, &d);
    EXPECT_EQ(s, NIMCP_OK);
    EXPECT_GT(d.vocab_size, 0u);
    /* Comprehensions counter must have advanced (each respond calls
     * comprehend internally). */
    EXPECT_GE(d.total_comprehensions, 5u);
}

}  // namespace
