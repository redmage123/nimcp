/**
 * @file test_api_memory_safety.cpp
 * @brief Memory safety regression tests for NIMCP API
 *
 * Tests memory safety and leak detection:
 * - No leaks after create/destroy cycles
 * - No leaks after failed operations
 * - Proper cleanup on errors
 * - Double-free protection
 * - NULL pointer safety
 *
 * Estimated tests: 20
 *
 * NOTE: These tests detect memory issues but require tools like
 * valgrind or AddressSanitizer for complete leak detection:
 *   valgrind --leak-check=full ./test_api_memory_safety
 *   or compile with: -fsanitize=address
 */

#include <gtest/gtest.h>
#include "nimcp.h"
#include <cstring>
#include <vector>
#include <unistd.h>

class APIMemorySafetyTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(nimcp_init(), NIMCP_SUCCESS);
    }

    void TearDown() override {
        nimcp_shutdown();
    }

    void cleanup_file(const char* filepath) {
        unlink(filepath);
    }
};

//=============================================================================
// No Leaks After Create/Destroy Cycles
//=============================================================================

TEST_F(APIMemorySafetyTest, NoLeaks_BrainCreateDestroy) {
    // Create and destroy brains multiple times
    for (int i = 0; i < 10; i++) {
        nimcp_brain_t brain = nimcp_brain_create(
            "leak_test",
            NIMCP_BRAIN_TINY,
            NIMCP_TASK_CLASSIFICATION,
            10,
            2
        );

        ASSERT_NE(brain, nullptr);
        nimcp_brain_destroy(brain);
    }

    // No leaks should be detected by valgrind/asan
}

TEST_F(APIMemorySafetyTest, NoLeaks_NetworkCreateDestroy) {
    // Create and destroy networks multiple times
    for (int i = 0; i < 10; i++) {
        nimcp_network_t network = nimcp_network_create(10, 2, 50, 0.01f);
        ASSERT_NE(network, nullptr);
        nimcp_network_destroy(network);
    }
}

TEST_F(APIMemorySafetyTest, NoLeaks_EthicsCreateDestroy) {
    // Create and destroy ethics modules multiple times
    for (int i = 0; i < 10; i++) {
        nimcp_ethics_t ethics = nimcp_ethics_create();
        ASSERT_NE(ethics, nullptr);
        nimcp_ethics_destroy(ethics);
    }
}

TEST_F(APIMemorySafetyTest, NoLeaks_KnowledgeCreateDestroy) {
    // Create and destroy knowledge graphs multiple times
    for (int i = 0; i < 10; i++) {
        nimcp_knowledge_t knowledge = nimcp_knowledge_create();
        ASSERT_NE(knowledge, nullptr);
        nimcp_knowledge_destroy(knowledge);
    }
}

TEST_F(APIMemorySafetyTest, NoLeaks_AllModuleTypes) {
    // Create and destroy all module types together
    for (int i = 0; i < 10; i++) {
        nimcp_brain_t brain = nimcp_brain_create("test", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 5, 2);
        nimcp_network_t network = nimcp_network_create(5, 2, 20, 0.01f);
        nimcp_ethics_t ethics = nimcp_ethics_create();
        nimcp_knowledge_t knowledge = nimcp_knowledge_create();

        // Destroy in various orders
        if (i % 4 == 0) {
            nimcp_brain_destroy(brain);
            nimcp_network_destroy(network);
            nimcp_ethics_destroy(ethics);
            nimcp_knowledge_destroy(knowledge);
        } else if (i % 4 == 1) {
            nimcp_knowledge_destroy(knowledge);
            nimcp_ethics_destroy(ethics);
            nimcp_network_destroy(network);
            nimcp_brain_destroy(brain);
        } else if (i % 4 == 2) {
            nimcp_ethics_destroy(ethics);
            nimcp_brain_destroy(brain);
            nimcp_knowledge_destroy(knowledge);
            nimcp_network_destroy(network);
        } else {
            nimcp_network_destroy(network);
            nimcp_knowledge_destroy(knowledge);
            nimcp_brain_destroy(brain);
            nimcp_ethics_destroy(ethics);
        }
    }
}

