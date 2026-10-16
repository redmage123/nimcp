//=============================================================================
// test_page_cow_stress.cpp - Thread Safety Stress Tests for Page COW
//=============================================================================
/**
 * @file test_page_cow_stress.cpp
 * @brief Phase 5 thread safety stress testing
 *
 * WHAT: Stress tests for concurrent page COW operations
 * WHY:  Verify thread safety under heavy load
 * HOW:  Multiple threads performing concurrent operations
 *
 * @author NIMCP Development Team
 * @date 2025-11-24
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>

// Headers have their own extern "C" guards
#include "utils/memory/nimcp_page_cow.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PageCowStressTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_TRUE(page_cow_init());
    }

    void TearDown() override {
        // Don't shutdown - other tests may need it
    }

    static constexpr size_t REGION_SIZE = 4 * 1024 * 1024;  // 4MB
    static constexpr int NUM_THREADS = 8;
    static constexpr int OPS_PER_THREAD = 500;
};

//=============================================================================
// Stress Test 1: Concurrent View Creation/Destruction
//=============================================================================

TEST_F(PageCowStressTest, ConcurrentViewCreateDestroy) {
    page_cow_config_t config = page_cow_default_config(REGION_SIZE);
    page_cow_region_t region = page_cow_region_create(&config, nullptr);
    ASSERT_NE(region, nullptr);

    std::atomic<int> success_count{0};
    std::atomic<int> error_count{0};

    auto worker = [&](int thread_id) {
        for (int i = 0; i < OPS_PER_THREAD; i++) {
            page_cow_view_t view = page_cow_view_create(region);
            if (view) {
                // Brief read
                const void* data = page_cow_view_read(view);
                EXPECT_NE(data, nullptr);

                page_cow_view_destroy(view);
                success_count++;
            } else {
                error_count++;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(error_count.load(), 0);
    EXPECT_EQ(success_count.load(), NUM_THREADS * OPS_PER_THREAD);

    page_cow_region_destroy(region);
}

//=============================================================================
// Stress Test 2: Concurrent COW Triggers
//=============================================================================

TEST_F(PageCowStressTest, ConcurrentCowTriggers) {
    page_cow_config_t config = page_cow_default_config(REGION_SIZE);
    page_cow_region_t region = page_cow_region_create(&config, nullptr);
    ASSERT_NE(region, nullptr);

    std::atomic<size_t> total_private_pages{0};

    auto worker = [&](int thread_id) {
        page_cow_view_t view = page_cow_view_create(region);
        ASSERT_NE(view, nullptr);

        // Each thread makes different pages private
        size_t start_page = thread_id * 100;
        size_t pages_made = page_cow_view_make_range_private(view, start_page, 50);

        total_private_pages += pages_made;

        // Verify we can write to our private pages
        void* write_ptr = page_cow_view_write(view);
        ASSERT_NE(write_ptr, nullptr);

        float* data = static_cast<float*>(write_ptr);
        size_t offset = start_page * PAGE_COW_PAGE_SIZE / sizeof(float);
        for (size_t i = 0; i < 100; i++) {
            data[offset + i] = static_cast<float>(thread_id * 1000 + i);
        }

        // Verify data integrity
        const float* read_ptr = static_cast<const float*>(page_cow_view_read(view));
        for (size_t i = 0; i < 100; i++) {
            EXPECT_FLOAT_EQ(read_ptr[offset + i], static_cast<float>(thread_id * 1000 + i));
        }

        page_cow_view_destroy(view);
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    // Each thread should have made 50 pages private
    EXPECT_EQ(total_private_pages.load(), NUM_THREADS * 50);

    page_cow_region_destroy(region);
}

//=============================================================================
// Stress Test 3: Concurrent Snapshots
//=============================================================================

TEST_F(PageCowStressTest, ConcurrentSnapshots) {
    page_cow_config_t config = page_cow_default_config(REGION_SIZE);
    page_cow_region_t region = page_cow_region_create(&config, nullptr);
    ASSERT_NE(region, nullptr);

    page_cow_view_t view = page_cow_view_create(region);
    ASSERT_NE(view, nullptr);

    std::atomic<int> snapshot_success{0};
    std::atomic<int> restore_success{0};

    auto worker = [&](int thread_id) {
        for (int i = 0; i < 50; i++) {
            // Create snapshot
            page_cow_snapshot_t snap = page_cow_snapshot_create(view);
            if (snap) {
                snapshot_success++;

                // Try restore (may fail due to concurrent modification)
                if (page_cow_snapshot_restore(view, snap)) {
                    restore_success++;
                }

                page_cow_snapshot_destroy(snap);
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) {  // Fewer threads for snapshot test
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    // Most snapshots should succeed
    EXPECT_GT(snapshot_success.load(), 150);

    page_cow_view_destroy(view);
    page_cow_region_destroy(region);
}

//=============================================================================
// Stress Test 4: Mixed Operations
//=============================================================================

TEST_F(PageCowStressTest, MixedOperations) {
    page_cow_config_t config = page_cow_default_config(REGION_SIZE);
    page_cow_region_t region = page_cow_region_create(&config, nullptr);
    ASSERT_NE(region, nullptr);

    std::atomic<bool> stop{false};
    std::atomic<int> operations{0};

    auto view_creator = [&]() {
        while (!stop) {
            page_cow_view_t view = page_cow_view_create(region);
            if (view) {
                operations++;
                std::this_thread::sleep_for(std::chrono::microseconds(10));
                page_cow_view_destroy(view);
            }
        }
    };

    auto cow_trigger = [&]() {
        std::mt19937 rng(std::random_device{}());
        while (!stop) {
            page_cow_view_t view = page_cow_view_create(region);
            if (view) {
                // Make random page private
                size_t page = rng() % (REGION_SIZE / PAGE_COW_PAGE_SIZE);
                page_cow_view_make_page_private(view, page);
                operations++;
                page_cow_view_destroy(view);
            }
        }
    };

    auto snapshot_maker = [&]() {
        while (!stop) {
            page_cow_view_t view = page_cow_view_create(region);
            if (view) {
                page_cow_snapshot_t snap = page_cow_snapshot_create(view);
                if (snap) {
                    operations++;
                    page_cow_snapshot_destroy(snap);
                }
                page_cow_view_destroy(view);
            }
        }
    };

    // Start mixed workload
    std::vector<std::thread> threads;
    threads.emplace_back(view_creator);
    threads.emplace_back(view_creator);
    threads.emplace_back(cow_trigger);
    threads.emplace_back(cow_trigger);
    threads.emplace_back(snapshot_maker);

    // Run for 1 second
    std::this_thread::sleep_for(std::chrono::seconds(1));
    stop = true;

    for (auto& t : threads) {
        t.join();
    }

    // Should have completed many operations
    EXPECT_GT(operations.load(), 100);

    page_cow_region_destroy(region);
}

//=============================================================================
// Stress Test 5: View Clone Chain
//=============================================================================

TEST_F(PageCowStressTest, ViewCloneChain) {
    page_cow_config_t config = page_cow_default_config(REGION_SIZE);
    page_cow_region_t region = page_cow_region_create(&config, nullptr);
    ASSERT_NE(region, nullptr);

    // Create chain of clones
    std::vector<page_cow_view_t> views;
    views.push_back(page_cow_view_create(region));
    ASSERT_NE(views[0], nullptr);

    for (int i = 0; i < 100; i++) {
        page_cow_view_t clone = page_cow_view_clone(views.back());
        ASSERT_NE(clone, nullptr);
        views.push_back(clone);
    }

    // All should share same data initially
    for (auto& view : views) {
        EXPECT_EQ(page_cow_view_get_private_page_count(view), 0);
    }

    // Modify alternate views
    for (size_t i = 0; i < views.size(); i += 2) {
        page_cow_view_make_page_private(views[i], i % 10);
    }

    // Cleanup in random order
    std::mt19937 rng(42);
    while (!views.empty()) {
        size_t idx = rng() % views.size();
        page_cow_view_destroy(views[idx]);
        views.erase(views.begin() + idx);
    }

    page_cow_region_destroy(region);
}

//=============================================================================
// Stress Test 6: High Contention
//=============================================================================

TEST_F(PageCowStressTest, HighContention) {
    // Small region = more contention on same pages
    page_cow_config_t config = page_cow_default_config(64 * 1024);  // 64KB = 16 pages
    page_cow_region_t region = page_cow_region_create(&config, nullptr);
    ASSERT_NE(region, nullptr);

    std::atomic<int> successful_ops{0};

    auto worker = [&](int thread_id) {
        for (int i = 0; i < 200; i++) {
            page_cow_view_t view = page_cow_view_create(region);
            if (!view) continue;

            // All threads try to make same pages private (contention)
            for (int p = 0; p < 16; p++) {
                page_cow_view_make_page_private(view, p);
            }

            // Write to view
            void* data = page_cow_view_write(view);
            if (data) {
                memset(data, thread_id, 64 * 1024);
                successful_ops++;
            }

            page_cow_view_destroy(view);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(successful_ops.load(), NUM_THREADS * 100);

    page_cow_region_destroy(region);
}

//=============================================================================
// Stress Test 7: Memory Pressure
//=============================================================================

TEST_F(PageCowStressTest, MemoryPressure) {
    // Create many regions and views
    std::vector<page_cow_region_t> regions;
    std::vector<page_cow_view_t> views;

    // Create 10 regions of 10MB each
    for (int r = 0; r < 10; r++) {
        page_cow_config_t config = page_cow_default_config(10 * 1024 * 1024);
        page_cow_region_t region = page_cow_region_create(&config, nullptr);
        if (!region) break;
        regions.push_back(region);

        // Create 10 views per region
        for (int v = 0; v < 10; v++) {
            page_cow_view_t view = page_cow_view_create(region);
            if (!view) break;
            views.push_back(view);
        }
    }

    // Verify memory savings
    size_t total_saved = 0;
    for (auto& view : views) {
        total_saved += page_cow_view_get_memory_saved(view);
    }

    // Should save ~90% since all views share
    EXPECT_GT(total_saved, views.size() * 9 * 1024 * 1024);  // >90% of 10MB per view

    // Cleanup
    for (auto& view : views) {
        page_cow_view_destroy(view);
    }
    for (auto& region : regions) {
        page_cow_region_destroy(region);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
