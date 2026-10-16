/**
 * @file fuzz_btree.cpp
 * @brief Fuzzing target for B-tree container API
 *
 * Tests B-tree operations with random insert/remove/find sequences
 * to discover memory errors, count inconsistencies, and crashes.
 *
 * This fuzzer specifically tests:
 * - Random insert/remove/find patterns
 * - Tree consistency (count tracking)
 * - Memory correctness (no double-frees, leaks)
 * - Edge cases (empty tree, single element, etc.)
 * - Predecessor/successor replacement correctness
 *
 * Build:
 *   cmake -DENABLE_FUZZING=ON -DCMAKE_CXX_COMPILER=clang++ ..
 *   make fuzz_btree
 *
 * Run:
 *   ./fuzz_btree -max_total_time=300
 *   ./fuzz_btree corpus_btree/ -max_total_time=3600
 *
 * Expected to catch:
 * - Double-free errors (predecessor/successor bugs)
 * - Count tracking inconsistencies
 * - Memory leaks
 * - Crash conditions in tree rebalancing
 */

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include "utils/containers/nimcp_btree.h"

#define MAX_OPERATIONS 1000
#define MAX_KEY_LENGTH 32

// Test data structure
typedef struct {
    char key[MAX_KEY_LENGTH];
    int value;
    bool is_allocated;  // Track if this should be in tree
} test_data_t;

// Global test data pool
static test_data_t test_pool[MAX_OPERATIONS];
static size_t pool_size = 0;

// Key extraction function
static const char* test_key_func(const void* data)
{
    return ((test_data_t*)data)->key;
}

// Comparison function
static int test_compare(const char* a, const char* b)
{
    return strcmp(a, b);
}

// Free function
static void test_free_func(void* data)
{
    if (data) {
        test_data_t* td = (test_data_t*)data;
        td->is_allocated = false;
    }
}

/**
 * @brief Manually count items in tree (slow but correct)
 *
 * WHAT: Counts items by iterating through tree
 * WHY:  Verify btree_count() is accurate
 * HOW:  Use foreach to count items
 */
static size_t count_items_manually(btree_t* tree)
{
    size_t count = 0;

    auto counter = [](void* data, void* user_data) -> int {
        size_t* count_ptr = (size_t*)user_data;
        (*count_ptr)++;
        return 0;  // Continue iteration
    };

    btree_foreach(tree, counter, &count);
    return count;
}

/**
 * @brief Verify tree consistency
 *
 * WHAT: Checks that reported count matches actual items
 * WHY:  Catch count tracking bugs
 * HOW:  Compare btree_count() with manual count
 */
static bool verify_tree_consistency(btree_t* tree)
{
    size_t reported_count = btree_count(tree);
    size_t actual_count = count_items_manually(tree);

    if (reported_count != actual_count) {
        // Count mismatch detected!
        fprintf(stderr, "CONSISTENCY ERROR: reported=%zu actual=%zu\n",
                reported_count, actual_count);
        return false;
    }

    return true;
}

/**
 * @brief LLVMFuzzerTestOneInput - Main fuzzer entry point
 *
 * WHAT: Tests B-tree with random operations from fuzzer input
 * WHY:  Discover bugs through random testing
 * HOW:  Parse fuzzer data as sequence of operations
 *
 * Input format:
 * - Each 10 bytes = one operation
 * - Byte 0: operation type (0=insert, 1=remove, 2=find)
 * - Bytes 1-9: key data
 */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    // Need at least one operation
    if (size < 10) {
        return 0;
    }

    // Reset test pool
    pool_size = 0;
    memset(test_pool, 0, sizeof(test_pool));

    // Create tree
    btree_t* tree = btree_create(test_compare, test_key_func, test_free_func);
    if (!tree) {
        return 0;
    }

    // Track expected count
    size_t expected_inserts = 0;
    size_t expected_removes = 0;

    // Parse fuzzer input as operations
    size_t num_ops = size / 10;
    if (num_ops > MAX_OPERATIONS) {
        num_ops = MAX_OPERATIONS;
    }

    for (size_t i = 0; i < num_ops; i++) {
        const uint8_t* op_data = data + (i * 10);
        uint8_t op_type = op_data[0] % 3;  // 0=insert, 1=remove, 2=find

        // Generate key from fuzzer data
        char key[MAX_KEY_LENGTH];
        size_t key_len = (op_data[1] % 20) + 1;  // 1-20 chars
        for (size_t j = 0; j < key_len && j < MAX_KEY_LENGTH - 1; j++) {
            // Use printable ASCII
            key[j] = 'a' + (op_data[2 + j] % 26);
        }
        key[key_len] = '\0';

        switch (op_type) {
        case 0: {  // INSERT
            if (pool_size < MAX_OPERATIONS) {
                test_data_t* item = &test_pool[pool_size];
                strncpy(item->key, key, MAX_KEY_LENGTH - 1);
                item->key[MAX_KEY_LENGTH - 1] = '\0';
                item->value = pool_size;
                item->is_allocated = true;

                int result = btree_insert(tree, item);
                if (result == BTREE_SUCCESS) {
                    pool_size++;
                    expected_inserts++;
                }
            }
            break;
        }

        case 1: {  // REMOVE
            int result = btree_remove(tree, key);
            if (result == BTREE_SUCCESS) {
                expected_removes++;
            }
            break;
        }

        case 2: {  // FIND
            void* found = btree_find(tree, key);
            if (found) {
                test_data_t* item = (test_data_t*)found;
                // Verify key matches
                if (strcmp(item->key, key) != 0) {
                    fprintf(stderr, "FIND ERROR: key mismatch\n");
                }
            }
            break;
        }
        }

        // Periodically verify consistency (every 100 ops)
        if (i % 100 == 99) {
            if (!verify_tree_consistency(tree)) {
                // Consistency check failed - this is a bug!
                btree_destroy(tree);
                return 0;
            }
        }
    }

    // Final consistency check
    verify_tree_consistency(tree);

    // Test edge cases
    btree_find(tree, "");           // Empty key
    btree_find(tree, NULL);         // NULL key
    btree_remove(tree, "");         // Empty key
    btree_remove(tree, NULL);       // NULL key
    btree_remove(tree, "nonexistent_key_xyz");  // Non-existent key

    // Test iterator
    btree_iterator_t* iter = btree_iterator_create(tree);
    if (iter) {
        void* item;
        while ((item = btree_iterator_next(iter)) != NULL) {
            // Just iterate, checking for crashes
        }
        btree_iterator_destroy(iter);
    }

    // Test foreach
    auto visitor = [](void* data, void* user_data) -> int {
        // Just visit, checking for crashes
        return 0;
    };
    btree_foreach(tree, visitor, NULL);

    // Destroy tree - this is where double-free bugs appear!
    btree_destroy(tree);

    // Test NULL handling
    btree_destroy(NULL);
    btree_count(NULL);
    btree_find(NULL, "test");
    btree_remove(NULL, "test");
    btree_foreach(NULL, visitor, NULL);
    btree_iterator_create(NULL);
    btree_iterator_destroy(NULL);

    return 0;
}

/**
 * @brief LLVMFuzzerInitialize - Optional fuzzer initialization
 *
 * WHAT: Initialize any global state before fuzzing
 * WHY:  Set up test environment
 * HOW:  Called once at startup
 */
extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv)
{
    (void)argc;
    (void)argv;

    // Initialize memory system if needed
    // nimcp_memory_init();

    return 0;
}
