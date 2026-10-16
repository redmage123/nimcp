/**
 * @file test_kdtree.cpp
 * @brief Comprehensive unit tests for KD-tree spatial indexing
 *
 * TEST COVERAGE:
 * - Unit tests: Creation, building, nearest neighbor, destruction
 * - Integration tests: Large datasets, performance benchmarks
 * - Regression tests: Edge cases, error handling, memory leaks
 */

#include <gtest/gtest.h>
#include "utils/spatial/nimcp_kdtree.h"
#include "utils/nimcp_test_base.h"
#include <vector>
#include <random>
#include <cmath>

//=============================================================================
// Test Fixture
//=============================================================================

class KDTreeTest : public NimcpTestBase {
protected:
    kdtree_t* tree;

    void SetUp() override {
        NimcpTestBase::SetUp();  // Call parent SetUp first for global state cleanup

        tree = kdtree_create();
        ASSERT_NE(tree, nullptr);
    }

    void TearDown() override {
        if (tree) {
            kdtree_destroy(tree);
            tree = nullptr;
        }

        NimcpTestBase::TearDown();  // Call parent TearDown last for global state cleanup
    }

    // Helper: Calculate distance squared
    float distance_sq(const float a[3], const float b[3]) {
        float dx = a[0] - b[0];
        float dy = a[1] - b[1];
        float dz = a[2] - b[2];
        return dx*dx + dy*dy + dz*dz;
    }
};

//=============================================================================
// Unit Tests - Creation & Destruction
//=============================================================================

TEST_F(KDTreeTest, CreateDestroy) {
    // Tree already created in SetUp
    EXPECT_EQ(kdtree_size(tree), 0);
    EXPECT_EQ(kdtree_depth(tree), 0);
}

TEST_F(KDTreeTest, CreateNull) {
    kdtree_destroy(nullptr); // Should not crash
}

//=============================================================================
// Unit Tests - Building
//=============================================================================

TEST_F(KDTreeTest, BuildEmpty) {
    kdtree_point_t points[1] = {{0, 0, 0}};
    void* data[1] = {nullptr};

    bool success = kdtree_build(tree, points, data, 0);
    EXPECT_FALSE(success); // 0 count should fail
}

TEST_F(KDTreeTest, BuildSinglePoint) {
    kdtree_point_t points[1] = {{1.0f, 2.0f, 3.0f}};
    int user_data = 42;
    void* data[1] = {&user_data};

    bool success = kdtree_build(tree, points, data, 1);
    EXPECT_TRUE(success);
    EXPECT_EQ(kdtree_size(tree), 1);
    EXPECT_EQ(kdtree_depth(tree), 1);
}

TEST_F(KDTreeTest, BuildMultiplePoints) {
    const int N = 100;
    std::vector<kdtree_point_t> points(N);
    std::vector<void*> data(N);
    std::vector<int> user_data(N);

    // Create random points
    std::mt19937 rng(12345);
    std::uniform_real_distribution<float> dist(-100.0f, 100.0f);

    for (int i = 0; i < N; i++) {
        points[i][0] = dist(rng);
        points[i][1] = dist(rng);
        points[i][2] = dist(rng);
        user_data[i] = i;
        data[i] = &user_data[i];
    }

    bool success = kdtree_build(tree, points.data(), data.data(), N);
    EXPECT_TRUE(success);
    EXPECT_EQ(kdtree_size(tree), N);
    EXPECT_GT(kdtree_depth(tree), 0);
    EXPECT_LE(kdtree_depth(tree), N); // Depth should be reasonable
}

TEST_F(KDTreeTest, BuildNullInputs) {
    kdtree_point_t points[1] = {{0, 0, 0}};
    void* data[1] = {nullptr};

    // Null tree
    EXPECT_FALSE(kdtree_build(nullptr, points, data, 1));

    // Null points
    EXPECT_FALSE(kdtree_build(tree, nullptr, data, 1));
}

TEST_F(KDTreeTest, RebuildTree) {
    kdtree_point_t points1[3] = {{0, 0, 0}, {1, 1, 1}, {2, 2, 2}};
    void* data1[3] = {(void*)1, (void*)2, (void*)3};

    kdtree_build(tree, points1, data1, 3);
    EXPECT_EQ(kdtree_size(tree), 3);

    // Clear and rebuild
    kdtree_clear(tree);
    EXPECT_EQ(kdtree_size(tree), 0);

    kdtree_point_t points2[5] = {{0, 0, 0}, {1, 1, 1}, {2, 2, 2}, {3, 3, 3}, {4, 4, 4}};
    void* data2[5] = {(void*)1, (void*)2, (void*)3, (void*)4, (void*)5};

    kdtree_build(tree, points2, data2, 5);
    EXPECT_EQ(kdtree_size(tree), 5);
}

