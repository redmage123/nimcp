/**
 * @file test_kdtree_range_search.cpp
 * @brief Comprehensive unit tests for KD-tree range search
 *
 * WHAT: Test suite for kdtree_range_search function
 * WHY:  Ensure 100% code coverage and correctness of range queries
 * HOW:  Test edge cases, performance, and correctness
 *
 * @author NIMCP Test Team
 * @date 2025-01-17
 */

#include <gtest/gtest.h>
#include "utils/spatial/nimcp_kdtree.h"
#include "utils/nimcp_test_base.h"
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <random>

//=============================================================================
// Test Fixtures
//=============================================================================

class KDTreeRangeSearchTest : public NimcpTestBase {
protected:
    kdtree_t* tree;
    std::vector<std::array<float, 3>> points;
    std::vector<void*> user_data;

    void SetUp() override {
        NimcpTestBase::SetUp();  // Call parent SetUp first

        tree = kdtree_create();
        ASSERT_NE(tree, nullptr);
    }

    void TearDown() override {
        if (tree) {
            kdtree_destroy(tree);
        }

        NimcpTestBase::TearDown();  // Call parent TearDown last
    }

    // Helper: Build tree with test points
    void BuildTreeWithPoints(const std::vector<std::array<float, 3>>& pts) {
        points = pts;
        user_data.resize(pts.size());

        // Use point indices as user data
        for (size_t i = 0; i < pts.size(); i++) {
            user_data[i] = (void*)(uintptr_t)i;
        }

        bool success = kdtree_build(tree, reinterpret_cast<kdtree_point_t*>(points.data()), user_data.data(),
                                   static_cast<uint32_t>(points.size()));
        ASSERT_TRUE(success);
    }

    // Helper: Compute Euclidean distance
    static float Distance(const std::array<float, 3>& a, const std::array<float, 3>& b) {
        float dx = a[0] - b[0];
        float dy = a[1] - b[1];
        float dz = a[2] - b[2];
        return std::sqrt(dx*dx + dy*dy + dz*dz);
    }

    // Helper: Brute force range search for validation
    std::vector<uint32_t> BruteForceRangeSearch(const std::array<float, 3> query,
                                                 float radius) {
        std::vector<uint32_t> results;
        for (size_t i = 0; i < points.size(); i++) {
            if (Distance(points[i], query) <= radius) {
                results.push_back(static_cast<uint32_t>(i));
            }
        }
        return results;
    }
};

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(KDTreeRangeSearchTest, NullTree) {
    // WHAT: Test with NULL tree
    // WHY:  Should handle gracefully without crashing
    // HOW:  Expect 0 results

    std::array<float, 3> query{{0.0f, 0.0f, 0.0f}};
    void* results[10];

    uint32_t count = kdtree_range_search(nullptr, query.data(), 1.0f, results, 10);
    EXPECT_EQ(count, 0);
}

TEST_F(KDTreeRangeSearchTest, NullResults) {
    // WHAT: Test with NULL results array
    // WHY:  Should handle gracefully
    // HOW:  Expect 0 results

    std::vector<std::array<float, 3>> pts = {
        std::array<float, 3>{{0.0f, 0.0f, 0.0f}}
    };
    BuildTreeWithPoints(pts);

    std::array<float, 3> query{{0.0f, 0.0f, 0.0f}};
    uint32_t count = kdtree_range_search(tree, query.data(), 1.0f, nullptr, 10);
    EXPECT_EQ(count, 0);
}

TEST_F(KDTreeRangeSearchTest, ZeroCapacity) {
    // WHAT: Test with zero capacity
    // WHY:  Should return 0 results
    // HOW:  Valid tree but capacity=0

    std::vector<std::array<float, 3>> pts = {
        std::array<float, 3>{{0.0f, 0.0f, 0.0f}}
    };
    BuildTreeWithPoints(pts);

    std::array<float, 3> query{{0.0f, 0.0f, 0.0f}};
    void* results[10];
    uint32_t count = kdtree_range_search(tree, query.data(), 1.0f, results, 0);
    EXPECT_EQ(count, 0);
}

TEST_F(KDTreeRangeSearchTest, NegativeRadius) {
    // WHAT: Test with negative radius
    // WHY:  Should handle gracefully (treat as invalid)
    // HOW:  Expect 0 results

    std::vector<std::array<float, 3>> pts = {
        std::array<float, 3>{{0.0f, 0.0f, 0.0f}}
    };
    BuildTreeWithPoints(pts);

    std::array<float, 3> query{{0.0f, 0.0f, 0.0f}};
    void* results[10];
    uint32_t count = kdtree_range_search(tree, query.data(), -1.0f, results, 10);
    EXPECT_EQ(count, 0);
}

