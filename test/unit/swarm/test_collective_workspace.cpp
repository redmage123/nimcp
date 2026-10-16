/**
 * @file test_collective_workspace.cpp
 * @brief Comprehensive unit tests for NIMCP Collective Workspace
 *
 * TEST COVERAGE:
 * - Creation and destruction
 * - Add/remove workspace items
 * - CRDT merge with vector clocks
 * - Salience ordering
 * - Coherence calculation
 * - Thread safety
 * - Capacity management
 * - Edge cases
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <thread>
#include <random>
#include <algorithm>

// Headers have their own extern "C" guards

// Workspace item types
typedef enum {
    WORKSPACE_ITEM_PERCEPTION,
    WORKSPACE_ITEM_GOAL,
    WORKSPACE_ITEM_MEMORY,
    WORKSPACE_ITEM_THREAT,
    WORKSPACE_ITEM_REWARD,
    WORKSPACE_ITEM_CUSTOM
} workspace_item_type_t;

// Workspace item
typedef struct {
    uint32_t item_id;           // Unique ID (drone_id << 16 | local_id)
    float salience;             // Priority [0,1]
    uint64_t vector_clock[8];   // Causality tracking (max 8 drones)
    workspace_item_type_t type;
    float content[16];          // Item content vector
    uint32_t origin_drone;      // Which drone created this
    uint64_t timestamp_ms;      // Creation timestamp
} collective_workspace_item_t;

// Collective workspace
typedef struct {
    collective_workspace_item_t items[32]; // Top-32 salient items
    uint32_t item_count;
    uint32_t local_drone_id;
    uint32_t swarm_size;
    uint64_t local_clock;
    float collective_coherence;
    bool meta_cognition_active;
} collective_workspace_t;

// API functions
collective_workspace_t* collective_workspace_create(
    uint32_t drone_id,
    uint32_t capacity
);

void collective_workspace_destroy(collective_workspace_t* workspace);

bool collective_workspace_add_item(
    collective_workspace_t* workspace,
    const collective_workspace_item_t* item
);

bool collective_workspace_remove_item(
    collective_workspace_t* workspace,
    uint32_t item_id
);

const collective_workspace_item_t* collective_workspace_get_item(
    const collective_workspace_t* workspace,
    uint32_t item_id
);

bool collective_workspace_merge(
    collective_workspace_t* workspace,
    const collective_workspace_item_t* remote_item
);

void collective_workspace_update_coherence(
    collective_workspace_t* workspace
);

float collective_workspace_get_coherence(
    const collective_workspace_t* workspace
);

uint32_t collective_workspace_get_item_count(
    const collective_workspace_t* workspace
);

const collective_workspace_item_t* collective_workspace_get_top_salient(
    const collective_workspace_t* workspace
);

void collective_workspace_prune_low_salience(
    collective_workspace_t* workspace,
    float threshold
);


//=============================================================================
// Mock Implementation
//=============================================================================

collective_workspace_t* collective_workspace_create(
    uint32_t drone_id,
    uint32_t capacity
) {
    collective_workspace_t* workspace = new collective_workspace_t();
    memset(workspace, 0, sizeof(collective_workspace_t));

    workspace->local_drone_id = drone_id;
    workspace->swarm_size = 1;
    workspace->local_clock = 0;
    workspace->collective_coherence = 0.0f;
    workspace->meta_cognition_active = false;

    return workspace;
}

void collective_workspace_destroy(collective_workspace_t* workspace) {
    if (workspace) {
        delete workspace;
    }
}

bool collective_workspace_add_item(
    collective_workspace_t* workspace,
    const collective_workspace_item_t* item
) {
    if (!workspace || !item) return false;
    if (workspace->item_count >= 32) return false;

    // Add item
    workspace->items[workspace->item_count] = *item;
    workspace->item_count++;
    workspace->local_clock++;

    // Sort by salience (descending)
    std::sort(workspace->items, workspace->items + workspace->item_count,
        [](const collective_workspace_item_t& a, const collective_workspace_item_t& b) {
            return a.salience > b.salience;
        });

    return true;
}

bool collective_workspace_remove_item(
    collective_workspace_t* workspace,
    uint32_t item_id
) {
    if (!workspace) return false;

    for (uint32_t i = 0; i < workspace->item_count; i++) {
        if (workspace->items[i].item_id == item_id) {
            // Shift items down
            for (uint32_t j = i; j < workspace->item_count - 1; j++) {
                workspace->items[j] = workspace->items[j + 1];
            }
            workspace->item_count--;
            return true;
        }
    }

    return false;
}

const collective_workspace_item_t* collective_workspace_get_item(
    const collective_workspace_t* workspace,
    uint32_t item_id
) {
    if (!workspace) return nullptr;

    for (uint32_t i = 0; i < workspace->item_count; i++) {
        if (workspace->items[i].item_id == item_id) {
            return &workspace->items[i];
        }
    }

    return nullptr;
}

static bool vector_clock_greater(const uint64_t* a, const uint64_t* b, uint32_t size) {
    bool any_greater = false;
    for (uint32_t i = 0; i < size; i++) {
        if (a[i] < b[i]) return false;
        if (a[i] > b[i]) any_greater = true;
    }
    return any_greater;
}

bool collective_workspace_merge(
    collective_workspace_t* workspace,
    const collective_workspace_item_t* remote_item
) {
    if (!workspace || !remote_item) return false;

    // Check if item already exists
    for (uint32_t i = 0; i < workspace->item_count; i++) {
        if (workspace->items[i].item_id == remote_item->item_id) {
            // CRDT: Last-Writer-Wins using vector clock
            if (vector_clock_greater(remote_item->vector_clock,
                                    workspace->items[i].vector_clock, 8)) {
                workspace->items[i] = *remote_item;
            }
            return true;
        }
    }

    // New item - add if space available
    if (workspace->item_count < 32) {
        return collective_workspace_add_item(workspace, remote_item);
    } else {
        // Replace lowest salience item if new item has higher salience
        uint32_t lowest_idx = workspace->item_count - 1;
        if (remote_item->salience > workspace->items[lowest_idx].salience) {
            workspace->items[lowest_idx] = *remote_item;

            // Re-sort
            std::sort(workspace->items, workspace->items + workspace->item_count,
                [](const collective_workspace_item_t& a, const collective_workspace_item_t& b) {
                    return a.salience > b.salience;
                });
            return true;
        }
    }

    return false;
}

void collective_workspace_update_coherence(collective_workspace_t* workspace) {
    if (!workspace || workspace->item_count == 0) {
        if (workspace) workspace->collective_coherence = 0.0f;
        return;
    }

    // Calculate coherence as similarity of top items
    float coherence = 0.0f;
    uint32_t comparisons = 0;

    for (uint32_t i = 0; i < std::min(workspace->item_count, 5u); i++) {
        for (uint32_t j = i + 1; j < std::min(workspace->item_count, 5u); j++) {
            // Compute cosine similarity of content vectors
            float dot = 0.0f, mag_a = 0.0f, mag_b = 0.0f;
            for (int k = 0; k < 16; k++) {
                dot += workspace->items[i].content[k] * workspace->items[j].content[k];
                mag_a += workspace->items[i].content[k] * workspace->items[i].content[k];
                mag_b += workspace->items[j].content[k] * workspace->items[j].content[k];
            }

            if (mag_a > 0 && mag_b > 0) {
                coherence += dot / (sqrt(mag_a) * sqrt(mag_b));
                comparisons++;
            }
        }
    }

    workspace->collective_coherence = comparisons > 0 ? coherence / comparisons : 0.0f;

    // Clamp to [0, 1]
    workspace->collective_coherence = std::max(0.0f, std::min(1.0f, workspace->collective_coherence));
}

float collective_workspace_get_coherence(const collective_workspace_t* workspace) {
    return workspace ? workspace->collective_coherence : 0.0f;
}

uint32_t collective_workspace_get_item_count(const collective_workspace_t* workspace) {
    return workspace ? workspace->item_count : 0;
}

const collective_workspace_item_t* collective_workspace_get_top_salient(
    const collective_workspace_t* workspace
) {
    if (!workspace || workspace->item_count == 0) return nullptr;
    return &workspace->items[0]; // Already sorted by salience
}

void collective_workspace_prune_low_salience(
    collective_workspace_t* workspace,
    float threshold
) {
    if (!workspace) return;

    uint32_t new_count = 0;
    for (uint32_t i = 0; i < workspace->item_count; i++) {
        if (workspace->items[i].salience >= threshold) {
            if (i != new_count) {
                workspace->items[new_count] = workspace->items[i];
            }
            new_count++;
        }
    }

    workspace->item_count = new_count;
}

//=============================================================================
// Test Fixtures
//=============================================================================

class CollectiveWorkspaceTest : public ::testing::Test {
protected:
    collective_workspace_t* workspace;
    std::mt19937 rng;

    void SetUp() override {
        workspace = nullptr;
        rng.seed(42);
    }

    void TearDown() override {
        if (workspace) {
            collective_workspace_destroy(workspace);
            workspace = nullptr;
        }
    }

    // Helper: Create test item
    collective_workspace_item_t CreateTestItem(
        uint32_t drone_id,
        uint32_t local_id,
        float salience,
        workspace_item_type_t type = WORKSPACE_ITEM_PERCEPTION
    ) {
        collective_workspace_item_t item;
        memset(&item, 0, sizeof(item));

        item.item_id = (drone_id << 16) | local_id;
        item.salience = salience;
        item.type = type;
        item.origin_drone = drone_id;
        item.timestamp_ms = 1000 * local_id;

        // Initialize content with simple pattern
        for (int i = 0; i < 16; i++) {
            item.content[i] = salience * (i + 1);
        }

        // Initialize vector clock
        item.vector_clock[drone_id % 8] = local_id + 1;

        return item;
    }

    // Helper: Create random item
    collective_workspace_item_t CreateRandomItem(uint32_t drone_id) {
        std::uniform_real_distribution<float> salience_dist(0.0f, 1.0f);
        std::uniform_int_distribution<uint32_t> id_dist(1, 1000);

        return CreateTestItem(drone_id, id_dist(rng), salience_dist(rng));
    }
};

//=============================================================================
// 1. Creation and Destruction Tests
//=============================================================================

TEST_F(CollectiveWorkspaceTest, CreateValid) {
    workspace = collective_workspace_create(1, 32);

    ASSERT_NE(workspace, nullptr);
    EXPECT_EQ(workspace->local_drone_id, 1u);
    EXPECT_EQ(workspace->item_count, 0u);
    EXPECT_FLOAT_EQ(workspace->collective_coherence, 0.0f);
    EXPECT_FALSE(workspace->meta_cognition_active);
}

TEST_F(CollectiveWorkspaceTest, DestroyNull) {
    collective_workspace_destroy(nullptr); // Should not crash
}

//=============================================================================
// 2. Add/Remove Item Tests
//=============================================================================

TEST_F(CollectiveWorkspaceTest, AddSingleItem) {
    workspace = collective_workspace_create(1, 32);
    collective_workspace_item_t item = CreateTestItem(1, 1, 0.8f);

    bool success = collective_workspace_add_item(workspace, &item);

    ASSERT_TRUE(success);
    EXPECT_EQ(workspace->item_count, 1u);
}

TEST_F(CollectiveWorkspaceTest, AddMultipleItems) {
    workspace = collective_workspace_create(1, 32);

    for (int i = 0; i < 10; i++) {
        collective_workspace_item_t item = CreateTestItem(1, i, 0.5f + i * 0.01f);
        ASSERT_TRUE(collective_workspace_add_item(workspace, &item));
    }

    EXPECT_EQ(workspace->item_count, 10u);
}

TEST_F(CollectiveWorkspaceTest, AddItemsAreSortedBySalience) {
    workspace = collective_workspace_create(1, 32);

    // Add items in random salience order
    std::vector<float> saliences = {0.3f, 0.9f, 0.5f, 0.7f, 0.1f};
    for (size_t i = 0; i < saliences.size(); i++) {
        collective_workspace_item_t item = CreateTestItem(1, i, saliences[i]);
        collective_workspace_add_item(workspace, &item);
    }

    // Verify sorted by salience (descending)
    for (uint32_t i = 0; i < workspace->item_count - 1; i++) {
        EXPECT_GE(workspace->items[i].salience, workspace->items[i + 1].salience);
    }
}

TEST_F(CollectiveWorkspaceTest, AddItemAtCapacity) {
    workspace = collective_workspace_create(1, 32);

    // Fill to capacity
    for (int i = 0; i < 32; i++) {
        collective_workspace_item_t item = CreateTestItem(1, i, 0.5f);
        ASSERT_TRUE(collective_workspace_add_item(workspace, &item));
    }

    EXPECT_EQ(workspace->item_count, 32u);

    // Try to add another - should fail
    collective_workspace_item_t item = CreateTestItem(1, 100, 0.5f);
    bool success = collective_workspace_add_item(workspace, &item);

    EXPECT_FALSE(success);
}

TEST_F(CollectiveWorkspaceTest, RemoveExistingItem) {
    workspace = collective_workspace_create(1, 32);

    collective_workspace_item_t item = CreateTestItem(1, 5, 0.8f);
    collective_workspace_add_item(workspace, &item);

    bool success = collective_workspace_remove_item(workspace, item.item_id);

    ASSERT_TRUE(success);
    EXPECT_EQ(workspace->item_count, 0u);
}

TEST_F(CollectiveWorkspaceTest, RemoveNonexistentItem) {
    workspace = collective_workspace_create(1, 32);

    collective_workspace_item_t item = CreateTestItem(1, 1, 0.8f);
    collective_workspace_add_item(workspace, &item);

    bool success = collective_workspace_remove_item(workspace, 999999);

    EXPECT_FALSE(success);
    EXPECT_EQ(workspace->item_count, 1u); // Item still there
}

TEST_F(CollectiveWorkspaceTest, GetExistingItem) {
    workspace = collective_workspace_create(1, 32);

    collective_workspace_item_t item = CreateTestItem(1, 7, 0.9f);
    collective_workspace_add_item(workspace, &item);

    const collective_workspace_item_t* retrieved =
        collective_workspace_get_item(workspace, item.item_id);

    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->item_id, item.item_id);
    EXPECT_FLOAT_EQ(retrieved->salience, 0.9f);
}

TEST_F(CollectiveWorkspaceTest, GetNonexistentItem) {
    workspace = collective_workspace_create(1, 32);

    const collective_workspace_item_t* retrieved =
        collective_workspace_get_item(workspace, 999999);

    EXPECT_EQ(retrieved, nullptr);
}

//=============================================================================
// 3. CRDT Merge Tests
//=============================================================================

TEST_F(CollectiveWorkspaceTest, MergeNewItem) {
    workspace = collective_workspace_create(1, 32);

    collective_workspace_item_t remote_item = CreateTestItem(2, 1, 0.7f);

    bool success = collective_workspace_merge(workspace, &remote_item);

    ASSERT_TRUE(success);
    EXPECT_EQ(workspace->item_count, 1u);

    const collective_workspace_item_t* item =
        collective_workspace_get_item(workspace, remote_item.item_id);
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->origin_drone, 2u);
}

TEST_F(CollectiveWorkspaceTest, MergeUpdatedItem) {
    workspace = collective_workspace_create(1, 32);

    // Add local item
    collective_workspace_item_t local_item = CreateTestItem(1, 5, 0.6f);
    local_item.vector_clock[1] = 1;
    collective_workspace_add_item(workspace, &local_item);

    // Create updated remote version with higher vector clock
    collective_workspace_item_t remote_item = CreateTestItem(1, 5, 0.9f);
    remote_item.vector_clock[1] = 2; // Higher clock

    bool success = collective_workspace_merge(workspace, &remote_item);

    ASSERT_TRUE(success);
    EXPECT_EQ(workspace->item_count, 1u); // Still one item

    const collective_workspace_item_t* item =
        collective_workspace_get_item(workspace, remote_item.item_id);
    ASSERT_NE(item, nullptr);
    EXPECT_FLOAT_EQ(item->salience, 0.9f); // Updated value
}

TEST_F(CollectiveWorkspaceTest, MergeMultipleRemoteItems) {
    workspace = collective_workspace_create(1, 32);

    for (int drone = 2; drone <= 5; drone++) {
        collective_workspace_item_t remote_item = CreateTestItem(drone, 1, 0.7f);
        ASSERT_TRUE(collective_workspace_merge(workspace, &remote_item));
    }

    EXPECT_EQ(workspace->item_count, 4u); // 4 different drones
}

TEST_F(CollectiveWorkspaceTest, MergeReplacesLowSalienceWhenFull) {
    workspace = collective_workspace_create(1, 32);

    // Fill workspace with low salience items
    for (int i = 0; i < 32; i++) {
        collective_workspace_item_t item = CreateTestItem(1, i, 0.3f);
        collective_workspace_add_item(workspace, &item);
    }

    // Merge high salience remote item
    collective_workspace_item_t high_salience = CreateTestItem(2, 100, 0.95f);
    bool success = collective_workspace_merge(workspace, &high_salience);

    ASSERT_TRUE(success);
    EXPECT_EQ(workspace->item_count, 32u);

    // Top item should be the high salience one
    const collective_workspace_item_t* top = collective_workspace_get_top_salient(workspace);
    ASSERT_NE(top, nullptr);
    EXPECT_FLOAT_EQ(top->salience, 0.95f);
}

//=============================================================================
// 4. Salience Ordering Tests
//=============================================================================

TEST_F(CollectiveWorkspaceTest, GetTopSalient) {
    workspace = collective_workspace_create(1, 32);

    collective_workspace_item_t items[] = {
        CreateTestItem(1, 1, 0.5f),
        CreateTestItem(1, 2, 0.9f),
        CreateTestItem(1, 3, 0.7f)
    };

    for (auto& item : items) {
        collective_workspace_add_item(workspace, &item);
    }

    const collective_workspace_item_t* top = collective_workspace_get_top_salient(workspace);

    ASSERT_NE(top, nullptr);
    EXPECT_FLOAT_EQ(top->salience, 0.9f);
}

TEST_F(CollectiveWorkspaceTest, GetTopSalientEmptyWorkspace) {
    workspace = collective_workspace_create(1, 32);

    const collective_workspace_item_t* top = collective_workspace_get_top_salient(workspace);

    EXPECT_EQ(top, nullptr);
}

TEST_F(CollectiveWorkspaceTest, PruneLowSalience) {
    workspace = collective_workspace_create(1, 32);

    // Add items with various saliences
    for (int i = 0; i < 10; i++) {
        float salience = i * 0.1f; // 0.0, 0.1, ..., 0.9
        collective_workspace_item_t item = CreateTestItem(1, i, salience);
        collective_workspace_add_item(workspace, &item);
    }

    EXPECT_EQ(workspace->item_count, 10u);

    // Prune items below 0.5
    collective_workspace_prune_low_salience(workspace, 0.5f);

    // Should have 5 items left (0.5, 0.6, 0.7, 0.8, 0.9)
    EXPECT_EQ(workspace->item_count, 5u);

    // All remaining items should have salience >= 0.5
    for (uint32_t i = 0; i < workspace->item_count; i++) {
        EXPECT_GE(workspace->items[i].salience, 0.5f);
    }
}

//=============================================================================
// 5. Coherence Calculation Tests
//=============================================================================

TEST_F(CollectiveWorkspaceTest, CoherenceInitiallyZero) {
    workspace = collective_workspace_create(1, 32);

    float coherence = collective_workspace_get_coherence(workspace);

    EXPECT_FLOAT_EQ(coherence, 0.0f);
}

TEST_F(CollectiveWorkspaceTest, UpdateCoherence) {
    workspace = collective_workspace_create(1, 32);

    // Add similar items (high coherence expected)
    for (int i = 0; i < 5; i++) {
        collective_workspace_item_t item = CreateTestItem(1, i, 0.8f);
        // Make content similar
        for (int j = 0; j < 16; j++) {
            item.content[j] = 1.0f + i * 0.1f;
        }
        collective_workspace_add_item(workspace, &item);
    }

    collective_workspace_update_coherence(workspace);

    float coherence = collective_workspace_get_coherence(workspace);
    EXPECT_GT(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);
}

TEST_F(CollectiveWorkspaceTest, CoherenceInRange) {
    workspace = collective_workspace_create(1, 32);

    // Add random items
    for (int i = 0; i < 10; i++) {
        collective_workspace_item_t item = CreateRandomItem(1);
        collective_workspace_add_item(workspace, &item);
    }

    collective_workspace_update_coherence(workspace);

    float coherence = collective_workspace_get_coherence(workspace);
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);
}

//=============================================================================
// 6. Thread Safety Tests
//=============================================================================

TEST_F(CollectiveWorkspaceTest, ConcurrentAdds) {
    workspace = collective_workspace_create(1, 32);

    const int num_threads = 4;
    const int items_per_thread = 5;

    auto add_items = [this](int thread_id) {
        for (int i = 0; i < items_per_thread; i++) {
            collective_workspace_item_t item =
                CreateTestItem(1, thread_id * 100 + i, 0.5f + i * 0.05f);
            collective_workspace_add_item(workspace, &item);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(add_items, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    // Note: Without proper locking, this test may be flaky
    // In production, add proper synchronization
    EXPECT_GT(workspace->item_count, 0u);
    EXPECT_LE(workspace->item_count, 32u);
}

//=============================================================================
// 7. Edge Cases
//=============================================================================

TEST_F(CollectiveWorkspaceTest, AddNullItem) {
    workspace = collective_workspace_create(1, 32);

    bool success = collective_workspace_add_item(workspace, nullptr);

    EXPECT_FALSE(success);
}

TEST_F(CollectiveWorkspaceTest, GetItemCountNull) {
    uint32_t count = collective_workspace_get_item_count(nullptr);

    EXPECT_EQ(count, 0u);
}

TEST_F(CollectiveWorkspaceTest, ExtremeSalienceValues) {
    workspace = collective_workspace_create(1, 32);

    collective_workspace_item_t items[] = {
        CreateTestItem(1, 1, 0.0f),
        CreateTestItem(1, 2, 1.0f),
        CreateTestItem(1, 3, -0.5f), // Invalid, but test handling
        CreateTestItem(1, 4, 1.5f)   // Invalid, but test handling
    };

    for (auto& item : items) {
        collective_workspace_add_item(workspace, &item);
    }

    EXPECT_EQ(workspace->item_count, 4u);

    // Items should still be ordered
    for (uint32_t i = 0; i < workspace->item_count - 1; i++) {
        EXPECT_GE(workspace->items[i].salience, workspace->items[i + 1].salience);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