//=============================================================================
// Unit Tests - Nearest Neighbor Search
//=============================================================================

TEST_F(KDTreeTest, NearestSinglePoint) {
    kdtree_point_t points[1] = {{5.0f, 10.0f, 15.0f}};
    int user_data = 100;
    void* data[1] = {&user_data};

    kdtree_build(tree, points, data, 1);

    kdtree_point_t query = {5.0f, 10.0f, 15.0f};
    float dist_sq;
    void* result = kdtree_nearest(tree, query, &dist_sq);

    EXPECT_EQ(result, &user_data);
    EXPECT_FLOAT_EQ(dist_sq, 0.0f); // Exact match
}

TEST_F(KDTreeTest, NearestMultiplePoints) {
    kdtree_point_t points[4] = {
        {0.0f, 0.0f, 0.0f},
        {10.0f, 0.0f, 0.0f},
        {0.0f, 10.0f, 0.0f},
        {0.0f, 0.0f, 10.0f}
    };
    int user_data[4] = {0, 1, 2, 3};
    void* data[4] = {&user_data[0], &user_data[1], &user_data[2], &user_data[3]};

    kdtree_build(tree, points, data, 4);

    // Query near point 0
    kdtree_point_t query1 = {1.0f, 1.0f, 1.0f};
    int* result1 = (int*)kdtree_nearest(tree, query1, nullptr);
    EXPECT_EQ(*result1, 0);

    // Query near point 1
    kdtree_point_t query2 = {9.0f, 1.0f, 1.0f};
    int* result2 = (int*)kdtree_nearest(tree, query2, nullptr);
    EXPECT_EQ(*result2, 1);

    // Query near point 2
    kdtree_point_t query3 = {1.0f, 9.0f, 1.0f};
    int* result3 = (int*)kdtree_nearest(tree, query3, nullptr);
    EXPECT_EQ(*result3, 2);

    // Query near point 3
    kdtree_point_t query4 = {1.0f, 1.0f, 9.0f};
    int* result4 = (int*)kdtree_nearest(tree, query4, nullptr);
    EXPECT_EQ(*result4, 3);
}

TEST_F(KDTreeTest, NearestEmptyTree) {
    kdtree_point_t query = {0, 0, 0};
    void* result = kdtree_nearest(tree, query, nullptr);
    EXPECT_EQ(result, nullptr);
}

TEST_F(KDTreeTest, NearestNullTree) {
    kdtree_point_t query = {0, 0, 0};
    void* result = kdtree_nearest(nullptr, query, nullptr);
    EXPECT_EQ(result, nullptr);
}

TEST_F(KDTreeTest, NearestDistanceCheck) {
    kdtree_point_t points[2] = {{0, 0, 0}, {10, 0, 0}};
    int user_data[2] = {0, 1};
    void* data[2] = {&user_data[0], &user_data[1]};

    kdtree_build(tree, points, data, 2);

    kdtree_point_t query = {3, 0, 0};
    float dist_sq;
    int* result = (int*)kdtree_nearest(tree, query, &dist_sq);

    EXPECT_EQ(*result, 0); // Closer to point 0
    EXPECT_FLOAT_EQ(dist_sq, 9.0f); // Distance squared: 3^2
}

//=============================================================================
// Integration Tests - Large Datasets
//=============================================================================

TEST_F(KDTreeTest, LargeDataset_1000Points) {
    const int N = 1000;
    std::vector<kdtree_point_t> points(N);
    std::vector<void*> data(N);
    std::vector<int> user_data(N);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1000.0f, 1000.0f);

    for (int i = 0; i < N; i++) {
        points[i][0] = dist(rng);
        points[i][1] = dist(rng);
        points[i][2] = dist(rng);
        user_data[i] = i;
        data[i] = &user_data[i];
    }

    bool success = kdtree_build(tree, points.data(), data.data(), N);
    ASSERT_TRUE(success);

    // Test multiple nearest neighbor queries
    for (int i = 0; i < 10; i++) {
        kdtree_point_t query = {dist(rng), dist(rng), dist(rng)};

        // KD-tree result
        float kdtree_dist_sq;
        void* kdtree_result = kdtree_nearest(tree, query, &kdtree_dist_sq);
        ASSERT_NE(kdtree_result, nullptr);

        // Verify with brute force
        float min_dist_sq = INFINITY;
        int nearest_idx = -1;
        for (int j = 0; j < N; j++) {
            float d = distance_sq(query, points[j]);
            if (d < min_dist_sq) {
                min_dist_sq = d;
                nearest_idx = j;
            }
        }

        int* kdtree_idx = (int*)kdtree_result;
        EXPECT_EQ(*kdtree_idx, nearest_idx);
        EXPECT_FLOAT_EQ(kdtree_dist_sq, min_dist_sq);
    }
}

