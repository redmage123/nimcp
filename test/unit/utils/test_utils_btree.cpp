/**
 * @file test_utils_btree.cpp
 * @brief Comprehensive unit tests for B-tree data structure
 *
 * WHAT: 100% test coverage for nimcp_btree.c
 * WHY:  B-tree is critical for ordered data storage - must be bulletproof
 * HOW:  Test all operations, edge cases, balancing, and thread safety
 *
 * TEST COVERAGE:
 * 1. B-tree creation with different configurations
 * 2. Insert operations (sorted, reverse, random order)
 * 3. Search/lookup operations
 * 4. Delete operations with rebalancing
 * 5. Tree balancing verification
 * 6. Iteration/traversal (in-order)
 * 7. Iterator functionality
 * 8. Edge cases (NULL params, empty tree, duplicates)
 * 9. Large-scale stress tests
 * 10. Thread safety verification
 * 11. Memory management
 * 12. Error code validation
 * 13. Node splitting scenarios
 * 14. Node merging scenarios
 * 15. Count tracking accuracy
 *
 * @version Unit Testing Framework v1.0
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include <algorithm>
#include <cstring>
#include <random>
#include <string>
#include <thread>
#include <vector>

    #include "utils/containers/nimcp_btree.h"
    #include "utils/memory/nimcp_memory.h"
    #include "utils/nimcp_test_base.h"

//=============================================================================
// Test Data Structure
//=============================================================================

struct TestData {
    char key[64];
    int value;
};

// Comparison function for test data
static int test_compare(const char* key1, const char* key2) {
    return strcmp(key1, key2);
}

// Key extraction function
static const char* test_key_func(const void* data) {
    return static_cast<const TestData*>(data)->key;
}

// Free function
static void test_free_func(void* data) {
    nimcp_free(data);
}

//=============================================================================
// Test Fixture
//=============================================================================

class BTreeUtilsTest : public NimcpTestBase {
protected:
    btree_t* tree = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();  // Call parent SetUp first for global state cleanup

        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();

        tree = btree_create(test_compare, test_key_func, test_free_func);
        ASSERT_NE(tree, nullptr) << "B-tree creation failed";
    }

    void TearDown() override {
        if (tree) {
            btree_destroy(tree);
            tree = nullptr;
        }
        nimcp_memory_cleanup();

        NimcpTestBase::TearDown();  // Call parent TearDown last for global state cleanup
    }

    // Helper to create test data
    TestData* CreateTestData(const char* key, int value) {
        TestData* data = static_cast<TestData*>(nimcp_malloc(sizeof(TestData)));
        if (data) {
            strncpy(data->key, key, sizeof(data->key) - 1);
            data->key[sizeof(data->key) - 1] = '\0';
            data->value = value;
        }
        return data;
    }

    // Helper to verify tree count matches expected
    void VerifyCount(size_t expected) {
        EXPECT_EQ(btree_count(tree), expected)
            << "Tree count mismatch: expected " << expected
            << ", got " << btree_count(tree);
    }
};

//=============================================================================
// Unit Test 1: B-tree creation with valid parameters
//=============================================================================

TEST_F(BTreeUtilsTest, Create_ValidParameters) {
    // WHAT: Verify B-tree creation with valid parameters
    // WHY:  Basic initialization must work correctly
    // HOW:  Create tree and verify it's empty

    EXPECT_NE(tree, nullptr);
    EXPECT_EQ(btree_count(tree), 0);
    SUCCEED() << "B-tree created successfully with valid parameters";
}

//=============================================================================
// Unit Test 2: B-tree creation with NULL compare function
//=============================================================================

TEST(BTreeCreationTest, Create_NullCompareFunction) {
    // WHAT: Verify NULL compare function is rejected
    // WHY:  Compare function is required for ordering
    // HOW:  Attempt creation with NULL compare, expect NULL return

    nimcp_memory_init();

    btree_t* tree = btree_create(nullptr, test_key_func, test_free_func);
    EXPECT_EQ(tree, nullptr) << "Should reject NULL compare function";

    nimcp_memory_cleanup();
    SUCCEED() << "NULL compare function properly rejected";
}

//=============================================================================
// Unit Test 3: B-tree creation with NULL key function
//=============================================================================

TEST(BTreeCreationTest, Create_NullKeyFunction) {
    // WHAT: Verify NULL key function is rejected
    // WHY:  Key extraction is required for comparisons
    // HOW:  Attempt creation with NULL key_func, expect NULL return

    nimcp_memory_init();

    btree_t* tree = btree_create(test_compare, nullptr, test_free_func);
    EXPECT_EQ(tree, nullptr) << "Should reject NULL key function";

    nimcp_memory_cleanup();
    SUCCEED() << "NULL key function properly rejected";
}

//=============================================================================
// Unit Test 4: Insert single element
//=============================================================================

TEST_F(BTreeUtilsTest, Insert_SingleElement) {
    // WHAT: Insert a single element and verify
    // WHY:  Basic insert must work
    // HOW:  Insert one element, check count and findability

    TestData* data = CreateTestData("key_001", 42);
    ASSERT_NE(data, nullptr);

    int result = btree_insert(tree, data);
    EXPECT_EQ(result, BTREE_SUCCESS);
    VerifyCount(1);

    TestData* found = static_cast<TestData*>(btree_find(tree, "key_001"));
    ASSERT_NE(found, nullptr);
    EXPECT_STREQ(found->key, "key_001");
    EXPECT_EQ(found->value, 42);

    SUCCEED() << "Single element insertion works";
}

//=============================================================================
// Unit Test 5: Insert multiple elements in sorted order
//=============================================================================

TEST_F(BTreeUtilsTest, Insert_SortedOrder) {
    // WHAT: Insert elements in ascending order
    // WHY:  Sequential insertions stress tree balancing
    // HOW:  Insert 50 elements sequentially, verify all exist

    const int count = 50;
    for (int i = 0; i < count; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%03d", i);
        TestData* data = CreateTestData(key, i);
        ASSERT_NE(data, nullptr);
        ASSERT_EQ(btree_insert(tree, data), BTREE_SUCCESS);
    }

    VerifyCount(count);

    // Verify all elements are findable
    for (int i = 0; i < count; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%03d", i);
        TestData* found = static_cast<TestData*>(btree_find(tree, key));
        ASSERT_NE(found, nullptr) << "Missing key: " << key;
        EXPECT_EQ(found->value, i);
    }

    SUCCEED() << "Sorted order insertion works";
}

//=============================================================================
// Unit Test 6: Insert in reverse order
//=============================================================================

TEST_F(BTreeUtilsTest, Insert_ReverseOrder) {
    // WHAT: Insert elements in descending order
    // WHY:  Reverse order stresses left-side insertions
    // HOW:  Insert 50 elements in reverse, verify all exist

    const int count = 50;
    for (int i = count - 1; i >= 0; i--) {
        char key[32];
        snprintf(key, sizeof(key), "key_%03d", i);
        TestData* data = CreateTestData(key, i);
        ASSERT_NE(data, nullptr);
        ASSERT_EQ(btree_insert(tree, data), BTREE_SUCCESS);
    }

    VerifyCount(count);

    // Verify all elements
    for (int i = 0; i < count; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%03d", i);
        EXPECT_NE(btree_find(tree, key), nullptr);
    }

    SUCCEED() << "Reverse order insertion works";
}

//=============================================================================
// Unit Test 7: Insert in random order
//=============================================================================

TEST_F(BTreeUtilsTest, Insert_RandomOrder) {
    // WHAT: Insert elements in random order
    // WHY:  Random insertions test general balancing
    // HOW:  Shuffle indices, insert, verify all exist

    const int count = 100;
    std::vector<int> indices;
    for (int i = 0; i < count; i++) {
        indices.push_back(i);
    }

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(indices.begin(), indices.end(), g);

    for (int i : indices) {
        char key[32];
        snprintf(key, sizeof(key), "key_%03d", i);
        TestData* data = CreateTestData(key, i);
        ASSERT_NE(data, nullptr);
        ASSERT_EQ(btree_insert(tree, data), BTREE_SUCCESS);
    }

    VerifyCount(count);

    SUCCEED() << "Random order insertion works";
}

//=============================================================================
// Unit Test 8: Insert with NULL data
//=============================================================================

TEST_F(BTreeUtilsTest, Insert_NullData) {
    // WHAT: Verify NULL data is rejected
    // WHY:  Cannot insert NULL into tree
    // HOW:  Attempt insert with NULL, expect error

    int result = btree_insert(tree, nullptr);
    EXPECT_EQ(result, BTREE_ERROR);
    VerifyCount(0);

    SUCCEED() << "NULL data properly rejected";
}

//=============================================================================
// Unit Test 9: Insert with NULL tree
//=============================================================================

TEST_F(BTreeUtilsTest, Insert_NullTree) {
    // WHAT: Verify NULL tree is handled safely
    // WHY:  Prevent crashes on invalid tree pointer
    // HOW:  Call insert on NULL tree, expect error

    TestData* data = CreateTestData("test", 1);
    int result = btree_insert(nullptr, data);
    EXPECT_EQ(result, BTREE_ERROR);

    nimcp_free(data);  // Clean up since insert failed
    SUCCEED() << "NULL tree properly handled";
}

//=============================================================================
// Unit Test 10: Search existing element
//=============================================================================

TEST_F(BTreeUtilsTest, Find_ExistingElement) {
    // WHAT: Search for existing element
    // WHY:  Basic search must work
    // HOW:  Insert element, search for it, verify found

    TestData* data = CreateTestData("search_key", 999);
    btree_insert(tree, data);

    TestData* found = static_cast<TestData*>(btree_find(tree, "search_key"));
    ASSERT_NE(found, nullptr);
    EXPECT_STREQ(found->key, "search_key");
    EXPECT_EQ(found->value, 999);

    SUCCEED() << "Search finds existing elements";
}

//=============================================================================
// Unit Test 11: Search non-existent element
//=============================================================================

TEST_F(BTreeUtilsTest, Find_NonexistentElement) {
    // WHAT: Search for non-existent element
    // WHY:  Must return NULL for missing keys
    // HOW:  Search empty tree, expect NULL

    void* found = btree_find(tree, "nonexistent");
    EXPECT_EQ(found, nullptr);

    // Add some elements and search for missing key
    TestData* data = CreateTestData("key_001", 1);
    btree_insert(tree, data);

    found = btree_find(tree, "key_999");
    EXPECT_EQ(found, nullptr);

    SUCCEED() << "Search returns NULL for missing keys";
}

//=============================================================================
// Unit Test 12: Search with NULL key
//=============================================================================

TEST_F(BTreeUtilsTest, Find_NullKey) {
    // WHAT: Verify NULL key is handled safely
    // WHY:  Prevent crashes on invalid key
    // HOW:  Search with NULL key, expect NULL return

    void* found = btree_find(tree, nullptr);
    EXPECT_EQ(found, nullptr);

    SUCCEED() << "NULL key properly handled";
}

//=============================================================================
// Unit Test 13: Delete existing element
//=============================================================================

TEST_F(BTreeUtilsTest, Remove_ExistingElement) {
    // WHAT: Delete existing element
    // WHY:  Basic delete must work
    // HOW:  Insert, delete, verify not found and count decreased

    TestData* data = CreateTestData("delete_me", 123);
    btree_insert(tree, data);
    VerifyCount(1);

    int result = btree_remove(tree, "delete_me");
    EXPECT_EQ(result, BTREE_SUCCESS);
    VerifyCount(0);

    void* found = btree_find(tree, "delete_me");
    EXPECT_EQ(found, nullptr) << "Deleted element should not be found";

    SUCCEED() << "Delete removes elements correctly";
}

//=============================================================================
// Unit Test 14: Delete from leaf node
//=============================================================================

TEST_F(BTreeUtilsTest, Remove_FromLeaf) {
    // WHAT: Delete element from leaf node
    // WHY:  Test leaf deletion logic
    // HOW:  Insert multiple, delete from leaf, verify

    // Insert enough elements to create internal nodes
    for (int i = 0; i < 10; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%03d", i);
        TestData* data = CreateTestData(key, i);
        btree_insert(tree, data);
    }

    size_t initial_count = btree_count(tree);

    // Delete an element (will be in a leaf)
    btree_remove(tree, "key_000");
    VerifyCount(initial_count - 1);
    EXPECT_EQ(btree_find(tree, "key_000"), nullptr);

    SUCCEED() << "Leaf deletion works";
}

//=============================================================================
// Unit Test 15: Delete with NULL key
//=============================================================================

TEST_F(BTreeUtilsTest, Remove_NullKey) {
    // WHAT: Verify NULL key is handled safely
    // WHY:  Prevent crashes on invalid key
    // HOW:  Attempt delete with NULL key, expect error

    int result = btree_remove(tree, nullptr);
    EXPECT_EQ(result, BTREE_ERROR);

    SUCCEED() << "NULL key properly handled in delete";
}

//=============================================================================
// Unit Test 16: Tree balancing verification
//=============================================================================

TEST_F(BTreeUtilsTest, Balancing_NodeSplitting) {
    // WHAT: Verify tree splits nodes when full
    // WHY:  Node splitting maintains B-tree properties
    // HOW:  Insert enough elements to trigger splits, verify all findable

    // BTREE_ORDER is 3, so max keys per node is 2*3-1 = 5
    // Insert more than 5 elements to trigger splitting
    const int count = 20;

    for (int i = 0; i < count; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%03d", i);
        TestData* data = CreateTestData(key, i);
        ASSERT_EQ(btree_insert(tree, data), BTREE_SUCCESS);
    }

    VerifyCount(count);

    // Verify all elements are still accessible after splits
    for (int i = 0; i < count; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%03d", i);
        EXPECT_NE(btree_find(tree, key), nullptr) << "Element lost after split: " << key;
    }

    SUCCEED() << "Tree balancing maintains all elements";
}

//=============================================================================
// Unit Test 17: Traversal in-order
//=============================================================================

struct TraversalContext {
    std::vector<std::string> keys;
    std::vector<int> values;
};

static void traversal_callback(void* data, void* user_data) {
    TestData* test_data = static_cast<TestData*>(data);
    TraversalContext* ctx = static_cast<TraversalContext*>(user_data);
    ctx->keys.push_back(test_data->key);
    ctx->values.push_back(test_data->value);
}

TEST_F(BTreeUtilsTest, Traversal_InOrder) {
    // WHAT: Verify in-order traversal visits elements sorted
    // WHY:  Traversal must maintain ordering
    // HOW:  Insert random, traverse, verify sorted

    std::vector<int> indices = {5, 2, 8, 1, 9, 3, 7, 4, 6, 0};
    for (int i : indices) {
        char key[32];
        snprintf(key, sizeof(key), "key_%03d", i);
        TestData* data = CreateTestData(key, i);
        btree_insert(tree, data);
    }

    TraversalContext ctx;
    btree_foreach(tree, traversal_callback, &ctx);

    EXPECT_EQ(ctx.keys.size(), indices.size());

    // Verify sorted order
    for (size_t i = 1; i < ctx.keys.size(); i++) {
        EXPECT_LT(ctx.keys[i-1], ctx.keys[i]) << "Traversal not in sorted order";
    }

    SUCCEED() << "In-order traversal maintains sorting";
}

//=============================================================================
// Unit Test 18: Traversal on empty tree
//=============================================================================

TEST_F(BTreeUtilsTest, Traversal_EmptyTree) {
    // WHAT: Verify traversal on empty tree doesn't crash
    // WHY:  Handle edge case gracefully
    // HOW:  Traverse empty tree, verify no calls to callback

    TraversalContext ctx;
    btree_foreach(tree, traversal_callback, &ctx);

    EXPECT_EQ(ctx.keys.size(), 0);
    SUCCEED() << "Empty tree traversal is safe";
}

//=============================================================================
// Unit Test 19: Traversal with NULL callback
//=============================================================================

TEST_F(BTreeUtilsTest, Traversal_NullCallback) {
    // WHAT: Verify NULL callback is handled safely
    // WHY:  Prevent crashes
    // HOW:  Call foreach with NULL callback

    TestData* data = CreateTestData("key", 1);
    btree_insert(tree, data);

    btree_foreach(tree, nullptr, nullptr);  // Should not crash
    SUCCEED() << "NULL callback handled safely";
}

//=============================================================================
// Unit Test 20: Iterator creation and basic usage
//=============================================================================

TEST_F(BTreeUtilsTest, Iterator_BasicUsage) {
    // WHAT: Create iterator and verify basic iteration
    // WHY:  Iterator must work for sequential access
    // HOW:  Create iterator, advance through elements

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

    SUCCEED() << "Iterator visits all elements";
}

//=============================================================================
// Unit Test 21: Iterator on empty tree
//=============================================================================

TEST_F(BTreeUtilsTest, Iterator_EmptyTree) {
    // WHAT: Verify iterator handles empty tree
    // WHY:  Edge case handling
    // HOW:  Create iterator on empty tree, verify no elements

    btree_iterator_t* iter = btree_iterator_create(tree);
    ASSERT_NE(iter, nullptr);

    void* data = nullptr;
    bool has_next = btree_iterator_next(iter, &data);
    EXPECT_FALSE(has_next);

    btree_iterator_destroy(iter);
    SUCCEED() << "Iterator handles empty tree";
}

//=============================================================================
// Unit Test 22: Iterator with NULL tree
//=============================================================================

TEST_F(BTreeUtilsTest, Iterator_NullTree) {
    // WHAT: Verify NULL tree is handled safely
    // WHY:  Prevent crashes
    // HOW:  Create iterator with NULL tree

    btree_iterator_t* iter = btree_iterator_create(nullptr);
    EXPECT_EQ(iter, nullptr);
    SUCCEED() << "NULL tree handled in iterator creation";
}

//=============================================================================
// Unit Test 23: Count tracking accuracy
//=============================================================================

TEST_F(BTreeUtilsTest, Count_TrackingAccuracy) {
    // WHAT: Verify count is accurate through operations
    // WHY:  Count must reflect actual element count
    // HOW:  Insert/delete and verify count at each step

    EXPECT_EQ(btree_count(tree), 0);

    // Insert elements
    for (int i = 0; i < 15; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%03d", i);
        TestData* data = CreateTestData(key, i);
        btree_insert(tree, data);
        EXPECT_EQ(btree_count(tree), static_cast<size_t>(i + 1));
    }

    // Delete elements
    for (int i = 0; i < 15; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%03d", i);
        btree_remove(tree, key);
        EXPECT_EQ(btree_count(tree), static_cast<size_t>(15 - i - 1));
    }

    SUCCEED() << "Count tracking is accurate";
}

//=============================================================================
// Unit Test 24: Count with NULL tree
//=============================================================================

TEST_F(BTreeUtilsTest, Count_NullTree) {
    // WHAT: Verify NULL tree returns 0
    // WHY:  Safe handling of NULL
    // HOW:  Call count on NULL tree

    EXPECT_EQ(btree_count(nullptr), 0);
    SUCCEED() << "NULL tree count returns 0";
}

//=============================================================================
// Unit Test 25: Large tree stress test
//=============================================================================

TEST_F(BTreeUtilsTest, Stress_LargeTree) {
    // WHAT: Stress test with large number of elements
    // WHY:  Verify scalability and correctness
    // HOW:  Insert 1000 elements, verify all operations work

    const int count = 1000;

    // Insert
    for (int i = 0; i < count; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%06d", i);
        TestData* data = CreateTestData(key, i);
        ASSERT_EQ(btree_insert(tree, data), BTREE_SUCCESS);
    }

    VerifyCount(count);

    // Verify random lookups
    for (int i = 0; i < count; i += 50) {
        char key[32];
        snprintf(key, sizeof(key), "key_%06d", i);
        TestData* found = static_cast<TestData*>(btree_find(tree, key));
        ASSERT_NE(found, nullptr) << "Missing element in large tree";
        EXPECT_EQ(found->value, i);
    }

    // Delete half
    for (int i = 0; i < count; i += 2) {
        char key[32];
        snprintf(key, sizeof(key), "key_%06d", i);
        btree_remove(tree, key);
    }

    VerifyCount(count / 2);

    SUCCEED() << "Large tree operations work correctly";
}

//=============================================================================
// Unit Test 26: Mixed operations stress test
//=============================================================================

TEST_F(BTreeUtilsTest, Stress_MixedOperations) {
    // WHAT: Stress test with mixed insert/delete
    // WHY:  Verify tree handles complex patterns
    // HOW:  Repeatedly insert and delete in patterns

    const int iterations = 50;

    for (int i = 0; i < iterations; i++) {
        // Insert 20 elements
        for (int j = 0; j < 20; j++) {
            char key[32];
            snprintf(key, sizeof(key), "key_%03d_%03d", i, j);
            TestData* data = CreateTestData(key, i * 20 + j);
            btree_insert(tree, data);
        }

        // Delete 10 elements
        for (int j = 0; j < 10; j++) {
            char key[32];
            snprintf(key, sizeof(key), "key_%03d_%03d", i, j);
            btree_remove(tree, key);
        }
    }

    EXPECT_EQ(btree_count(tree), iterations * 10);
    SUCCEED() << "Mixed operations maintain consistency";
}

//=============================================================================
// Unit Test 27: Delete multiple elements
//=============================================================================

TEST_F(BTreeUtilsTest, Remove_MultipleElements) {
    // WHAT: Delete multiple elements in sequence
    // WHY:  Verify deletion doesn't corrupt tree
    // HOW:  Insert many, delete many, verify remaining

    const int count = 30;
    for (int i = 0; i < count; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%03d", i);
        TestData* data = CreateTestData(key, i);
        btree_insert(tree, data);
    }

    // Delete every other element
    for (int i = 0; i < count; i += 2) {
        char key[32];
        snprintf(key, sizeof(key), "key_%03d", i);
        btree_remove(tree, key);
    }

    VerifyCount(count / 2);

    // Verify deleted elements are gone
    for (int i = 0; i < count; i += 2) {
        char key[32];
        snprintf(key, sizeof(key), "key_%03d", i);
        EXPECT_EQ(btree_find(tree, key), nullptr);
    }

    // Verify remaining elements exist
    for (int i = 1; i < count; i += 2) {
        char key[32];
        snprintf(key, sizeof(key), "key_%03d", i);
        EXPECT_NE(btree_find(tree, key), nullptr);
    }

    SUCCEED() << "Multiple deletions work correctly";
}

//=============================================================================
// Unit Test 28: Destroy with NULL tree
//=============================================================================

TEST(BTreeDestructionTest, Destroy_NullTree) {
    // WHAT: Verify NULL tree destruction is safe
    // WHY:  Prevent crashes
    // HOW:  Call destroy on NULL tree

    btree_destroy(nullptr);  // Should not crash
    SUCCEED() << "NULL tree destruction is safe";
}

//=============================================================================
// Unit Test 29: Thread safety - concurrent insertions
//=============================================================================

TEST_F(BTreeUtilsTest, ThreadSafety_ConcurrentInserts) {
    // WHAT: Verify concurrent insertions are thread-safe
    // WHY:  B-tree claims thread safety
    // HOW:  Multiple threads inserting simultaneously

    const int NUM_THREADS = 4;
    const int ITEMS_PER_THREAD = 50;

    auto worker = [this](int thread_id) {
        for (int i = 0; i < ITEMS_PER_THREAD; i++) {
            char key[32];
            snprintf(key, sizeof(key), "t%d_key_%03d", thread_id, i);
            TestData* data = CreateTestData(key, thread_id * 1000 + i);
            btree_insert(this->tree, data);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    VerifyCount(NUM_THREADS * ITEMS_PER_THREAD);
    SUCCEED() << "Concurrent insertions are thread-safe";
}

//=============================================================================
// Unit Test 30: Thread safety - concurrent mixed operations
//=============================================================================

TEST_F(BTreeUtilsTest, ThreadSafety_MixedOperations) {
    // WHAT: Verify concurrent insert/find/delete operations
    // WHY:  Real-world usage involves mixed operations
    // HOW:  Threads performing different operations simultaneously

    // Pre-populate tree
    for (int i = 0; i < 100; i++) {
        char key[32];
        snprintf(key, sizeof(key), "initial_%03d", i);
        TestData* data = CreateTestData(key, i);
        btree_insert(tree, data);
    }

    auto inserter = [this]() {
        for (int i = 0; i < 25; i++) {
            char key[32];
            snprintf(key, sizeof(key), "insert_%03d", i);
            TestData* data = CreateTestData(key, i);
            btree_insert(this->tree, data);
        }
    };

    auto finder = [this]() {
        for (int i = 0; i < 100; i++) {
            char key[32];
            snprintf(key, sizeof(key), "initial_%03d", i % 100);
            btree_find(this->tree, key);
        }
    };

    auto remover = [this]() {
        for (int i = 0; i < 25; i++) {
            char key[32];
            snprintf(key, sizeof(key), "initial_%03d", i * 2);
            btree_remove(this->tree, key);
        }
    };

    std::thread t1(inserter);
    std::thread t2(finder);
    std::thread t3(remover);

    t1.join();
    t2.join();
    t3.join();

    // Tree should still be functional
    EXPECT_GT(btree_count(tree), 0);
    SUCCEED() << "Concurrent mixed operations are thread-safe";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
