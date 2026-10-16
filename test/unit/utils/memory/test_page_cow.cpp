//=============================================================================
// test_page_cow.cpp - Unit Tests for Page-Level COW
//=============================================================================
/**
 * @file test_page_cow.cpp
 * @brief Unit tests for page-level copy-on-write implementation
 *
 * Tests cover:
 * - Region creation and destruction
 * - View creation and cloning
 * - COW triggering on write
 * - Snapshot creation and restoration
 * - Memory savings calculations
 * - Multi-view scenarios
 *
 * @author NIMCP Development Team
 * @date 2025-11-24
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <vector>
#include <atomic>

// Headers have their own extern "C" guards
#include "utils/memory/nimcp_page_cow.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PageCowTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize page COW subsystem
        ASSERT_TRUE(page_cow_init());
    }

    void TearDown() override {
        // Note: Don't shutdown - other tests may need it
        // page_cow_shutdown();
    }

    // Helper to create test data
    std::vector<float> createTestData(size_t num_floats, float start_value = 0.0f) {
        std::vector<float> data(num_floats);
        for (size_t i = 0; i < num_floats; i++) {
            data[i] = start_value + static_cast<float>(i);
        }
        return data;
    }
};

//=============================================================================
// Region Creation Tests
//=============================================================================

TEST_F(PageCowTest, CreateRegion_ValidConfig) {
    page_cow_config_t config = page_cow_default_config(1024 * 1024);  // 1MB
    page_cow_region_t region = page_cow_region_create(&config, nullptr);

    ASSERT_NE(region, nullptr);
    EXPECT_GE(page_cow_region_get_size(region), 1024 * 1024);

    page_cow_region_destroy(region);
}

TEST_F(PageCowTest, CreateRegion_WithInitialData) {
    const size_t size = 64 * 1024;  // 64KB
    std::vector<float> data = createTestData(size / sizeof(float));

    page_cow_config_t config = page_cow_default_config(size);
    page_cow_region_t region = page_cow_region_create(&config, data.data());

    ASSERT_NE(region, nullptr);

    page_cow_region_destroy(region);
}

TEST_F(PageCowTest, CreateRegion_NullConfig) {
    page_cow_region_t region = page_cow_region_create(nullptr, nullptr);
    EXPECT_EQ(region, nullptr);
}

TEST_F(PageCowTest, CreateRegion_ZeroSize) {
    page_cow_config_t config = {0};
    config.size = 0;
    page_cow_region_t region = page_cow_region_create(&config, nullptr);
    EXPECT_EQ(region, nullptr);
}

TEST_F(PageCowTest, CreateRegion_Statistics) {
    const size_t size = 16 * PAGE_COW_PAGE_SIZE;  // 16 pages
    page_cow_config_t config = page_cow_default_config(size);
    page_cow_region_t region = page_cow_region_create(&config, nullptr);

    ASSERT_NE(region, nullptr);

    page_cow_stats_t stats;
    ASSERT_TRUE(page_cow_region_get_stats(region, &stats));

    EXPECT_EQ(stats.total_pages, 16);
    EXPECT_EQ(stats.shared_pages, 16);
    EXPECT_EQ(stats.private_pages, 0);
    EXPECT_EQ(stats.active_views, 0);

    page_cow_region_destroy(region);
}

//=============================================================================
// View Creation Tests
//=============================================================================

TEST_F(PageCowTest, CreateView_FromRegion) {
    const size_t size = 8 * PAGE_COW_PAGE_SIZE;
    page_cow_config_t config = page_cow_default_config(size);
    page_cow_region_t region = page_cow_region_create(&config, nullptr);
    ASSERT_NE(region, nullptr);

    page_cow_view_t view = page_cow_view_create(region);
    ASSERT_NE(view, nullptr);

    EXPECT_EQ(page_cow_view_get_shared_page_count(view), 8);
    EXPECT_EQ(page_cow_view_get_private_page_count(view), 0);

    page_cow_view_destroy(view);
    page_cow_region_destroy(region);
}

TEST_F(PageCowTest, CreateView_MultipleViews) {
    const size_t size = 4 * PAGE_COW_PAGE_SIZE;
    page_cow_config_t config = page_cow_default_config(size);
    page_cow_region_t region = page_cow_region_create(&config, nullptr);
    ASSERT_NE(region, nullptr);

    std::vector<page_cow_view_t> views;
    for (int i = 0; i < 10; i++) {
        page_cow_view_t view = page_cow_view_create(region);
        ASSERT_NE(view, nullptr);
        views.push_back(view);
    }

    page_cow_stats_t stats;
    page_cow_region_get_stats(region, &stats);
    EXPECT_EQ(stats.active_views, 10);

    for (auto view : views) {
        page_cow_view_destroy(view);
    }

    page_cow_region_destroy(region);
}

TEST_F(PageCowTest, CloneView) {
    const size_t size = 4 * PAGE_COW_PAGE_SIZE;
    std::vector<float> data = createTestData(size / sizeof(float), 100.0f);

    page_cow_config_t config = page_cow_default_config(size);
    page_cow_region_t region = page_cow_region_create(&config, data.data());
    ASSERT_NE(region, nullptr);

    page_cow_view_t view1 = page_cow_view_create(region);
    ASSERT_NE(view1, nullptr);

    page_cow_view_t view2 = page_cow_view_clone(view1);
    ASSERT_NE(view2, nullptr);

    // Both should have same data
    const float* data1 = static_cast<const float*>(page_cow_view_read(view1));
    const float* data2 = static_cast<const float*>(page_cow_view_read(view2));

    EXPECT_EQ(data1[0], 100.0f);
    EXPECT_EQ(data2[0], 100.0f);

    page_cow_view_destroy(view2);
    page_cow_view_destroy(view1);
    page_cow_region_destroy(region);
}

//=============================================================================
// Read/Write Tests
//=============================================================================

TEST_F(PageCowTest, ReadView_NoModification) {
    const size_t size = 2 * PAGE_COW_PAGE_SIZE;
    std::vector<float> data = createTestData(size / sizeof(float), 42.0f);

    page_cow_config_t config = page_cow_default_config(size);
    page_cow_region_t region = page_cow_region_create(&config, data.data());
    page_cow_view_t view = page_cow_view_create(region);

    const float* read_data = static_cast<const float*>(page_cow_view_read(view));
    ASSERT_NE(read_data, nullptr);

    EXPECT_FLOAT_EQ(read_data[0], 42.0f);
    EXPECT_FLOAT_EQ(read_data[100], 142.0f);

    // All pages should still be shared
    EXPECT_EQ(page_cow_view_get_shared_page_count(view), 2);
    EXPECT_EQ(page_cow_view_get_private_page_count(view), 0);

    page_cow_view_destroy(view);
    page_cow_region_destroy(region);
}

TEST_F(PageCowTest, WriteView_MakesPagePrivate) {
    const size_t size = 4 * PAGE_COW_PAGE_SIZE;
    std::vector<float> data = createTestData(size / sizeof(float));

    page_cow_config_t config = page_cow_default_config(size);
    page_cow_region_t region = page_cow_region_create(&config, data.data());
    page_cow_view_t view = page_cow_view_create(region);

    // Make page 0 private explicitly
    ASSERT_TRUE(page_cow_view_make_page_private(view, 0));

    EXPECT_EQ(page_cow_view_get_private_page_count(view), 1);
    EXPECT_EQ(page_cow_view_get_shared_page_count(view), 3);

    page_cow_view_destroy(view);
    page_cow_region_destroy(region);
}

TEST_F(PageCowTest, WriteView_VerifyDataIntegrity) {
    const size_t size = 2 * PAGE_COW_PAGE_SIZE;
    std::vector<float> data = createTestData(size / sizeof(float), 0.0f);

    page_cow_config_t config = page_cow_default_config(size);
    page_cow_region_t region = page_cow_region_create(&config, data.data());

    page_cow_view_t view1 = page_cow_view_create(region);
    page_cow_view_t view2 = page_cow_view_create(region);

    // Make view1 page 0 private and modify
    ASSERT_TRUE(page_cow_view_make_page_private(view1, 0));
    float* write_data = static_cast<float*>(page_cow_view_write(view1));
    ASSERT_NE(write_data, nullptr);
    write_data[0] = 999.0f;

    // View2 should still see original data
    const float* read_data2 = static_cast<const float*>(page_cow_view_read(view2));
    EXPECT_FLOAT_EQ(read_data2[0], 0.0f);

    // View1 should see modified data
    const float* read_data1 = static_cast<const float*>(page_cow_view_read(view1));
    EXPECT_FLOAT_EQ(read_data1[0], 999.0f);

    page_cow_view_destroy(view2);
    page_cow_view_destroy(view1);
    page_cow_region_destroy(region);
}

TEST_F(PageCowTest, MakeRangePrivate) {
    const size_t size = 10 * PAGE_COW_PAGE_SIZE;
    page_cow_config_t config = page_cow_default_config(size);
    page_cow_region_t region = page_cow_region_create(&config, nullptr);
    page_cow_view_t view = page_cow_view_create(region);

    // Make pages 2-5 private (4 pages)
    size_t made_private = page_cow_view_make_range_private(view, 2, 4);
    EXPECT_EQ(made_private, 4);

    EXPECT_EQ(page_cow_view_get_private_page_count(view), 4);
    EXPECT_EQ(page_cow_view_get_shared_page_count(view), 6);

    // Verify individual page states
    EXPECT_EQ(page_cow_view_get_page_state(view, 0), PAGE_STATE_SHARED);
    EXPECT_EQ(page_cow_view_get_page_state(view, 1), PAGE_STATE_SHARED);
    EXPECT_EQ(page_cow_view_get_page_state(view, 2), PAGE_STATE_PRIVATE);
    EXPECT_EQ(page_cow_view_get_page_state(view, 3), PAGE_STATE_PRIVATE);
    EXPECT_EQ(page_cow_view_get_page_state(view, 4), PAGE_STATE_PRIVATE);
    EXPECT_EQ(page_cow_view_get_page_state(view, 5), PAGE_STATE_PRIVATE);
    EXPECT_EQ(page_cow_view_get_page_state(view, 6), PAGE_STATE_SHARED);

    page_cow_view_destroy(view);
    page_cow_region_destroy(region);
}

//=============================================================================
// Memory Savings Tests
//=============================================================================

TEST_F(PageCowTest, MemorySavings_AllShared) {
    const size_t size = 100 * PAGE_COW_PAGE_SIZE;  // 400KB
    page_cow_config_t config = page_cow_default_config(size);
    page_cow_region_t region = page_cow_region_create(&config, nullptr);

    // Create 10 views all sharing same pages
    std::vector<page_cow_view_t> views;
    for (int i = 0; i < 10; i++) {
        views.push_back(page_cow_view_create(region));
    }

    // Each view saves ~400KB by sharing
    for (auto view : views) {
        size_t saved = page_cow_view_get_memory_saved(view);
        EXPECT_EQ(saved, 100 * PAGE_COW_PAGE_SIZE);
    }

    for (auto view : views) {
        page_cow_view_destroy(view);
    }
    page_cow_region_destroy(region);
}

TEST_F(PageCowTest, MemorySavings_PartialPrivate) {
    const size_t size = 100 * PAGE_COW_PAGE_SIZE;
    page_cow_config_t config = page_cow_default_config(size);
    page_cow_region_t region = page_cow_region_create(&config, nullptr);
    page_cow_view_t view = page_cow_view_create(region);

    // Make 10 pages private
    page_cow_view_make_range_private(view, 0, 10);

    size_t saved = page_cow_view_get_memory_saved(view);
    EXPECT_EQ(saved, 90 * PAGE_COW_PAGE_SIZE);

    page_cow_view_destroy(view);
    page_cow_region_destroy(region);
}

//=============================================================================
// Snapshot Tests
//=============================================================================

TEST_F(PageCowTest, SnapshotCreate_Instant) {
    const size_t size = 8 * PAGE_COW_PAGE_SIZE;
    page_cow_config_t config = page_cow_default_config(size);
    page_cow_region_t region = page_cow_region_create(&config, nullptr);
    page_cow_view_t view = page_cow_view_create(region);

    // Make some pages private first
    page_cow_view_make_range_private(view, 0, 2);

    // Create snapshot
    page_cow_snapshot_t snap = page_cow_snapshot_create(view);
    ASSERT_NE(snap, nullptr);

    page_cow_snapshot_destroy(snap);
    page_cow_view_destroy(view);
    page_cow_region_destroy(region);
}

TEST_F(PageCowTest, SnapshotRestore) {
    const size_t size = 4 * PAGE_COW_PAGE_SIZE;
    std::vector<float> data = createTestData(size / sizeof(float), 100.0f);

    page_cow_config_t config = page_cow_default_config(size);
    page_cow_region_t region = page_cow_region_create(&config, data.data());
    page_cow_view_t view = page_cow_view_create(region);

    // Create snapshot at initial state
    page_cow_snapshot_t snap = page_cow_snapshot_create(view);
    ASSERT_NE(snap, nullptr);

    // Modify view
    page_cow_view_make_page_private(view, 0);
    float* write_data = static_cast<float*>(page_cow_view_write(view));
    write_data[0] = 999.0f;

    EXPECT_EQ(page_cow_view_get_private_page_count(view), 1);

    // Verify modification
    const float* read_data = static_cast<const float*>(page_cow_view_read(view));
    EXPECT_FLOAT_EQ(read_data[0], 999.0f);

    // Restore from snapshot
    ASSERT_TRUE(page_cow_snapshot_restore(view, snap));

    // Verify restored to original
    read_data = static_cast<const float*>(page_cow_view_read(view));
    EXPECT_FLOAT_EQ(read_data[0], 100.0f);

    page_cow_snapshot_destroy(snap);
    page_cow_view_destroy(view);
    page_cow_region_destroy(region);
}

TEST_F(PageCowTest, SnapshotDeltaPages) {
    const size_t size = 8 * PAGE_COW_PAGE_SIZE;
    page_cow_config_t config = page_cow_default_config(size);
    page_cow_region_t region = page_cow_region_create(&config, nullptr);
    page_cow_view_t view = page_cow_view_create(region);

    // Create snapshot
    page_cow_snapshot_t snap = page_cow_snapshot_create(view);

    // Make 3 pages private
    page_cow_view_make_range_private(view, 2, 3);

    // Check delta
    size_t delta = page_cow_snapshot_get_delta_pages(view, snap);
    EXPECT_EQ(delta, 3);

    page_cow_snapshot_destroy(snap);
    page_cow_view_destroy(view);
    page_cow_region_destroy(region);
}

//=============================================================================
// Helper Function Tests
//=============================================================================

TEST_F(PageCowTest, OffsetToPage) {
    EXPECT_EQ(page_cow_offset_to_page(0), 0);
    EXPECT_EQ(page_cow_offset_to_page(4095), 0);
    EXPECT_EQ(page_cow_offset_to_page(4096), 1);
    EXPECT_EQ(page_cow_offset_to_page(8192), 2);
}

TEST_F(PageCowTest, PageToOffset) {
    EXPECT_EQ(page_cow_page_to_offset(0), 0);
    EXPECT_EQ(page_cow_page_to_offset(1), 4096);
    EXPECT_EQ(page_cow_page_to_offset(10), 40960);
}

TEST_F(PageCowTest, AlignSize) {
    EXPECT_EQ(page_cow_align_size(1), PAGE_COW_PAGE_SIZE);
    EXPECT_EQ(page_cow_align_size(4096), PAGE_COW_PAGE_SIZE);
    EXPECT_EQ(page_cow_align_size(4097), 2 * PAGE_COW_PAGE_SIZE);
    EXPECT_EQ(page_cow_align_size(8192), 2 * PAGE_COW_PAGE_SIZE);
}

TEST_F(PageCowTest, NumPages) {
    EXPECT_EQ(page_cow_num_pages(1), 1);
    EXPECT_EQ(page_cow_num_pages(4096), 1);
    EXPECT_EQ(page_cow_num_pages(4097), 2);
    EXPECT_EQ(page_cow_num_pages(8192), 2);
    EXPECT_EQ(page_cow_num_pages(1024 * 1024), 256);  // 1MB = 256 pages
}

//=============================================================================
// Large Data Tests (50MB weights simulation)
//=============================================================================

TEST_F(PageCowTest, LargeData_50MB_MemorySavings) {
    const size_t size = 50 * 1024 * 1024;  // 50MB
    page_cow_config_t config = page_cow_default_config(size);
    config.zero_on_allocate = true;

    page_cow_region_t region = page_cow_region_create(&config, nullptr);
    ASSERT_NE(region, nullptr);

    // Create 5 views (simulating 5 replicas)
    std::vector<page_cow_view_t> views;
    for (int i = 0; i < 5; i++) {
        page_cow_view_t view = page_cow_view_create(region);
        ASSERT_NE(view, nullptr);
        views.push_back(view);
    }

    // Each view saves 50MB
    size_t expected_pages = page_cow_num_pages(size);
    for (auto view : views) {
        EXPECT_EQ(page_cow_view_get_shared_page_count(view), expected_pages);
        EXPECT_EQ(page_cow_view_get_memory_saved(view), size);
    }

    // Simulate: View 0 modifies 1% of pages (fine-tuning)
    size_t modified_pages = expected_pages / 100;
    page_cow_view_make_range_private(views[0], 0, modified_pages);

    // View 0 now has 99% savings instead of 100%
    size_t saved = page_cow_view_get_memory_saved(views[0]);
    size_t expected_saved = (expected_pages - modified_pages) * PAGE_COW_PAGE_SIZE;
    EXPECT_EQ(saved, expected_saved);

    // Other views still have 100% savings
    for (size_t i = 1; i < views.size(); i++) {
        EXPECT_EQ(page_cow_view_get_memory_saved(views[i]), size);
    }

    for (auto view : views) {
        page_cow_view_destroy(view);
    }
    page_cow_region_destroy(region);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(PageCowTest, NullHandling) {
    // Void functions - just call them to verify no crash
    page_cow_region_destroy(nullptr);  // Should not crash
    page_cow_view_destroy(nullptr);    // Should not crash

    // Pointer-returning functions
    EXPECT_EQ(page_cow_view_read(nullptr), nullptr);
    EXPECT_EQ(page_cow_view_write(nullptr), nullptr);
    EXPECT_FALSE(page_cow_view_make_page_private(nullptr, 0));
    EXPECT_EQ(page_cow_view_get_private_page_count(nullptr), 0);
    EXPECT_EQ(page_cow_view_get_shared_page_count(nullptr), 0);
    EXPECT_EQ(page_cow_snapshot_create(nullptr), nullptr);
}

TEST_F(PageCowTest, InvalidPageIndex) {
    const size_t size = 4 * PAGE_COW_PAGE_SIZE;
    page_cow_config_t config = page_cow_default_config(size);
    page_cow_region_t region = page_cow_region_create(&config, nullptr);
    page_cow_view_t view = page_cow_view_create(region);

    // Try to make page 100 private (only 4 pages exist)
    EXPECT_FALSE(page_cow_view_make_page_private(view, 100));

    // Invalid page state
    EXPECT_EQ(page_cow_view_get_page_state(view, 100), PAGE_STATE_UNMAPPED);

    page_cow_view_destroy(view);
    page_cow_region_destroy(region);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