TEST_F(KDTreeTest, PerformanceBenchmark) {
    const int N = 10000;
    std::vector<kdtree_point_t> points(N);
    std::vector<void*> data(N);
    std::vector<int> user_data(N);

    std::mt19937 rng(99);
    std::uniform_real_distribution<float> dist(-10000.0f, 10000.0f);

    for (int i = 0; i < N; i++) {
        points[i][0] = dist(rng);
        points[i][1] = dist(rng);
        points[i][2] = dist(rng);
        user_data[i] = i;
        data[i] = &user_data[i];
    }

    bool success = kdtree_build(tree, points.data(), data.data(), N);
    ASSERT_TRUE(success);

    // Depth should be O(log N)
    uint32_t depth = kdtree_depth(tree);
    uint32_t expected_depth = (uint32_t)(log2(N)) + 5; // Allow some slack
    EXPECT_LE(depth, expected_depth);
}

//=============================================================================
// Regression Tests - Edge Cases
//=============================================================================

TEST_F(KDTreeTest, IdenticalPoints) {
    kdtree_point_t points[5] = {
        {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}
    };
    int user_data[5] = {0, 1, 2, 3, 4};
    void* data[5] = {&user_data[0], &user_data[1], &user_data[2],
                     &user_data[3], &user_data[4]};

    bool success = kdtree_build(tree, points, data, 5);
    EXPECT_TRUE(success);

    kdtree_point_t query = {1, 1, 1};
    void* result = kdtree_nearest(tree, query, nullptr);
    EXPECT_NE(result, nullptr); // Should find one of them
}

TEST_F(KDTreeTest, CollinearPoints) {
    kdtree_point_t points[5] = {
        {0, 0, 0}, {1, 0, 0}, {2, 0, 0}, {3, 0, 0}, {4, 0, 0}
    };
    int user_data[5] = {0, 1, 2, 3, 4};
    void* data[5] = {&user_data[0], &user_data[1], &user_data[2],
                     &user_data[3], &user_data[4]};

    bool success = kdtree_build(tree, points, data, 5);
    EXPECT_TRUE(success);

    kdtree_point_t query = {2.1f, 0, 0};
    int* result = (int*)kdtree_nearest(tree, query, nullptr);
    EXPECT_EQ(*result, 2);
}

TEST_F(KDTreeTest, NegativeCoordinates) {
    kdtree_point_t points[4] = {
        {-10, -10, -10}, {-5, -5, -5}, {5, 5, 5}, {10, 10, 10}
    };
    int user_data[4] = {0, 1, 2, 3};
    void* data[4] = {&user_data[0], &user_data[1], &user_data[2], &user_data[3]};

    bool success = kdtree_build(tree, points, data, 4);
    EXPECT_TRUE(success);

    kdtree_point_t query = {-6, -6, -6};
    int* result = (int*)kdtree_nearest(tree, query, nullptr);
    EXPECT_EQ(*result, 1); // Closest to {-5, -5, -5}
}

TEST_F(KDTreeTest, VeryLargeCoordinates) {
    kdtree_point_t points[2] = {
        {1e6f, 1e6f, 1e6f}, {2e6f, 2e6f, 2e6f}
    };
    int user_data[2] = {0, 1};
    void* data[2] = {&user_data[0], &user_data[1]};

    bool success = kdtree_build(tree, points, data, 2);
    EXPECT_TRUE(success);

    kdtree_point_t query = {1.5e6f, 1.5e6f, 1.5e6f};
    void* result = kdtree_nearest(tree, query, nullptr);
    EXPECT_NE(result, nullptr);
}

//=============================================================================
// Regression Tests - Memory Safety
//=============================================================================

TEST_F(KDTreeTest, NoMemoryLeaks) {
    // Build and rebuild multiple times
    for (int iter = 0; iter < 10; iter++) {
        const int N = 100;
        std::vector<kdtree_point_t> points(N);
        std::vector<void*> data(N);
        std::vector<int> user_data(N);

        for (int i = 0; i < N; i++) {
            points[i][0] = (float)i;
            points[i][1] = (float)i;
            points[i][2] = (float)i;
            user_data[i] = i;
            data[i] = &user_data[i];
        }

        kdtree_build(tree, points.data(), data.data(), N);
        kdtree_clear(tree);
    }
    // If no leaks, this should complete without issues
    SUCCEED();
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
