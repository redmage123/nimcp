//=============================================================================
// test_knowledge_cow.cpp - Unit Tests for Knowledge COW Wrapper
//=============================================================================
/**
 * @file test_knowledge_cow.cpp
 * @brief Unit tests for knowledge system COW wrapper
 *
 * @author NIMCP Development Team
 * @date 2025-11-24
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

// Headers have their own extern "C" guards
#include "cognitive/knowledge/nimcp_knowledge_cow.h"

//=============================================================================
// Test Fixture
//=============================================================================

class KnowledgeCowTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Page COW is initialized automatically
    }

    std::vector<float> createTestKnowledge(size_t num_floats, float start = 0.0f) {
        std::vector<float> data(num_floats);
        for (size_t i = 0; i < num_floats; i++) {
            data[i] = start + static_cast<float>(i);
        }
        return data;
    }
};

//=============================================================================
// Base Creation Tests
//=============================================================================

TEST_F(KnowledgeCowTest, CreateBase_ValidConfig) {
    knowledge_cow_config_t config = knowledge_cow_default_config(1024 * 1024);  // 1MB
    knowledge_cow_base_t base = knowledge_cow_base_create(&config, nullptr, 0);

    ASSERT_NE(base, nullptr);

    knowledge_cow_base_destroy(base);
}

TEST_F(KnowledgeCowTest, CreateBase_WithInitialData) {
    const size_t size = 64 * 1024;  // 64KB
    std::vector<float> data = createTestKnowledge(size / sizeof(float), 100.0f);

    knowledge_cow_config_t config = knowledge_cow_default_config(size);
    knowledge_cow_base_t base = knowledge_cow_base_create(&config, data.data(), data.size() * sizeof(float));

    ASSERT_NE(base, nullptr);

    knowledge_cow_base_destroy(base);
}

TEST_F(KnowledgeCowTest, CreateBase_NullConfig) {
    knowledge_cow_base_t base = knowledge_cow_base_create(nullptr, nullptr, 0);
    EXPECT_EQ(base, nullptr);
}

//=============================================================================
// View Creation Tests
//=============================================================================

TEST_F(KnowledgeCowTest, CreateView_FromBase) {
    const size_t size = 32 * 1024;
    knowledge_cow_config_t config = knowledge_cow_default_config(size);
    knowledge_cow_base_t base = knowledge_cow_base_create(&config, nullptr, 0);
    ASSERT_NE(base, nullptr);

    knowledge_cow_view_t view = knowledge_cow_view_create(base);
    ASSERT_NE(view, nullptr);

    EXPECT_FALSE(knowledge_cow_view_is_modified(view));
    EXPECT_EQ(knowledge_cow_view_get_private_page_count(view), 0);

    knowledge_cow_view_destroy(view);
    knowledge_cow_base_destroy(base);
}

TEST_F(KnowledgeCowTest, CreateView_MultipleViews) {
    const size_t size = 64 * 1024;
    knowledge_cow_config_t config = knowledge_cow_default_config(size);
    knowledge_cow_base_t base = knowledge_cow_base_create(&config, nullptr, 0);
    ASSERT_NE(base, nullptr);

    std::vector<knowledge_cow_view_t> views;
    for (int i = 0; i < 10; i++) {
        knowledge_cow_view_t view = knowledge_cow_view_create(base);
        ASSERT_NE(view, nullptr);
        views.push_back(view);
    }

    knowledge_cow_stats_t stats;
    knowledge_cow_base_get_stats(base, &stats);
    EXPECT_EQ(stats.active_views, 10);

    for (auto view : views) {
        knowledge_cow_view_destroy(view);
    }
    knowledge_cow_base_destroy(base);
}

TEST_F(KnowledgeCowTest, CloneView) {
    const size_t size = 32 * 1024;
    std::vector<float> data = createTestKnowledge(size / sizeof(float), 42.0f);

    knowledge_cow_config_t config = knowledge_cow_default_config(size);
    knowledge_cow_base_t base = knowledge_cow_base_create(&config, data.data(), size);
    ASSERT_NE(base, nullptr);

    knowledge_cow_view_t view1 = knowledge_cow_view_create(base);
    knowledge_cow_view_t view2 = knowledge_cow_view_clone(view1);

    ASSERT_NE(view1, nullptr);
    ASSERT_NE(view2, nullptr);

    // Both should see same data
    const float* data1 = static_cast<const float*>(knowledge_cow_view_read(view1));
    const float* data2 = static_cast<const float*>(knowledge_cow_view_read(view2));

    EXPECT_FLOAT_EQ(data1[0], 42.0f);
    EXPECT_FLOAT_EQ(data2[0], 42.0f);

    knowledge_cow_view_destroy(view2);
    knowledge_cow_view_destroy(view1);
    knowledge_cow_base_destroy(base);
}

//=============================================================================
// Read/Write Tests
//=============================================================================

TEST_F(KnowledgeCowTest, ReadView_NoModification) {
    const size_t size = 16 * 1024;
    std::vector<float> data = createTestKnowledge(size / sizeof(float), 1.0f);

    knowledge_cow_config_t config = knowledge_cow_default_config(size);
    knowledge_cow_base_t base = knowledge_cow_base_create(&config, data.data(), size);
    knowledge_cow_view_t view = knowledge_cow_view_create(base);

    const float* read_data = static_cast<const float*>(knowledge_cow_view_read(view));
    ASSERT_NE(read_data, nullptr);

    EXPECT_FLOAT_EQ(read_data[0], 1.0f);
    EXPECT_FLOAT_EQ(read_data[100], 101.0f);

    // Should still be unmodified
    EXPECT_FALSE(knowledge_cow_view_is_modified(view));

    knowledge_cow_view_destroy(view);
    knowledge_cow_base_destroy(base);
}

TEST_F(KnowledgeCowTest, WriteView_MakesPrivate) {
    const size_t size = 32 * 1024;
    knowledge_cow_config_t config = knowledge_cow_default_config(size);
    knowledge_cow_base_t base = knowledge_cow_base_create(&config, nullptr, 0);
    knowledge_cow_view_t view = knowledge_cow_view_create(base);

    // Make a region private
    ASSERT_TRUE(knowledge_cow_view_make_region_private(view, 0, 4096));

    EXPECT_TRUE(knowledge_cow_view_is_modified(view));
    EXPECT_GT(knowledge_cow_view_get_private_page_count(view), 0);

    knowledge_cow_view_destroy(view);
    knowledge_cow_base_destroy(base);
}

TEST_F(KnowledgeCowTest, WriteView_DataIntegrity) {
    const size_t size = 16 * 1024;
    std::vector<float> data = createTestKnowledge(size / sizeof(float), 0.0f);

    knowledge_cow_config_t config = knowledge_cow_default_config(size);
    knowledge_cow_base_t base = knowledge_cow_base_create(&config, data.data(), size);

    knowledge_cow_view_t view1 = knowledge_cow_view_create(base);
    knowledge_cow_view_t view2 = knowledge_cow_view_create(base);

    // Modify view1
    knowledge_cow_view_make_region_private(view1, 0, 4096);
    float* write_data = static_cast<float*>(knowledge_cow_view_write(view1));
    write_data[0] = 999.0f;

    // View2 should still see original
    const float* read_data2 = static_cast<const float*>(knowledge_cow_view_read(view2));
    EXPECT_FLOAT_EQ(read_data2[0], 0.0f);

    // View1 should see modified
    const float* read_data1 = static_cast<const float*>(knowledge_cow_view_read(view1));
    EXPECT_FLOAT_EQ(read_data1[0], 999.0f);

    knowledge_cow_view_destroy(view2);
    knowledge_cow_view_destroy(view1);
    knowledge_cow_base_destroy(base);
}

//=============================================================================
// Memory Savings Tests
//=============================================================================

TEST_F(KnowledgeCowTest, MemorySavings_AllShared) {
    const size_t size = 1024 * 1024;  // 1MB
    knowledge_cow_config_t config = knowledge_cow_default_config(size);
    knowledge_cow_base_t base = knowledge_cow_base_create(&config, nullptr, 0);

    // Create 10 views sharing knowledge
    std::vector<knowledge_cow_view_t> views;
    for (int i = 0; i < 10; i++) {
        views.push_back(knowledge_cow_view_create(base));
    }

    // Each view saves ~1MB
    for (auto view : views) {
        size_t saved = knowledge_cow_view_get_memory_saved(view);
        EXPECT_GT(saved, 900 * 1024);  // At least 90% savings
    }

    for (auto view : views) {
        knowledge_cow_view_destroy(view);
    }
    knowledge_cow_base_destroy(base);
}

//=============================================================================
// Snapshot Tests
//=============================================================================

TEST_F(KnowledgeCowTest, SnapshotCreate) {
    const size_t size = 32 * 1024;
    knowledge_cow_config_t config = knowledge_cow_default_config(size);
    knowledge_cow_base_t base = knowledge_cow_base_create(&config, nullptr, 0);
    knowledge_cow_view_t view = knowledge_cow_view_create(base);

    knowledge_cow_snapshot_t snap = knowledge_cow_snapshot_create(view);
    ASSERT_NE(snap, nullptr);

    knowledge_cow_snapshot_destroy(snap);
    knowledge_cow_view_destroy(view);
    knowledge_cow_base_destroy(base);
}

TEST_F(KnowledgeCowTest, SnapshotRestore) {
    const size_t size = 16 * 1024;
    std::vector<float> data = createTestKnowledge(size / sizeof(float), 100.0f);

    knowledge_cow_config_t config = knowledge_cow_default_config(size);
    knowledge_cow_base_t base = knowledge_cow_base_create(&config, data.data(), size);
    knowledge_cow_view_t view = knowledge_cow_view_create(base);

    // Create snapshot
    knowledge_cow_snapshot_t snap = knowledge_cow_snapshot_create(view);

    // Modify view
    knowledge_cow_view_make_region_private(view, 0, 4096);
    float* write_data = static_cast<float*>(knowledge_cow_view_write(view));
    write_data[0] = 999.0f;

    EXPECT_TRUE(knowledge_cow_view_is_modified(view));

    // Verify modified
    const float* read = static_cast<const float*>(knowledge_cow_view_read(view));
    EXPECT_FLOAT_EQ(read[0], 999.0f);

    // Restore
    ASSERT_TRUE(knowledge_cow_snapshot_restore(view, snap));

    // Verify restored
    read = static_cast<const float*>(knowledge_cow_view_read(view));
    EXPECT_FLOAT_EQ(read[0], 100.0f);

    knowledge_cow_snapshot_destroy(snap);
    knowledge_cow_view_destroy(view);
    knowledge_cow_base_destroy(base);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(KnowledgeCowTest, NullHandling) {
    knowledge_cow_base_destroy(nullptr);  // Should not crash
    knowledge_cow_view_destroy(nullptr);  // Should not crash

    EXPECT_EQ(knowledge_cow_view_create(nullptr), nullptr);
    EXPECT_EQ(knowledge_cow_view_read(nullptr), nullptr);
    EXPECT_EQ(knowledge_cow_view_write(nullptr), nullptr);
    EXPECT_EQ(knowledge_cow_view_get_memory_saved(nullptr), 0);
    EXPECT_FALSE(knowledge_cow_view_is_modified(nullptr));
    EXPECT_EQ(knowledge_cow_snapshot_create(nullptr), nullptr);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
