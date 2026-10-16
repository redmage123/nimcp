/**
 * @file test_versioning.cpp
 * @brief GoogleTest unit tests for NIMCP edge model versioning subsystem
 *
 * Tests version compatibility checks, architecture hashing, and migration paths.
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "edge/nimcp_edge.h"
#include "edge/nimcp_edge_types.h"
}

class VersioningTest : public ::testing::Test {
protected:
    void TearDown() override {}
};

TEST_F(VersioningTest, SameVersionIsCompatible) {
    uint32_t layers[] = {128, 256, 128};
    nimcp_model_version_t v1 = nimcp_version_create(1, 0, 0, layers, 3);
    nimcp_model_version_t v2 = nimcp_version_create(1, 0, 0, layers, 3);

    nimcp_compatibility_result_t result;
    int ret = nimcp_version_check_compatibility(&v1, &v2, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.architecturally_compatible);
    EXPECT_TRUE(result.delta_compatible);
}

TEST_F(VersioningTest, DifferentMajorIsIncompatible) {
    uint32_t layers[] = {128, 256, 128};
    nimcp_model_version_t v1 = nimcp_version_create(1, 0, 0, layers, 3);
    nimcp_model_version_t v2 = nimcp_version_create(2, 0, 0, layers, 3);

    nimcp_compatibility_result_t result;
    nimcp_version_check_compatibility(&v1, &v2, &result);
    EXPECT_FALSE(result.architecturally_compatible);
}

TEST_F(VersioningTest, SameMajorDifferentMinorIsDeltaCompatible) {
    uint32_t layers[] = {128, 256, 128};
    nimcp_model_version_t v1 = nimcp_version_create(1, 0, 0, layers, 3);
    nimcp_model_version_t v2 = nimcp_version_create(1, 5, 0, layers, 3);

    nimcp_compatibility_result_t result;
    nimcp_version_check_compatibility(&v1, &v2, &result);
    EXPECT_TRUE(result.architecturally_compatible);
    EXPECT_TRUE(result.delta_compatible);
}

TEST_F(VersioningTest, ArchHashSameLayersSameHash) {
    uint32_t layers[] = {128, 256, 512, 256, 128};
    uint32_t h1 = nimcp_version_compute_arch_hash(layers, 5);
    uint32_t h2 = nimcp_version_compute_arch_hash(layers, 5);
    EXPECT_EQ(h1, h2);
}

TEST_F(VersioningTest, ArchHashDifferentLayersDifferentHash) {
    uint32_t layers_a[] = {128, 256, 128};
    uint32_t layers_b[] = {128, 512, 128};
    uint32_t h1 = nimcp_version_compute_arch_hash(layers_a, 3);
    uint32_t h2 = nimcp_version_compute_arch_hash(layers_b, 3);
    EXPECT_NE(h1, h2);
}

TEST_F(VersioningTest, ArchHashOrderMatters) {
    uint32_t layers_a[] = {128, 256, 512};
    uint32_t layers_b[] = {512, 256, 128};
    uint32_t h1 = nimcp_version_compute_arch_hash(layers_a, 3);
    uint32_t h2 = nimcp_version_compute_arch_hash(layers_b, 3);
    EXPECT_NE(h1, h2);
}

TEST_F(VersioningTest, ArchHashDifferentCountsDifferentHash) {
    uint32_t layers_a[] = {128, 256};
    uint32_t layers_b[] = {128, 256, 128};
    uint32_t h1 = nimcp_version_compute_arch_hash(layers_a, 2);
    uint32_t h2 = nimcp_version_compute_arch_hash(layers_b, 3);
    EXPECT_NE(h1, h2);
}

TEST_F(VersioningTest, MigrationPathDeltaForMinorDifference) {
    uint32_t layers[] = {128, 256, 128};
    nimcp_model_version_t v1 = nimcp_version_create(1, 0, 0, layers, 3);
    nimcp_model_version_t v2 = nimcp_version_create(1, 3, 0, layers, 3);

    nimcp_compatibility_result_t result;
    nimcp_version_check_compatibility(&v1, &v2, &result);

    // Delta-compatible versions should suggest "delta" migration
    if (result.delta_compatible) {
        EXPECT_TRUE(
            strstr(result.migration_path, "delta") != nullptr ||
            strstr(result.migration_path, "none") != nullptr)
            << "migration_path: " << result.migration_path;
    }
}

TEST_F(VersioningTest, MigrationPathRedistillForMajorDifference) {
    uint32_t layers_a[] = {128, 256, 128};
    uint32_t layers_b[] = {256, 512, 256};
    nimcp_model_version_t v1 = nimcp_version_create(1, 0, 0, layers_a, 3);
    nimcp_model_version_t v2 = nimcp_version_create(2, 0, 0, layers_b, 3);

    nimcp_compatibility_result_t result;
    nimcp_version_check_compatibility(&v1, &v2, &result);

    EXPECT_TRUE(
        strstr(result.migration_path, "re-distill") != nullptr ||
        strstr(result.migration_path, "redistill") != nullptr)
        << "migration_path: " << result.migration_path;
}

TEST_F(VersioningTest, NullResultHandled) {
    uint32_t layers[] = {128};
    nimcp_model_version_t v = nimcp_version_create(1, 0, 0, layers, 1);
    int ret = nimcp_version_check_compatibility(&v, &v, nullptr);
    // Should return error or handle gracefully (not crash)
    (void)ret;
}

TEST_F(VersioningTest, VersionCreateSetsArchHash) {
    uint32_t layers[] = {100, 200, 100};
    nimcp_model_version_t v = nimcp_version_create(1, 2, 3, layers, 3);
    EXPECT_EQ(v.major, 1u);
    EXPECT_EQ(v.minor, 2u);
    EXPECT_EQ(v.patch, 3u);

    uint32_t expected_hash = nimcp_version_compute_arch_hash(layers, 3);
    EXPECT_EQ(v.arch_hash, expected_hash);
}

TEST_F(VersioningTest, PatchDifferenceIsFullyCompatible) {
    uint32_t layers[] = {128, 256, 128};
    nimcp_model_version_t v1 = nimcp_version_create(1, 2, 0, layers, 3);
    nimcp_model_version_t v2 = nimcp_version_create(1, 2, 5, layers, 3);

    nimcp_compatibility_result_t result;
    nimcp_version_check_compatibility(&v1, &v2, &result);
    EXPECT_TRUE(result.architecturally_compatible);
    EXPECT_TRUE(result.delta_compatible);
}