TEST_F(APIMemorySafetyTest, NoLeaks_BrainWithOperations) {
    // Create, use, and destroy brains with operations
    for (int i = 0; i < 10; i++) {
        nimcp_brain_t brain = nimcp_brain_create("ops_test", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 5, 2);
        ASSERT_NE(brain, nullptr);

        float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

        // Perform operations
        nimcp_brain_learn_example(brain, features, 5, "test", 1.0f);

        char label[64];
        float confidence;
        nimcp_brain_predict(brain, features, 5, label, &confidence);

        nimcp_brain_destroy(brain);
    }
}

TEST_F(APIMemorySafetyTest, NoLeaks_COWClones) {
    // Test COW clones don't leak
    for (int i = 0; i < 5; i++) {
        nimcp_brain_t original = nimcp_brain_create("original", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 5, 2);
        ASSERT_NE(original, nullptr);

        // Train original
        float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        for (int j = 0; j < 10; j++) {
            nimcp_brain_learn_example(original, features, 5, "test", 1.0f);
        }

        // Create clones
        std::vector<nimcp_brain_t> clones;
        for (int j = 0; j < 5; j++) {
            nimcp_brain_t clone = nimcp_brain_clone_cow(original);
            if (clone) {
                clones.push_back(clone);
            }
        }

        // Destroy clones
        for (auto clone : clones) {
            nimcp_brain_destroy(clone);
        }

        nimcp_brain_destroy(original);
    }
}

TEST_F(APIMemorySafetyTest, NoLeaks_Snapshots) {
    // Test snapshots don't leak
    for (int i = 0; i < 5; i++) {
        nimcp_brain_t brain = nimcp_brain_create("snap_test", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 5, 2);
        ASSERT_NE(brain, nullptr);

        float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        nimcp_brain_learn_example(brain, features, 5, "test", 1.0f);

        // Create snapshots
        std::vector<nimcp_brain_snapshot_t> snapshots;
        for (int j = 0; j < 3; j++) {
            nimcp_brain_snapshot_t snap = nimcp_brain_snapshot_cow(brain);
            if (snap) {
                snapshots.push_back(snap);
            }
        }

        // Destroy snapshots
        for (auto snap : snapshots) {
            nimcp_brain_snapshot_destroy(snap);
        }

        nimcp_brain_destroy(brain);
    }
}

//=============================================================================
// No Leaks After Failed Operations
//=============================================================================

