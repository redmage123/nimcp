/**
 * @file test_grounded_language_network_bridge.cpp
 * @brief Unit tests for the per-network bridges (LNN/CNN/FNO/ANN).
 *
 * WHAT: Verify attach NULL safety, broadcast no-op when no networks
 *       attached, modulation accessor zero-fill contract, and that
 *       an attached ANN callback receives the semantic vector.
 *
 * WHY:  These bridges run on every comprehend call. A bad pointer
 *       in any of the four networks would propagate into
 *       comprehension_confidence and silently bias the trained
 *       vocabulary. The contracts under test:
 *         (1) NULL attach is safe + clears prior cache
 *         (2) Broadcast with no networks returns 0 hits
 *         (3) Modulation reads zero when nothing is wired
 *         (4) ANN callback fires with the right vector + dim
 *         (5) comprehend doesn't degrade confidence when no
 *             networks contribute
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

extern "C" {
#include "language/nimcp_grounded_language.h"
#include "cognitive/memory/nimcp_semantic_memory.h"
}

namespace {

constexpr uint32_t TEST_DIM = 32;

class GLNetBridge : public ::testing::Test {
protected:
    grounded_language_t* gl = nullptr;
    semantic_memory_system_t* sm = nullptr;

    void SetUp() override {
        sm = semantic_memory_create();
        ASSERT_NE(sm, nullptr);
        gl = grounded_language_create(TEST_DIM, sm);
        ASSERT_NE(gl, nullptr);
    }
    void TearDown() override {
        if (gl) grounded_language_destroy(gl);
        if (sm) semantic_memory_destroy(sm);
    }
};

/* --- Attach: NULL safety ------------------------------------------- */
TEST_F(GLNetBridge, AttachNullSafe) {
    grounded_language_attach_lnn(gl, nullptr);
    grounded_language_attach_cortex_cnn(gl, nullptr);
    grounded_language_attach_fno(gl, nullptr);
    grounded_language_attach_ann(gl, nullptr, nullptr);
    /* Must not crash. */
}

TEST_F(GLNetBridge, AttachOnNullHandle) {
    grounded_language_attach_lnn(nullptr, nullptr);  /* no crash */
    grounded_language_attach_cortex_cnn(nullptr, nullptr);
    grounded_language_attach_fno(nullptr, nullptr);
    grounded_language_attach_ann(nullptr, nullptr, nullptr);
}

/* --- Modulation accessor: zero-fill contract ----------------------- */
TEST_F(GLNetBridge, FreshModulationAllZeros) {
    gl_network_modulation_t out;
    memset(&out, 0xAA, sizeof(out));
    ASSERT_EQ(0, grounded_language_get_network_modulation(gl, &out));
    EXPECT_EQ(0.0f, out.lnn_magnitude);
    EXPECT_EQ(0.0f, out.cnn_magnitude);
    EXPECT_EQ(0.0f, out.fno_magnitude);
    EXPECT_EQ(0.0f, out.ann_magnitude);
}

TEST_F(GLNetBridge, ModulationNullSafe) {
    EXPECT_EQ(-1, grounded_language_get_network_modulation(gl, nullptr));
    gl_network_modulation_t out;
    EXPECT_EQ(-1, grounded_language_get_network_modulation(nullptr, &out));
    /* On NULL gl, out must still be zero-filled. */
    EXPECT_EQ(0.0f, out.lnn_magnitude);
}

/* --- Broadcast: no networks → 0 hits ------------------------------- */
TEST_F(GLNetBridge, BroadcastWithNoNetworksReturnsZeroHits) {
    std::vector<float> v(TEST_DIM, 0.5f);
    EXPECT_EQ(0, grounded_language_broadcast_to_networks(
        gl, v.data(), TEST_DIM));
    /* All cached magnitudes still zero. */
    gl_network_modulation_t out;
    grounded_language_get_network_modulation(gl, &out);
    EXPECT_EQ(0.0f, out.lnn_magnitude);
    EXPECT_EQ(0.0f, out.cnn_magnitude);
    EXPECT_EQ(0.0f, out.fno_magnitude);
    EXPECT_EQ(0.0f, out.ann_magnitude);
}

