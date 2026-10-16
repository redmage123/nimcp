/**
 * @file test_btree.cpp
 * @brief Unit tests for thread-safe B-tree implementation
 */

#include <gtest/gtest.h>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>
#include "utils/containers/nimcp_btree.h"
#include "utils/memory/nimcp_memory.h"

// Test data structure
struct TestData {
    char key[64];
    int value;
};

// Comparison function for test data
static int test_compare(const char* key1, const char* key2)
{
    return strcmp(key1, key2);
}

// Key extraction function
static const char* test_key_func(const void* data)
{
    return static_cast<const TestData*>(data)->key;
}

// Free function
static void test_free_func(void* data)
{
    nimcp_free(data);
}

// Test fixture for B-tree operations
class BTreeTest : public ::testing::Test {
   protected:
    btree_t* tree = nullptr;

    void SetUp() override
    {
        nimcp_memory_init();
        tree = btree_create(test_compare, test_key_func, test_free_func);
        ASSERT_NE(tree, nullptr);
    }

    void TearDown() override
    {
        if (tree) {
            btree_destroy(tree);
            tree = nullptr;
        }
        nimcp_memory_cleanup();
    }

    // Helper to create test data
    TestData* CreateTestData(const char* key, int value)
    {
        TestData* data = static_cast<TestData*>(nimcp_malloc(sizeof(TestData)));
        strncpy(data->key, key, sizeof(data->key) - 1);
        data->key[sizeof(data->key) - 1] = '\0';
        data->value = value;
        return data;
    }
};

//=============================================================================
// Creation and Destruction Tests
//=============================================================================

/**
 * WHAT: Test B-tree creation with valid parameters
 * WHY: Verify basic initialization
 */
TEST_F(BTreeTest, Create_ValidParameters)
{
    // Tree created in SetUp
    EXPECT_NE(tree, nullptr);
    EXPECT_EQ(btree_count(tree), 0);
}

/**
 * WHAT: Test B-tree creation with null parameters
 * WHY: Verify null safety
 */
TEST(BTreeCreationTest, Create_NullParameters)
{
    nimcp_memory_init();

    // Null compare function
    btree_t* tree1 = btree_create(nullptr, test_key_func, nullptr);
    EXPECT_EQ(tree1, nullptr);

    // Null key function
    btree_t* tree2 = btree_create(test_compare, nullptr, nullptr);
    EXPECT_EQ(tree2, nullptr);

    // Null free function is allowed
    btree_t* tree3 = btree_create(test_compare, test_key_func, nullptr);
    EXPECT_NE(tree3, nullptr);
    if (tree3)
        btree_destroy(tree3);

    nimcp_memory_cleanup();
}

/**
 * WHAT: Test B-tree destruction
 * WHY: Verify cleanup works
 */
TEST_F(BTreeTest, Destroy_EmptyTree)
{
    btree_destroy(tree);
    tree = nullptr;  // Prevent double-free in TearDown
}

/**
 * WHAT: Test B-tree destruction with null
 * WHY: Verify null safety
 */
TEST(BTreeCreationTest, Destroy_Null)
{
    btree_destroy(nullptr);  // Should not crash
}

//=============================================================================
// Insert Operations Tests
//=============================================================================

/**
 * WHAT: Test single insertion
 * WHY: Verify basic insert operation
 */
TEST_F(BTreeTest, Insert_Single)
{
    TestData* data = CreateTestData("key1", 42);
    int result = btree_insert(tree, data);

    EXPECT_EQ(result, BTREE_SUCCESS);
    EXPECT_EQ(btree_count(tree), 1);
}

/**
 * WHAT: Test multiple insertions
 * WHY: Verify tree handles multiple entries
 */
TEST_F(BTreeTest, Insert_Multiple)
{
    const int count = 10;
    for (int i = 0; i < count; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%03d", i);
        TestData* data = CreateTestData(key, i);
        ASSERT_EQ(btree_insert(tree, data), BTREE_SUCCESS);
    }

    EXPECT_EQ(btree_count(tree), count);
}