TEST_F(KDTreeRangeSearchTest, ZeroRadius) {
    // WHAT: Test with zero radius
    // WHY:  Should only find exact matches
    // HOW:  Point at query location should be found

    std::vector<std::array<float, 3>> pts = {
        std::array<float, 3>{{0.0f, 0.0f, 0.0f}},
        std::array<float, 3>{{1.0f, 0.0f, 0.0f}},
        std::array<float, 3>{{0.0f, 1.0f, 0.0f}}
    };
    BuildTreeWithPoints(pts);

    std::array<float, 3> query{{0.0f, 0.0f, 0.0f}};
    void* results[10];
    uint32_t count = kdtree_range_search(tree, query.data(), 0.0f, results, 10);

    // Should find only the exact match
    EXPECT_EQ(count, 1);
    EXPECT_EQ((uintptr_t)results[0], 0);
}

//=============================================================================
// Correctness Tests
//=============================================================================

TEST_F(KDTreeRangeSearchTest, SinglePoint_Inside) {
    // WHAT: Single point inside radius
    // WHY:  Basic correctness test
    // HOW:  Point at origin, query at origin, radius > 0

    std::vector<std::array<float, 3>> pts = {
        std::array<float, 3>{{0.0f, 0.0f, 0.0f}}
    };
    BuildTreeWithPoints(pts);

    std::array<float, 3> query{{0.0f, 0.0f, 0.0f}};
    void* results[10];
    uint32_t count = kdtree_range_search(tree, query.data(), 1.0f, results, 10);

    EXPECT_EQ(count, 1);
    EXPECT_EQ((uintptr_t)results[0], 0);
}

TEST_F(KDTreeRangeSearchTest, SinglePoint_Outside) {
    // WHAT: Single point outside radius
    // WHY:  Ensure points beyond radius are excluded
    // HOW:  Point at (10,0,0), query at origin, radius 5

    std::vector<std::array<float, 3>> pts = {
        std::array<float, 3>{{10.0f, 0.0f, 0.0f}}
    };
    BuildTreeWithPoints(pts);

    std::array<float, 3> query{{0.0f, 0.0f, 0.0f}};
    void* results[10];
    uint32_t count = kdtree_range_search(tree, query.data(), 5.0f, results, 10);

    EXPECT_EQ(count, 0);
}

TEST_F(KDTreeRangeSearchTest, MultiplePoints_Grid) {
    // WHAT: Grid of points with known distances
    // WHY:  Test with structured data
    // HOW:  3x3x3 grid, query at center

    std::vector<std::array<float, 3>> pts;
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            for (int z = -1; z <= 1; z++) {
                pts.push_back(std::array<float, 3>{{(float)x, (float)y, (float)z}});
            }
        }
    }
    BuildTreeWithPoints(pts);

    std::array<float, 3> query{{0.0f, 0.0f, 0.0f}};
    void* results[100];

    // Radius 0.5 should find center point only (face neighbors at distance 1.0)
    uint32_t count = kdtree_range_search(tree, query.data(), 0.5f, results, 100);
    EXPECT_EQ(count, 1);

    // Radius 1.1 should find center + 6 face neighbors (edges at distance √2 ≈ 1.414)
    count = kdtree_range_search(tree, query.data(), 1.1f, results, 100);
    EXPECT_EQ(count, 7);

    // Radius 1.5 should find center + 6 faces + 12 edges (corners at distance √3 ≈ 1.732)
    count = kdtree_range_search(tree, query.data(), 1.5f, results, 100);
    EXPECT_EQ(count, 19);

    // Radius 2.0 should find all 27 points
    count = kdtree_range_search(tree, query.data(), 2.0f, results, 100);
    EXPECT_EQ(count, 27);
}

TEST_F(KDTreeRangeSearchTest, CapacityLimit) {
    // WHAT: Test capacity limiting
    // WHY:  Ensure function respects capacity parameter
    // HOW:  Many points in range, but small capacity

    std::vector<std::array<float, 3>> pts;
    for (int i = 0; i < 100; i++) {
        pts.push_back(std::array<float, 3>{{0.0f, 0.0f, (float)i * 0.01f}});
    }
    BuildTreeWithPoints(pts);

    std::array<float, 3> query{{0.0f, 0.0f, 0.5f}};
    void* results[10];

    // Radius 10.0 would find all 100 points, but capacity is only 10
    uint32_t count = kdtree_range_search(tree, query.data(), 10.0f, results, 10);
    EXPECT_EQ(count, 10);
}

//=============================================================================
// Accuracy Tests (vs Brute Force)
//=============================================================================

TEST_F(KDTreeRangeSearchTest, RandomPoints_SmallRadius) {
    // WHAT: Random points, verify against brute force
    // WHY:  Ensure algorithmic correctness
    // HOW:  Generate random points, compare results

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-10.0f, 10.0f);

    std::vector<std::array<float, 3>> pts;
    for (int i = 0; i < 100; i++) {
        pts.push_back(std::array<float, 3>{{dist(rng), dist(rng), dist(rng)}});
    }
    BuildTreeWithPoints(pts);

    std::array<float, 3> query{{0.0f, 0.0f, 0.0f}};
    void* results[100];

    float radius = 3.0f;
    uint32_t count = kdtree_range_search(tree, query.data(), radius, results, 100);

    // Verify against brute force
    std::vector<uint32_t> expected = BruteForceRangeSearch(query, radius);
    EXPECT_EQ(count, expected.size());

    // Convert results to indices
    std::vector<uint32_t> found;
    for (uint32_t i = 0; i < count; i++) {
        found.push_back((uintptr_t)results[i]);
    }

    // Sort both for comparison
    std::sort(found.begin(), found.end());
    std::sort(expected.begin(), expected.end());

    EXPECT_EQ(found, expected);
}