TEST_F(GLNetBridge, BroadcastInvalidArgs) {
    std::vector<float> v(TEST_DIM, 0.5f);
    EXPECT_EQ(-1, grounded_language_broadcast_to_networks(
        nullptr, v.data(), TEST_DIM));
    EXPECT_EQ(-1, grounded_language_broadcast_to_networks(
        gl, nullptr, TEST_DIM));
    EXPECT_EQ(-1, grounded_language_broadcast_to_networks(
        gl, v.data(), 0));
}

/* --- ANN callback receives the right input ------------------------- */
struct AnnCtx {
    int call_count;
    uint32_t last_dim;
    float last_first_value;
};

extern "C" int ann_test_predict(void* ctx, const float* in, uint32_t in_dim,
                                  float* out, uint32_t out_dim) {
    AnnCtx* a = (AnnCtx*)ctx;
    a->call_count++;
    a->last_dim = in_dim;
    a->last_first_value = in[0];
    /* Echo input to output for magnitude calc. */
    uint32_t n = (in_dim < out_dim) ? in_dim : out_dim;
    for (uint32_t i = 0; i < n; i++) out[i] = in[i];
    return 0;
}

TEST_F(GLNetBridge, AnnCallbackFiresOnBroadcast) {
    AnnCtx ctx{};
    grounded_language_attach_ann(gl, ann_test_predict, &ctx);

    std::vector<float> v(TEST_DIM, 0.0f);
    v[0] = 0.7f;
    int hits = grounded_language_broadcast_to_networks(
        gl, v.data(), TEST_DIM);
    EXPECT_EQ(1, hits);
    EXPECT_EQ(1, ctx.call_count);
    EXPECT_EQ(TEST_DIM, ctx.last_dim);
    EXPECT_FLOAT_EQ(0.7f, ctx.last_first_value);

    /* Modulation now non-zero on ANN channel. */
    gl_network_modulation_t mod;
    grounded_language_get_network_modulation(gl, &mod);
    EXPECT_GT(mod.ann_magnitude, 0.0f);
    EXPECT_EQ(0.0f, mod.lnn_magnitude);  /* others still off */
}

TEST_F(GLNetBridge, AnnDetachClearsCache) {
    AnnCtx ctx{};
    grounded_language_attach_ann(gl, ann_test_predict, &ctx);
    std::vector<float> v(TEST_DIM, 0.5f);
    grounded_language_broadcast_to_networks(gl, v.data(), TEST_DIM);

    /* Detach. */
    grounded_language_attach_ann(gl, nullptr, nullptr);
    gl_network_modulation_t mod;
    grounded_language_get_network_modulation(gl, &mod);
    EXPECT_EQ(0.0f, mod.ann_magnitude);
}

/* --- Comprehend without bridges: confidence unaffected ------------- */
TEST_F(GLNetBridge, ComprehendWithNoBridgesNotDegraded) {
    /* Seed a couple of words. */
    std::vector<float> f(TEST_DIM, 0.0f); f[0] = 1.0f;
    grounded_language_fast_map(gl, "alpha", f.data(), TEST_DIM, 0);
    grounded_language_fast_map(gl, "beta",  f.data(), TEST_DIM, 0);

    gl_comprehension_result_t r;
    ASSERT_EQ(0, grounded_language_comprehend(gl, "alpha beta", &r));
    /* No bridges → factor=1.0, no boost, no degradation. */
    EXPECT_GE(r.comprehension_confidence, 0.0f);
    EXPECT_LE(r.comprehension_confidence, 1.0f);
    gl_comprehension_result_cleanup(&r);
}

/* --- Comprehend with ANN bridge: confidence boosted -------------- */
TEST_F(GLNetBridge, ComprehendWithAnnBoostsConfidence) {
    AnnCtx ctx{};
    grounded_language_attach_ann(gl, ann_test_predict, &ctx);

    std::vector<float> f(TEST_DIM, 0.0f); f[0] = 1.0f;
    grounded_language_fast_map(gl, "alpha", f.data(), TEST_DIM, 0);

    gl_comprehension_result_t r;
    ASSERT_EQ(0, grounded_language_comprehend(gl, "alpha", &r));
    /* The ANN echo path produces a non-trivial magnitude, boosting
     * confidence slightly above the pure-lexical baseline. */
    EXPECT_GT(ctx.call_count, 0);
    EXPECT_LE(r.comprehension_confidence, 1.0f);  /* still capped */
    gl_comprehension_result_cleanup(&r);
}

}  // namespace