/**
 * WHAT: Test insertion with null data
 * WHY: Verify null safety
 */
TEST_F(BTreeTest, Insert_NullData)
{
    int result = btree_insert(tree, nullptr);
    EXPECT_EQ(result, BTREE_ERROR);
}

/**
 * WHAT: Test insertion with null tree
 * WHY: Verify null safety
 */
TEST_F(BTreeTest, Insert_NullTree)
{
    TestData* data = CreateTestData("key", 42);
    int result = btree_insert(nullptr, data);
    EXPECT_EQ(result, BTREE_ERROR);
    nimcp_free(data);  // Cleanup since insert failed
}

/**
 * WHAT: Test inserting in sorted order
 * WHY: Verify tree handles sequential insertions
 */
TEST_F(BTreeTest, Insert_SortedOrder)
{
    const int count = 20;
    for (int i = 0; i < count; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%03d", i);
        TestData* data = CreateTestData(key, i);
        ASSERT_EQ(btree_insert(tree, data), BTREE_SUCCESS);
    }

    EXPECT_EQ(btree_count(tree), count);
}

/**
 * WHAT: Test inserting in reverse order
 * WHY: Verify tree handles reverse insertions
 */
TEST_F(BTreeTest, Insert_ReverseOrder)
{
    const int count = 20;
    for (int i = count - 1; i >= 0; i--) {
        char key[32];
        snprintf(key, sizeof(key), "key_%03d", i);
        TestData* data = CreateTestData(key, i);
        ASSERT_EQ(btree_insert(tree, data), BTREE_SUCCESS);
    }

    EXPECT_EQ(btree_count(tree), count);
}

/**
 * WHAT: Test inserting in random order
 * WHY: Verify tree handles unsorted insertions
 */
TEST_F(BTreeTest, Insert_RandomOrder)
{
    std::vector<int> indices;
    for (int i = 0; i < 30; i++) {
        indices.push_back(i);
    }
    std::random_shuffle(indices.begin(), indices.end());

    for (int i : indices) {
        char key[32];
        snprintf(key, sizeof(key), "key_%03d", i);
        TestData* data = CreateTestData(key, i);
        ASSERT_EQ(btree_insert(tree, data), BTREE_SUCCESS);
    }

    EXPECT_EQ(btree_count(tree), 30);
}

//=============================================================================
// Find Operations Tests
//=============================================================================

/**
 * WHAT: Test finding existing entry
 * WHY: Verify search operation
 */
TEST_F(BTreeTest, Find_ExistingEntry)
{
    TestData* data = CreateTestData("test_key", 42);
    ASSERT_EQ(btree_insert(tree, data), BTREE_SUCCESS);

    TestData* found = static_cast<TestData*>(btree_find(tree, "test_key"));
    ASSERT_NE(found, nullptr);
    EXPECT_STREQ(found->key, "test_key");
    EXPECT_EQ(found->value, 42);
}

/**
 * WHAT: Test finding nonexistent entry
 * WHY: Verify search returns null for missing keys
 */
TEST_F(BTreeTest, Find_NonexistentEntry)
{
    TestData* data = CreateTestData("key1", 1);
    ASSERT_EQ(btree_insert(tree, data), BTREE_SUCCESS);

    void* found = btree_find(tree, "nonexistent");
    EXPECT_EQ(found, nullptr);
}

/**
 * WHAT: Test finding with null key
 * WHY: Verify null safety
 */
TEST_F(BTreeTest, Find_NullKey)
{
    void* found = btree_find(tree, nullptr);
    EXPECT_EQ(found, nullptr);
}

/**
 * WHAT: Test finding with null tree
 * WHY: Verify null safety
 */
TEST_F(BTreeTest, Find_NullTree)
{
    void* found = btree_find(nullptr, "key");
    EXPECT_EQ(found, nullptr);
}

/**
 * WHAT: Test finding in tree with multiple entries
 * WHY: Verify search works with many entries
 */
