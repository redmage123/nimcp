/**
 * @file test_delta.cpp
 * @brief GoogleTest unit tests for NIMCP edge delta weight push subsystem
 *
 * Tests delta computation, application, sparsity thresholding,
 * compression round-trips, and cleanup.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "edge/nimcp_edge.h"
#include "edge/nimcp_edge_types.h"
}

class DeltaTest : public ::testing::Test {
protected:
    nimcp_weight_delta_t delta;

    void SetUp() override {
        memset(&delta, 0, sizeof(delta));
    }

    void TearDown() override {
        nimcp_weight_delta_destroy(&delta);
    }
};

TEST_F(DeltaTest, ComputeIdenticalWeightsZeroChanges) {
    float weights[] = {1.0f, 2.0f, 3.0f, 4.0f};

    int ret = nimcp_weight_delta_compute(weights, weights, 4, 0.01f, &delta);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(delta.num_changes, 0u);
}

TEST_F(DeltaTest, ComputeOneWeightChanged) {
    float old_w[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float new_w[] = {1.0f, 2.0f, 5.0f, 4.0f}; // index 2 changed

    int ret = nimcp_weight_delta_compute(old_w, new_w, 4, 0.01f, &delta);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(delta.num_changes, 1u);

    // The delta should contain the change at the correct index
    ASSERT_NE(delta.weight_deltas, nullptr);
    EXPECT_NEAR(delta.weight_deltas[0], 2.0f, 0.01f);
}

TEST_F(DeltaTest, ComputeAllWeightsChanged) {
    float old_w[] = {0.0f, 0.0f, 0.0f, 0.0f};
    float new_w[] = {1.0f, 2.0f, 3.0f, 4.0f};

    int ret = nimcp_weight_delta_compute(old_w, new_w, 4, 0.01f, &delta);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(delta.num_changes, 4u);
}

TEST_F(DeltaTest, SparsityThresholdFiltersSmallDeltas) {
    float old_w[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float new_w[] = {1.001f, 2.0f, 3.0f, 5.0f}; // index 0: tiny change, index 3: large

    int ret = nimcp_weight_delta_compute(old_w, new_w, 4, 0.01f, &delta);
    EXPECT_EQ(ret, 0);

    // Only index 3 should be included (change > threshold)
    EXPECT_EQ(delta.num_changes, 1u);
}

TEST_F(DeltaTest, ApplyDeltaModifiesWeights) {
    float old_w[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float new_w[] = {1.0f, 4.0f, 3.0f, 6.0f};

    nimcp_weight_delta_compute(old_w, new_w, 4, 0.01f, &delta);

    float target[] = {1.0f, 2.0f, 3.0f, 4.0f};
    int ret = nimcp_weight_delta_apply(target, &delta);
    EXPECT_EQ(ret, 0);

    // target should now match new_w
    for (int i = 0; i < 4; i++) {
        EXPECT_NEAR(target[i], new_w[i], 0.01f)
            << "Mismatch at index " << i;
    }
}

TEST_F(DeltaTest, ApplyDeltaUnchangedWeightsStaySame) {
    float old_w[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float new_w[] = {1.0f, 2.0f, 5.0f, 4.0f}; // only index 2 changed

    nimcp_weight_delta_compute(old_w, new_w, 4, 0.01f, &delta);

    float target[] = {1.0f, 2.0f, 3.0f, 4.0f};
    nimcp_weight_delta_apply(target, &delta);

    EXPECT_FLOAT_EQ(target[0], 1.0f); // unchanged
    EXPECT_FLOAT_EQ(target[1], 2.0f); // unchanged
    EXPECT_NEAR(target[2], 5.0f, 0.01f); // changed
    EXPECT_FLOAT_EQ(target[3], 4.0f); // unchanged
}

TEST_F(DeltaTest, CompressDecompressRoundTrip) {
    float old_w[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float new_w[] = {1.5f, 2.5f, 3.5f, 4.5f, 5.5f};

    nimcp_weight_delta_compute(old_w, new_w, 5, 0.01f, &delta);
    EXPECT_EQ(delta.num_changes, 5u);

    // Save original deltas
    std::vector<float> original_deltas(delta.weight_deltas,
        delta.weight_deltas + delta.num_changes);

    int ret = nimcp_weight_delta_compress(&delta);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(delta.compressed_size, 0u);

    ret = nimcp_weight_delta_decompress(&delta);
    EXPECT_EQ(ret, 0);

    // Verify deltas are preserved
    for (uint32_t i = 0; i < delta.num_changes; i++) {
        EXPECT_NEAR(delta.weight_deltas[i], original_deltas[i], 1e-6f);
    }
}

TEST_F(DeltaTest, DestroyFreesMemory) {
    float old_w[] = {1.0f, 2.0f};
    float new_w[] = {3.0f, 4.0f};

    nimcp_weight_delta_compute(old_w, new_w, 2, 0.01f, &delta);

    nimcp_weight_delta_destroy(&delta);
    EXPECT_EQ(delta.weight_deltas, nullptr);
    EXPECT_EQ(delta.num_changes, 0u);

    // Reset to prevent double-free in TearDown
    memset(&delta, 0, sizeof(delta));
}

TEST_F(DeltaTest, LargeWeightArray) {
    const uint32_t n = 10000;
    std::vector<float> old_w(n, 1.0f);
    std::vector<float> new_w(n, 1.0f);

    // Change every 10th weight
    for (uint32_t i = 0; i < n; i += 10) {
        new_w[i] = 2.0f;
    }

    int ret = nimcp_weight_delta_compute(old_w.data(), new_w.data(), n, 0.01f, &delta);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(delta.num_changes, n / 10);
}

TEST_F(DeltaTest, NegativeDeltasWorkCorrectly) {
    float old_w[] = {5.0f, 10.0f, 15.0f};
    float new_w[] = {2.0f, 5.0f, 10.0f};

    nimcp_weight_delta_compute(old_w, new_w, 3, 0.01f, &delta);
    EXPECT_EQ(delta.num_changes, 3u);

    float target[] = {5.0f, 10.0f, 15.0f};
    nimcp_weight_delta_apply(target, &delta);

    for (int i = 0; i < 3; i++) {
        EXPECT_NEAR(target[i], new_w[i], 0.01f);
    }
}

TEST_F(DeltaTest, ZeroThresholdIncludesAllChanges) {
    float old_w[] = {1.0f, 2.0f, 3.0f};
    float new_w[] = {1.0001f, 2.0001f, 3.0001f}; // tiny changes

    nimcp_weight_delta_compute(old_w, new_w, 3, 0.0f, &delta);
    EXPECT_EQ(delta.num_changes, 3u);
}

TEST_F(DeltaTest, CompressDecompressLZ4RoundTrip) {
    float old_w[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    float new_w[] = {1.5f, 2.5f, 3.5f, 4.5f, 5.5f, 6.5f, 7.5f, 8.5f};

    nimcp_weight_delta_compute(old_w, new_w, 8, 0.01f, &delta);
    EXPECT_EQ(delta.num_changes, 8u);

    // Save original deltas for comparison
    std::vector<float> original_deltas(delta.weight_deltas,
        delta.weight_deltas + delta.num_changes);

    // Compress
    int ret = nimcp_weight_delta_compress(&delta);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(delta.compressed_size, 0u);
    ASSERT_NE(delta.compressed_data, nullptr);

    // Decompress
    ret = nimcp_weight_delta_decompress(&delta);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(delta.compressed_data, nullptr);
    EXPECT_EQ(delta.compressed_size, 0u);

    // Verify deltas are preserved exactly
    for (uint32_t i = 0; i < delta.num_changes; i++) {
        EXPECT_FLOAT_EQ(delta.weight_deltas[i], original_deltas[i])
            << "Mismatch at index " << i;
    }
}

TEST_F(DeltaTest, CompressEmptyDelta) {
    // Delta with 0 changes
    float weights[] = {1.0f, 2.0f, 3.0f};
    nimcp_weight_delta_compute(weights, weights, 3, 0.01f, &delta);
    EXPECT_EQ(delta.num_changes, 0u);

    int ret = nimcp_weight_delta_compress(&delta);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(delta.compressed_size, 0u);
}