TEST_F(KDTreeRangeSearchTest, RandomPoints_LargeRadius) {
    // WHAT: Large radius that captures many points
    // WHY:  Test performance with large result sets
    // HOW:  Radius that captures ~50% of points

    std::mt19937 rng(123);
    std::uniform_real_distribution<float> dist(-10.0f, 10.0f);

    std::vector<std::array<float, 3>> pts;
    for (int i = 0; i < 200; i++) {
        pts.push_back(std::array<float, 3>{{dist(rng), dist(rng), dist(rng)}});
    }
    BuildTreeWithPoints(pts);

    std::array<float, 3> query{{0.0f, 0.0f, 0.0f}};
    void* results[200];

    float radius = 10.0f;
    uint32_t count = kdtree_range_search(tree, query.data(), radius, results, 200);

    // Verify against brute force
    std::vector<uint32_t> expected = BruteForceRangeSearch(query, radius);
    EXPECT_EQ(count, expected.size());
}

//=============================================================================
// Boundary Tests
//=============================================================================

TEST_F(KDTreeRangeSearchTest, PointExactlyOnBoundary) {
    // WHAT: Point exactly at radius distance
    // WHY:  Test boundary inclusion (should be included with <=)
    // HOW:  Point at distance exactly equal to radius

    std::vector<std::array<float, 3>> pts = {
        std::array<float, 3>{{0.0f, 0.0f, 0.0f}},
        std::array<float, 3>{{1.0f, 0.0f, 0.0f}},  // Distance = 1.0
        std::array<float, 3>{{0.0f, 1.0f, 0.0f}},  // Distance = 1.0
        std::array<float, 3>{{0.0f, 0.0f, 1.0f}}   // Distance = 1.0
    };
    BuildTreeWithPoints(pts);

    std::array<float, 3> query{{0.0f, 0.0f, 0.0f}};
    void* results[10];

    // Radius exactly 1.0 - should include boundary points
    uint32_t count = kdtree_range_search(tree, query.data(), 1.0f, results, 10);
    EXPECT_EQ(count, 4);  // All 4 points
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(KDTreeRangeSearchTest, Performance_1000Points) {
    // WHAT: Performance with 1000 points
    // WHY:  Ensure reasonable performance
    // HOW:  Time 100 queries, expect < 1ms average

    std::mt19937 rng(456);
    std::uniform_real_distribution<float> dist(-100.0f, 100.0f);

    std::vector<std::array<float, 3>> pts;
    for (int i = 0; i < 1000; i++) {
        pts.push_back(std::array<float, 3>{{dist(rng), dist(rng), dist(rng)}});
    }
    BuildTreeWithPoints(pts);

    void* results[100];
    std::array<float, 3> query{{0.0f, 0.0f, 0.0f}};

    // Warm-up
    kdtree_range_search(tree, query.data(), 10.0f, results, 100);

    // Timed queries
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; i++) {
        kdtree_range_search(tree, query.data(), 10.0f, results, 100);
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avg_us = duration.count() / 100.0;

    // Should be much faster than brute force O(N)
    EXPECT_LT(avg_us, 1000.0);  // < 1ms per query

    // Print for visibility
    std::cout << "Average range search time: " << avg_us << " us" << std::endl;
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(KDTreeRangeSearchTest, EmptyResults) {
    // WHAT: Query that returns no results
    // WHY:  Test early exit paths
    // HOW:  Query far from all points

    std::vector<std::array<float, 3>> pts = {
        std::array<float, 3>{{0.0f, 0.0f, 0.0f}},
        std::array<float, 3>{{1.0f, 1.0f, 1.0f}}
    };
    BuildTreeWithPoints(pts);

    std::array<float, 3> query{{100.0f, 100.0f, 100.0f}};
    void* results[10];

    uint32_t count = kdtree_range_search(tree, query.data(), 1.0f, results, 10);
    EXPECT_EQ(count, 0);
}

TEST_F(KDTreeRangeSearchTest, AllPointsInRange) {
    // WHAT: Radius that captures all points
    // WHY:  Test maximum result case
    // HOW:  Very large radius

    std::vector<std::array<float, 3>> pts;
    for (int i = 0; i < 50; i++) {
        pts.push_back(std::array<float, 3>{{(float)i, (float)i, (float)i}});
    }
    BuildTreeWithPoints(pts);

    std::array<float, 3> query{{25.0f, 25.0f, 25.0f}};
    void* results[50];

    uint32_t count = kdtree_range_search(tree, query.data(), 1000.0f, results, 50);
    EXPECT_EQ(count, 50);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