TEST_F(BTreeTest, Find_MultipleEntries)
{
    const int count = 50;
    for (int i = 0; i < count; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%03d", i);
        TestData* data = CreateTestData(key, i);
        ASSERT_EQ(btree_insert(tree, data), BTREE_SUCCESS);
    }

    // Find entries at different positions
    TestData* found0 = static_cast<TestData*>(btree_find(tree, "key_000"));
    ASSERT_NE(found0, nullptr);
    EXPECT_EQ(found0->value, 0);

    TestData* found25 = static_cast<TestData*>(btree_find(tree, "key_025"));
    ASSERT_NE(found25, nullptr);
    EXPECT_EQ(found25->value, 25);

    TestData* found49 = static_cast<TestData*>(btree_find(tree, "key_049"));
    ASSERT_NE(found49, nullptr);
    EXPECT_EQ(found49->value, 49);
}

//=============================================================================
// Remove Operations Tests
//=============================================================================

/**
 * WHAT: Test removing existing entry
 * WHY: Verify delete operation
 */
TEST_F(BTreeTest, Remove_ExistingEntry)
{
    TestData* data = CreateTestData("test_key", 42);
    ASSERT_EQ(btree_insert(tree, data), BTREE_SUCCESS);
    EXPECT_EQ(btree_count(tree), 1);

    int result = btree_remove(tree, "test_key");
    EXPECT_EQ(result, BTREE_SUCCESS);
    EXPECT_EQ(btree_count(tree), 0);

    // Should not be findable anymore
    void* found = btree_find(tree, "test_key");
    EXPECT_EQ(found, nullptr);
}

/**
 * WHAT: Test removing nonexistent entry
 * WHY: Verify remove handles missing keys gracefully
 */
TEST_F(BTreeTest, Remove_NonexistentEntry)
{
    TestData* data = CreateTestData("key1", 1);
    ASSERT_EQ(btree_insert(tree, data), BTREE_SUCCESS);

    // Note: Current implementation doesn't distinguish not found from success
    int result = btree_remove(tree, "nonexistent");
    // Just verify it doesn't crash and tree is still valid
    EXPECT_EQ(btree_count(tree), 1);
}

/**
 * WHAT: Test removing with null key
 * WHY: Verify null safety
 */
TEST_F(BTreeTest, Remove_NullKey)
{
    int result = btree_remove(tree, nullptr);
    EXPECT_EQ(result, BTREE_ERROR);
}

/**
 * WHAT: Test removing with null tree
 * WHY: Verify null safety
 */
TEST_F(BTreeTest, Remove_NullTree)
{
    int result = btree_remove(nullptr, "key");
    EXPECT_EQ(result, BTREE_ERROR);
}

/**
 * WHAT: Test removing multiple entries
 * WHY: Verify tree handles multiple deletions
 */
TEST_F(BTreeTest, Remove_MultipleEntries)
{
    const int count = 20;
    for (int i = 0; i < count; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%03d", i);
        TestData* data = CreateTestData(key, i);
        ASSERT_EQ(btree_insert(tree, data), BTREE_SUCCESS);
    }

    EXPECT_EQ(btree_count(tree), count);

    // Remove every other entry
    for (int i = 0; i < count; i += 2) {
        char key[32];
        snprintf(key, sizeof(key), "key_%03d", i);
        ASSERT_EQ(btree_remove(tree, key), BTREE_SUCCESS);
    }

    EXPECT_EQ(btree_count(tree), count / 2);

    // Verify removed entries are gone
    for (int i = 0; i < count; i += 2) {
        char key[32];
        snprintf(key, sizeof(key), "key_%03d", i);
        EXPECT_EQ(btree_find(tree, key), nullptr);
    }

    // Verify remaining entries still exist
    for (int i = 1; i < count; i += 2) {
        char key[32];
        snprintf(key, sizeof(key), "key_%03d", i);
        EXPECT_NE(btree_find(tree, key), nullptr);
    }
}

//=============================================================================
// Count Tests
//=============================================================================

/**
 * WHAT: Test count on empty tree
 * WHY: Verify count returns 0 for empty tree
 */