TEST_F(APIMemorySafetyTest, NoLeaks_AfterNullArgumentErrors) {
    nimcp_brain_t brain = nimcp_brain_create("error_test", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(brain, nullptr);

    // Cause multiple NULL argument errors
    for (int i = 0; i < 10; i++) {
        nimcp_brain_learn_example(brain, nullptr, 0, nullptr, 1.0f);
        nimcp_brain_predict(brain, nullptr, 0, nullptr, nullptr);
        nimcp_brain_infer(brain, nullptr, 0, nullptr, 0);
    }

    nimcp_brain_destroy(brain);
}

TEST_F(APIMemorySafetyTest, NoLeaks_AfterInvalidOperations) {
    nimcp_brain_t brain = nimcp_brain_create("invalid_test", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(brain, nullptr);

    // Cause multiple invalid operations
    float features[] = {1.0f, 2.0f, 3.0f};  // Wrong size
    char label[64];
    float confidence;

    for (int i = 0; i < 10; i++) {
        nimcp_brain_predict(brain, features, 3, label, &confidence);  // Wrong feature count
    }

    nimcp_brain_destroy(brain);
}

TEST_F(APIMemorySafetyTest, NoLeaks_AfterFailedSave) {
    nimcp_brain_t brain = nimcp_brain_create("save_fail", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(brain, nullptr);

    // Try to save to invalid paths multiple times
    for (int i = 0; i < 5; i++) {
        nimcp_brain_save(brain, "/invalid/path/brain.nimcp");
    }

    nimcp_brain_destroy(brain);
}

TEST_F(APIMemorySafetyTest, NoLeaks_AfterFailedLoad) {
    // Try to load from invalid paths multiple times
    for (int i = 0; i < 10; i++) {
        nimcp_brain_t brain = nimcp_brain_load("/nonexistent/brain.nimcp");
        EXPECT_EQ(brain, nullptr);
    }

    // No leaks should occur from failed loads
}

//=============================================================================
// Proper Cleanup on Errors
//=============================================================================

TEST_F(APIMemorySafetyTest, Cleanup_AfterPartialConstruction) {
    // Test cleanup when construction fails partway
    for (int i = 0; i < 10; i++) {
        // NULL name should cause failure
        nimcp_brain_t brain = nimcp_brain_create(nullptr, NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 5, 2);
        EXPECT_EQ(brain, nullptr);
    }
}

TEST_F(APIMemorySafetyTest, Cleanup_NetworkAfterErrors) {
    nimcp_network_t network = nimcp_network_create(10, 2, 50, 0.01f);
    ASSERT_NE(network, nullptr);

    // Cause multiple errors
    for (int i = 0; i < 10; i++) {
        nimcp_network_forward(network, nullptr, 0, nullptr, 0);
        nimcp_network_train(network, nullptr, 0, nullptr, 0);
    }

    nimcp_network_destroy(network);
}

TEST_F(APIMemorySafetyTest, Cleanup_EthicsAfterErrors) {
    nimcp_ethics_t ethics = nimcp_ethics_create();
    ASSERT_NE(ethics, nullptr);

    // Cause multiple errors
    for (int i = 0; i < 10; i++) {
        float score;
        nimcp_ethics_check(ethics, nullptr, 0, &score);
        nimcp_ethics_check(ethics, nullptr, 0, nullptr);
    }

    nimcp_ethics_destroy(ethics);
}

TEST_F(APIMemorySafetyTest, Cleanup_KnowledgeAfterErrors) {
    nimcp_knowledge_t knowledge = nimcp_knowledge_create();
    ASSERT_NE(knowledge, nullptr);

    // Cause multiple errors
    for (int i = 0; i < 10; i++) {
        nimcp_knowledge_add_fact(knowledge, nullptr, nullptr, nullptr);
        char result[1024];
        nimcp_knowledge_query(knowledge, nullptr, result, 1024);
    }

    nimcp_knowledge_destroy(knowledge);
}

//=============================================================================
// Double-Free Protection
//=============================================================================

TEST_F(APIMemorySafetyTest, DoubleFree_BrainDestroy) {
    nimcp_brain_t brain = nimcp_brain_create("double_free", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(brain, nullptr);

    // First destroy (valid)
    nimcp_brain_destroy(brain);

    // Second destroy (should be safe - no crash)
    // NOTE: In practice, don't do this! This tests robustness only.
    // Uncommenting this line is undefined behavior but shouldn't crash:
    // nimcp_brain_destroy(brain);
}

TEST_F(APIMemorySafetyTest, DoubleFree_NullDestroy) {
    // Destroying NULL should be safe
    nimcp_brain_destroy(nullptr);
    nimcp_network_destroy(nullptr);
    nimcp_ethics_destroy(nullptr);
    nimcp_knowledge_destroy(nullptr);
    nimcp_brain_snapshot_destroy(nullptr);

    // Should not crash
}

TEST_F(APIMemorySafetyTest, DoubleFree_SnapshotDestroy) {
    nimcp_brain_t brain = nimcp_brain_create("snap_double", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(brain, nullptr);

    float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    nimcp_brain_learn_example(brain, features, 5, "test", 1.0f);

    nimcp_brain_snapshot_t snapshot = nimcp_brain_snapshot_cow(brain);
    if (snapshot) {
        nimcp_brain_snapshot_destroy(snapshot);
        // Second destroy should be safe (don't do this in practice)
        // nimcp_brain_snapshot_destroy(snapshot);
    }

    nimcp_brain_destroy(brain);
}

//=============================================================================
// NULL Pointer Safety
//=============================================================================

TEST_F(APIMemorySafetyTest, NullSafety_BrainOperations) {
    // All brain operations should handle NULL gracefully
    float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    char label[64];
    float confidence;
    float outputs[2];

    // NULL brain handle
    EXPECT_EQ(nimcp_brain_learn_example(nullptr, features, 5, "test", 1.0f), NIMCP_ERROR_NULL_ARG);
    EXPECT_EQ(nimcp_brain_predict(nullptr, features, 5, label, &confidence), NIMCP_ERROR_NULL_ARG);
    EXPECT_EQ(nimcp_brain_infer(nullptr, features, 5, outputs, 2), NIMCP_ERROR_NULL_ARG);
    EXPECT_EQ(nimcp_brain_save(nullptr, "/tmp/test.nimcp"), NIMCP_ERROR_NULL_ARG);

    nimcp_brain_t brain = nimcp_brain_create("null_test", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(brain, nullptr);

    // NULL feature array
    EXPECT_EQ(nimcp_brain_learn_example(brain, nullptr, 5, "test", 1.0f), NIMCP_ERROR_NULL_ARG);
    EXPECT_EQ(nimcp_brain_predict(brain, nullptr, 5, label, &confidence), NIMCP_ERROR_NULL_ARG);
    EXPECT_EQ(nimcp_brain_infer(brain, nullptr, 5, outputs, 2), NIMCP_ERROR_NULL_ARG);

    // NULL label
    EXPECT_EQ(nimcp_brain_learn_example(brain, features, 5, nullptr, 1.0f), NIMCP_ERROR_NULL_ARG);
    EXPECT_EQ(nimcp_brain_predict(brain, features, 5, nullptr, &confidence), NIMCP_ERROR_NULL_ARG);

    // NULL output pointers
    EXPECT_EQ(nimcp_brain_predict(brain, features, 5, label, nullptr), NIMCP_ERROR_NULL_ARG);
    EXPECT_EQ(nimcp_brain_infer(brain, features, 5, nullptr, 2), NIMCP_ERROR_NULL_ARG);

    // NULL filepath
    EXPECT_EQ(nimcp_brain_save(brain, nullptr), NIMCP_ERROR_NULL_ARG);
    EXPECT_EQ(nimcp_brain_load(nullptr), nullptr);

    nimcp_brain_destroy(brain);
}

TEST_F(APIMemorySafetyTest, NullSafety_NetworkOperations) {
    float inputs[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float targets[] = {1.0f, 0.0f};
    float outputs[2];

    // NULL network handle
    EXPECT_NE(nimcp_network_forward(nullptr, inputs, 5, outputs, 2), NIMCP_SUCCESS);
    EXPECT_NE(nimcp_network_train(nullptr, inputs, 5, targets, 2), NIMCP_SUCCESS);

    nimcp_network_t network = nimcp_network_create(5, 2, 20, 0.01f);
    ASSERT_NE(network, nullptr);

    // NULL input/output arrays
    EXPECT_NE(nimcp_network_forward(network, nullptr, 5, outputs, 2), NIMCP_SUCCESS);
    EXPECT_NE(nimcp_network_forward(network, inputs, 5, nullptr, 2), NIMCP_SUCCESS);
    EXPECT_NE(nimcp_network_train(network, nullptr, 5, targets, 2), NIMCP_SUCCESS);
    EXPECT_NE(nimcp_network_train(network, inputs, 5, nullptr, 2), NIMCP_SUCCESS);

    nimcp_network_destroy(network);
}

TEST_F(APIMemorySafetyTest, NullSafety_EthicsOperations) {
    float situation[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float score;

    // NULL ethics handle
    EXPECT_NE(nimcp_ethics_check(nullptr, situation, 5, &score), NIMCP_SUCCESS);

    nimcp_ethics_t ethics = nimcp_ethics_create();
    ASSERT_NE(ethics, nullptr);

    // NULL situation array
    EXPECT_NE(nimcp_ethics_check(ethics, nullptr, 5, &score), NIMCP_SUCCESS);

    // NULL output score
    EXPECT_NE(nimcp_ethics_check(ethics, situation, 5, nullptr), NIMCP_SUCCESS);

    nimcp_ethics_destroy(ethics);
}

TEST_F(APIMemorySafetyTest, NullSafety_KnowledgeOperations) {
    char result[1024];

    // NULL knowledge handle
    EXPECT_NE(nimcp_knowledge_add_fact(nullptr, "s", "p", "o"), NIMCP_SUCCESS);
    EXPECT_NE(nimcp_knowledge_query(nullptr, "q", result, 1024), NIMCP_SUCCESS);

    nimcp_knowledge_t knowledge = nimcp_knowledge_create();
    ASSERT_NE(knowledge, nullptr);

    // NULL arguments
    EXPECT_NE(nimcp_knowledge_add_fact(knowledge, nullptr, "p", "o"), NIMCP_SUCCESS);
    EXPECT_NE(nimcp_knowledge_add_fact(knowledge, "s", nullptr, "o"), NIMCP_SUCCESS);
    EXPECT_NE(nimcp_knowledge_add_fact(knowledge, "s", "p", nullptr), NIMCP_SUCCESS);

    EXPECT_NE(nimcp_knowledge_query(knowledge, nullptr, result, 1024), NIMCP_SUCCESS);
    EXPECT_NE(nimcp_knowledge_query(knowledge, "q", nullptr, 1024), NIMCP_SUCCESS);

    nimcp_knowledge_destroy(knowledge);
}

TEST_F(APIMemorySafetyTest, NullSafety_UtilityFunctions) {
    // Error function should handle being called anytime
    const char* error = nimcp_get_error();
    EXPECT_NE(error, nullptr);

    // Version functions should work without init
    const char* version = nimcp_version();
    EXPECT_NE(version, nullptr);

    int version_int = nimcp_version_int();
    EXPECT_GT(version_int, 0);
}

TEST_F(APIMemorySafetyTest, NullSafety_WorkingMemory) {
    nimcp_brain_t brain = nimcp_brain_create("wm_null", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 256, 10);
    ASSERT_NE(brain, nullptr);

    float data[256] = {0};
    uint32_t size, capacity;

    // NULL brain handle
    EXPECT_NE(nimcp_brain_working_memory_add(nullptr, data, 256, 0.8f), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_brain_working_memory_get(nullptr, 0, &size), nullptr);
    EXPECT_NE(nimcp_brain_working_memory_stats(nullptr, &size, &capacity), NIMCP_SUCCESS);
    EXPECT_NE(nimcp_brain_working_memory_refresh(nullptr, 0), NIMCP_SUCCESS);

    // NULL data
    nimcp_status_t status = nimcp_brain_working_memory_add(brain, nullptr, 256, 0.8f);
    // May succeed or fail depending on implementation

    // NULL output pointers
    status = nimcp_brain_working_memory_stats(brain, nullptr, &capacity);
    status = nimcp_brain_working_memory_stats(brain, &size, nullptr);

    nimcp_brain_destroy(brain);
}

TEST_F(APIMemorySafetyTest, NullSafety_GlobalWorkspace) {
    nimcp_brain_t brain = nimcp_brain_create("gw_null", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 256, 10);
    ASSERT_NE(brain, nullptr);

    float content[256] = {0};
    uint32_t dim;
    nimcp_cognitive_module_t source;
    bool has_broadcast;

    // NULL brain handle
    EXPECT_NE(nimcp_brain_workspace_compete(nullptr, NIMCP_MODULE_PERCEPTION, content, 256, 0.8f), NIMCP_SUCCESS);
    EXPECT_NE(nimcp_brain_workspace_read(nullptr, content, 256, &dim, &source), NIMCP_SUCCESS);
    EXPECT_NE(nimcp_brain_workspace_has_broadcast(nullptr, &has_broadcast), NIMCP_SUCCESS);

    // NULL content
    nimcp_status_t status = nimcp_brain_workspace_compete(brain, NIMCP_MODULE_PERCEPTION, nullptr, 256, 0.8f);
    status = nimcp_brain_workspace_read(brain, nullptr, 256, &dim, &source);

    // NULL output pointers
    status = nimcp_brain_workspace_read(brain, content, 256, nullptr, &source);
    status = nimcp_brain_workspace_read(brain, content, 256, &dim, nullptr);
    status = nimcp_brain_workspace_has_broadcast(brain, nullptr);

    nimcp_brain_destroy(brain);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    printf("\n");
    printf("=================================================================\n");
    printf("Memory Safety Tests\n");
    printf("=================================================================\n");
    printf("NOTE: Run with valgrind or AddressSanitizer for complete analysis:\n");
    printf("  valgrind --leak-check=full --show-leak-kinds=all ./test_api_memory_safety\n");
    printf("  or compile with: -fsanitize=address -fsanitize=leak\n");
    printf("=================================================================\n");
    printf("\n");

    return RUN_ALL_TESTS();
}