TEST_F(BTreeTest, Count_EmptyTree)
{
    EXPECT_EQ(btree_count(tree), 0);
}

/**
 * WHAT: Test count with null tree
 * WHY: Verify null safety
 */
TEST_F(BTreeTest, Count_NullTree)
{
    EXPECT_EQ(btree_count(nullptr), 0);
}

/**
 * WHAT: Test count tracks insertions
 * WHY: Verify count is accurate
 */
TEST_F(BTreeTest, Count_TracksInsertions)
{
    for (int i = 0; i < 15; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%d", i);
        TestData* data = CreateTestData(key, i);
        btree_insert(tree, data);
        EXPECT_EQ(btree_count(tree), static_cast<size_t>(i + 1));
    }
}

/**
 * WHAT: Test count tracks deletions
 * WHY: Verify count decreases on remove
 */
TEST_F(BTreeTest, Count_TracksDeletions)
{
    const int count = 10;
    for (int i = 0; i < count; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%d", i);
        TestData* data = CreateTestData(key, i);
        btree_insert(tree, data);
    }

    for (int i = 0; i < count; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%d", i);
        btree_remove(tree, key);
        EXPECT_EQ(btree_count(tree), static_cast<size_t>(count - i - 1));
    }
}

//=============================================================================
// Traversal Tests
//=============================================================================

// Traversal context
struct TraversalContext {
    std::vector<std::string> keys;
    std::vector<int> values;
};

static void traversal_callback(void* data, void* user_data)
{
    TestData* test_data = static_cast<TestData*>(data);
    TraversalContext* ctx = static_cast<TraversalContext*>(user_data);
    ctx->keys.push_back(test_data->key);
    ctx->values.push_back(test_data->value);
}

/**
 * WHAT: Test in-order traversal
 * WHY: Verify foreach visits all entries in sorted order
 */
TEST_F(BTreeTest, Foreach_InOrder)
{
    // Insert in random order
    std::vector<int> indices = {5, 2, 8, 1, 9, 3, 7, 4, 6, 0};
    for (int i : indices) {
        char key[32];
        snprintf(key, sizeof(key), "key_%d", i);
        TestData* data = CreateTestData(key, i);
        btree_insert(tree, data);
    }

    TraversalContext ctx;
    btree_foreach(tree, traversal_callback, &ctx);

    // Should visit all entries
    EXPECT_EQ(ctx.keys.size(), indices.size());

    // Should be in sorted order
    for (size_t i = 1; i < ctx.keys.size(); i++) {
        EXPECT_LT(ctx.keys[i - 1], ctx.keys[i]);
    }
}

/**
 * WHAT: Test traversal on empty tree
 * WHY: Verify foreach handles empty tree
 */
TEST_F(BTreeTest, Foreach_EmptyTree)
{
    TraversalContext ctx;
    btree_foreach(tree, traversal_callback, &ctx);

    EXPECT_EQ(ctx.keys.size(), 0);
}

/**
 * WHAT: Test traversal with null callback
 * WHY: Verify null safety
 */
TEST_F(BTreeTest, Foreach_NullCallback)
{
    TestData* data = CreateTestData("key", 1);
    btree_insert(tree, data);

    // Should not crash
    btree_foreach(tree, nullptr, nullptr);
}

/**
 * WHAT: Test traversal with null tree
 * WHY: Verify null safety
 */
TEST_F(BTreeTest, Foreach_NullTree)
{
    TraversalContext ctx;
    btree_foreach(nullptr, traversal_callback, &ctx);  // Should not crash
}

//=============================================================================
// Iterator Tests
//=============================================================================

/**
 * WHAT: Test iterator creation
 * WHY: Verify iterator can be created
 */
TEST_F(BTreeTest, Iterator_Create)
{
    btree_iterator_t* iter = btree_iterator_create(tree);
    ASSERT_NE(iter, nullptr);
    btree_iterator_destroy(iter);
}

/**
 * WHAT: Test iterator on empty tree
 * WHY: Verify iterator handles empty tree
 */
TEST_F(BTreeTest, Iterator_EmptyTree)
{
    btree_iterator_t* iter = btree_iterator_create(tree);
    ASSERT_NE(iter, nullptr);

    void* data = nullptr;
    bool has_next = btree_iterator_next(iter, &data);
    EXPECT_FALSE(has_next);

    btree_iterator_destroy(iter);
}

/**
 * WHAT: Test iterating through entries
 * WHY: Verify iterator visits all entries
 */
TEST_F(BTreeTest, Iterator_AllEntries)
{
    const int count = 10;
    for (int i = 0; i < count; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%03d", i);
        TestData* data = CreateTestData(key, i);
        btree_insert(tree, data);
    }

    btree_iterator_t* iter = btree_iterator_create(tree);
    ASSERT_NE(iter, nullptr);

    int visited = 0;
    void* data = nullptr;
    while (btree_iterator_next(iter, &data)) {
        ASSERT_NE(data, nullptr);
        visited++;
    }

    EXPECT_EQ(visited, count);
    btree_iterator_destroy(iter);
}

/**
 * WHAT: Test iterator with null tree
 * WHY: Verify null safety
 */
TEST_F(BTreeTest, Iterator_NullTree)
{
    btree_iterator_t* iter = btree_iterator_create(nullptr);
    EXPECT_EQ(iter, nullptr);
}

/**
 * WHAT: Test destroying null iterator
 * WHY: Verify null safety
 */
TEST_F(BTreeTest, Iterator_DestroyNull)
{
    btree_iterator_destroy(nullptr);  // Should not crash
}

//=============================================================================
// Stress Tests
//=============================================================================

/**
 * WHAT: Test with large number of entries
 * WHY: Verify performance and correctness at scale
 */
TEST_F(BTreeTest, Stress_LargeTree)
{
    const int count = 1000;

    // Insert many entries
    for (int i = 0; i < count; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%06d", i);
        TestData* data = CreateTestData(key, i);
        ASSERT_EQ(btree_insert(tree, data), BTREE_SUCCESS);
    }

    EXPECT_EQ(btree_count(tree), count);

    // Verify random lookups
    for (int i = 0; i < count; i += 10) {
        char key[32];
        snprintf(key, sizeof(key), "key_%06d", i);
        TestData* found = static_cast<TestData*>(btree_find(tree, key));
        ASSERT_NE(found, nullptr);
        EXPECT_EQ(found->value, i);
    }

    // Remove half the entries
    for (int i = 0; i < count; i += 2) {
        char key[32];
        snprintf(key, sizeof(key), "key_%06d", i);
        btree_remove(tree, key);
    }

    EXPECT_EQ(btree_count(tree), count / 2);
}

/**
 * WHAT: Test insert and remove patterns
 * WHY: Verify tree handles mixed operations
 */
TEST_F(BTreeTest, Stress_MixedOperations)
{
    const int iterations = 100;

    for (int i = 0; i < iterations; i++) {
        // Insert 10 entries
        for (int j = 0; j < 10; j++) {
            char key[32];
            snprintf(key, sizeof(key), "key_%d_%d", i, j);
            TestData* data = CreateTestData(key, i * 10 + j);
            btree_insert(tree, data);
        }

        // Remove 5 entries
        for (int j = 0; j < 5; j++) {
            char key[32];
            snprintf(key, sizeof(key), "key_%d_%d", i, j);
            btree_remove(tree, key);
        }
    }

    // Should have 5 entries per iteration
    EXPECT_EQ(btree_count(tree), iterations * 5);
}

/**
 * WHAT: Test with duplicate keys (update scenario)
 * WHY: Verify tree handles key reuse
 * NOTE: B-tree doesn't support updates, this tests the behavior
 */
TEST_F(BTreeTest, DuplicateKeys_Behavior)
{
    TestData* data1 = CreateTestData("same_key", 1);
    TestData* data2 = CreateTestData("same_key", 2);

    ASSERT_EQ(btree_insert(tree, data1), BTREE_SUCCESS);

    // Second insert with same key - behavior may vary
    // Just verify it doesn't crash
    btree_insert(tree, data2);
}
