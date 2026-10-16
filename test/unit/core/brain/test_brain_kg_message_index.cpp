/**
 * @file test_brain_kg_message_index.cpp
 * @brief Unit tests for Phase 6: KG Query Optimization - Message-Type Index
 *
 * Tests the efficient message-type handler lookup functionality added to brain_kg.
 */

#include <gtest/gtest.h>

#include "core/brain/nimcp_brain_kg.h"
#include "utils/memory/nimcp_memory.h"

class BrainKGMessageIndexTest : public ::testing::Test {
protected:
    brain_kg_t* kg;

    void SetUp() override {
        brain_kg_config_t config;
        brain_kg_default_config(&config);
        // Disable security for testing
        config.enable_security = false;
        config.enable_access_control = false;
        config.enable_integrity_checks = false;
        kg = brain_kg_create(&config);
        ASSERT_NE(kg, nullptr);
    }

    void TearDown() override {
        if (kg) {
            brain_kg_destroy(kg);
            kg = nullptr;
        }
    }
};

// Test basic handler list creation and destruction
TEST_F(BrainKGMessageIndexTest, test_handler_list_destroy_null_safe) {
    brain_kg_handler_list_destroy(nullptr);  // Should not crash
}

// Test get handlers for non-existent message type
TEST_F(BrainKGMessageIndexTest, test_get_handlers_empty) {
    brain_kg_handler_list_t* handlers = brain_kg_get_handlers_for_message_type(kg, 0x1234);
    ASSERT_NE(handlers, nullptr);
    EXPECT_EQ(handlers->count, 0u);
    brain_kg_handler_list_destroy(handlers);
}

// Test add single message handler
TEST_F(BrainKGMessageIndexTest, test_add_single_handler) {
    // Create a module node first
    brain_kg_node_id_t node_id = brain_kg_add_node(kg, "test_module",
        BRAIN_KG_NODE_COGNITIVE, "Test module for message handling");
    ASSERT_NE(node_id, BRAIN_KG_INVALID_NODE);

    // Add message handler
    int result = brain_kg_add_message_handler(kg, node_id, 0x100);
    EXPECT_EQ(result, 0);

    // Verify handler is found
    brain_kg_handler_list_t* handlers = brain_kg_get_handlers_for_message_type(kg, 0x100);
    ASSERT_NE(handlers, nullptr);
    EXPECT_EQ(handlers->count, 1u);
    EXPECT_EQ(handlers->handlers[0], node_id);
    brain_kg_handler_list_destroy(handlers);
}

// Test add multiple handlers for same message type
TEST_F(BrainKGMessageIndexTest, test_add_multiple_handlers_same_message) {
    // Create multiple modules
    brain_kg_node_id_t node1 = brain_kg_add_node(kg, "module1",
        BRAIN_KG_NODE_COGNITIVE, "First handler");
    brain_kg_node_id_t node2 = brain_kg_add_node(kg, "module2",
        BRAIN_KG_NODE_COGNITIVE, "Second handler");
    brain_kg_node_id_t node3 = brain_kg_add_node(kg, "module3",
        BRAIN_KG_NODE_COGNITIVE, "Third handler");

    ASSERT_NE(node1, BRAIN_KG_INVALID_NODE);
    ASSERT_NE(node2, BRAIN_KG_INVALID_NODE);
    ASSERT_NE(node3, BRAIN_KG_INVALID_NODE);

    // Add handlers for same message type
    uint32_t msg_type = 0x200;
    EXPECT_EQ(brain_kg_add_message_handler(kg, node1, msg_type), 0);
    EXPECT_EQ(brain_kg_add_message_handler(kg, node2, msg_type), 0);
    EXPECT_EQ(brain_kg_add_message_handler(kg, node3, msg_type), 0);

    // Verify all handlers found
    brain_kg_handler_list_t* handlers = brain_kg_get_handlers_for_message_type(kg, msg_type);
    ASSERT_NE(handlers, nullptr);
    EXPECT_EQ(handlers->count, 3u);
    brain_kg_handler_list_destroy(handlers);
}

// Test duplicate handler registration (should be idempotent)
TEST_F(BrainKGMessageIndexTest, test_duplicate_handler_registration) {
    brain_kg_node_id_t node_id = brain_kg_add_node(kg, "dup_module",
        BRAIN_KG_NODE_COGNITIVE, "Duplicate test");
    ASSERT_NE(node_id, BRAIN_KG_INVALID_NODE);

    uint32_t msg_type = 0x300;

    // Add same handler twice
    EXPECT_EQ(brain_kg_add_message_handler(kg, node_id, msg_type), 0);
    EXPECT_EQ(brain_kg_add_message_handler(kg, node_id, msg_type), 0);

    // Should only appear once
    brain_kg_handler_list_t* handlers = brain_kg_get_handlers_for_message_type(kg, msg_type);
    ASSERT_NE(handlers, nullptr);
    EXPECT_EQ(handlers->count, 1u);
    brain_kg_handler_list_destroy(handlers);
}

// Test remove message handler
TEST_F(BrainKGMessageIndexTest, test_remove_handler) {
    brain_kg_node_id_t node1 = brain_kg_add_node(kg, "rem_module1",
        BRAIN_KG_NODE_COGNITIVE, "To be removed");
    brain_kg_node_id_t node2 = brain_kg_add_node(kg, "rem_module2",
        BRAIN_KG_NODE_COGNITIVE, "Stays");

    uint32_t msg_type = 0x400;
    brain_kg_add_message_handler(kg, node1, msg_type);
    brain_kg_add_message_handler(kg, node2, msg_type);

    // Remove first handler
    EXPECT_EQ(brain_kg_remove_message_handler(kg, node1, msg_type), 0);

    // Verify only second handler remains
    brain_kg_handler_list_t* handlers = brain_kg_get_handlers_for_message_type(kg, msg_type);
    ASSERT_NE(handlers, nullptr);
    EXPECT_EQ(handlers->count, 1u);
    EXPECT_EQ(handlers->handlers[0], node2);
    brain_kg_handler_list_destroy(handlers);
}

// Test remove non-existent handler
TEST_F(BrainKGMessageIndexTest, test_remove_nonexistent_handler) {
    brain_kg_node_id_t node_id = brain_kg_add_node(kg, "ne_module",
        BRAIN_KG_NODE_COGNITIVE, "Test");

    // Try to remove handler that was never added
    EXPECT_EQ(brain_kg_remove_message_handler(kg, node_id, 0x500), -1);
}

// Test add handler for invalid node ID (BRAIN_KG_INVALID_NODE only)
// Note: Phase 7 relaxed validation - any valid ID is now accepted as a handler,
// whether or not a corresponding node exists (for bio_module_id support)
TEST_F(BrainKGMessageIndexTest, test_add_handler_invalid_node) {
    EXPECT_EQ(brain_kg_add_message_handler(kg, BRAIN_KG_INVALID_NODE, 0x600), -1);
    // Non-existent node IDs are now allowed (for bio_module_id support)
    EXPECT_EQ(brain_kg_add_message_handler(kg, 99999, 0x600), 0);
}

// Test get module handled messages (reverse lookup)
TEST_F(BrainKGMessageIndexTest, test_get_module_handled_messages) {
    brain_kg_node_id_t node_id = brain_kg_add_node(kg, "multi_handler",
        BRAIN_KG_NODE_COGNITIVE, "Handles multiple messages");

    // Register for multiple message types
    brain_kg_add_message_handler(kg, node_id, 0x700);
    brain_kg_add_message_handler(kg, node_id, 0x701);
    brain_kg_add_message_handler(kg, node_id, 0x702);

    // Query what messages this module handles
    uint32_t msg_types[10];
    uint32_t count = brain_kg_get_module_handled_messages(kg, node_id, msg_types, 10);

    EXPECT_EQ(count, 3u);
}

// Test invalidate and rebuild index
TEST_F(BrainKGMessageIndexTest, test_invalidate_and_rebuild) {
    brain_kg_node_id_t node_id = brain_kg_add_node(kg, "rebuild_module",
        BRAIN_KG_NODE_COGNITIVE, "For rebuild test");

    brain_kg_add_message_handler(kg, node_id, 0x800);

    // Verify handler exists before invalidation
    brain_kg_handler_list_t* pre_handlers = brain_kg_get_handlers_for_message_type(kg, 0x800);
    ASSERT_NE(pre_handlers, nullptr);
    EXPECT_EQ(pre_handlers->count, 1u);
    brain_kg_handler_list_destroy(pre_handlers);

    // Invalidate index - marks it dirty
    brain_kg_invalidate_message_index(kg);

    // Note: rebuild scans HANDLES_MESSAGE edges, not the in-memory index
    // Since we used the index API directly (not edges), rebuild finds nothing
    // This is correct behavior - rebuild reconstructs from edges

    // The index should still work for direct queries after invalidation
    // (until rebuild is triggered by a query)
    brain_kg_handler_list_t* handlers = brain_kg_get_handlers_for_message_type(kg, 0x800);
    ASSERT_NE(handlers, nullptr);
    // After rebuild from edges (which is empty), handlers won't be found
    // This tests that rebuild correctly clears and rebuilds from edges
    EXPECT_EQ(handlers->count, 0u);  // Correct: no edges exist
    brain_kg_handler_list_destroy(handlers);
}

// Test rebuild from HANDLES_MESSAGE edges
TEST_F(BrainKGMessageIndexTest, test_explicit_rebuild) {
    brain_kg_node_id_t node1 = brain_kg_add_node(kg, "rebuild1", BRAIN_KG_NODE_COGNITIVE, "");
    brain_kg_node_id_t node2 = brain_kg_add_node(kg, "rebuild2", BRAIN_KG_NODE_COGNITIVE, "");

    // Add handlers via the index API (no edges created)
    brain_kg_add_message_handler(kg, node1, 0x900);
    brain_kg_add_message_handler(kg, node2, 0x901);
    brain_kg_add_message_handler(kg, node1, 0x901);

    // Verify handlers exist in index
    brain_kg_handler_list_t* handlers = brain_kg_get_handlers_for_message_type(kg, 0x901);
    ASSERT_NE(handlers, nullptr);
    EXPECT_EQ(handlers->count, 2u);  // node1 and node2 both handle 0x901
    brain_kg_handler_list_destroy(handlers);

    // Explicit rebuild - scans edges, not in-memory index
    // Since no HANDLES_MESSAGE edges exist, rebuild returns 0
    int indexed = brain_kg_rebuild_message_index(kg);
    EXPECT_EQ(indexed, 0);  // No edges to rebuild from

    // After rebuild, index is cleared (was rebuilt from empty edge set)
    handlers = brain_kg_get_handlers_for_message_type(kg, 0x901);
    ASSERT_NE(handlers, nullptr);
    EXPECT_EQ(handlers->count, 0u);  // Cleared by rebuild
    brain_kg_handler_list_destroy(handlers);
}

// Test null parameters
TEST_F(BrainKGMessageIndexTest, test_null_parameters) {
    EXPECT_EQ(brain_kg_get_handlers_for_message_type(nullptr, 0x100), nullptr);
    EXPECT_EQ(brain_kg_add_message_handler(nullptr, 1, 0x100), -1);
    EXPECT_EQ(brain_kg_remove_message_handler(nullptr, 1, 0x100), -1);
    EXPECT_EQ(brain_kg_rebuild_message_index(nullptr), -1);

    uint32_t types[5];
    EXPECT_EQ(brain_kg_get_module_handled_messages(nullptr, 1, types, 5), 0u);
    EXPECT_EQ(brain_kg_get_module_handled_messages(kg, 1, nullptr, 5), 0u);
    EXPECT_EQ(brain_kg_get_module_handled_messages(kg, 1, types, 0), 0u);
}

// Test many message types (capacity)
TEST_F(BrainKGMessageIndexTest, test_many_message_types) {
    brain_kg_node_id_t node_id = brain_kg_add_node(kg, "many_msgs",
        BRAIN_KG_NODE_COGNITIVE, "Handles many messages");

    // Register for many message types
    for (uint32_t i = 0; i < 100; i++) {
        EXPECT_EQ(brain_kg_add_message_handler(kg, node_id, 0x1000 + i), 0);
    }

    // Verify a few
    for (uint32_t i = 0; i < 100; i += 10) {
        brain_kg_handler_list_t* handlers = brain_kg_get_handlers_for_message_type(kg, 0x1000 + i);
        ASSERT_NE(handlers, nullptr);
        EXPECT_EQ(handlers->count, 1u);
        brain_kg_handler_list_destroy(handlers);
    }
}

// Test many handlers per message type
TEST_F(BrainKGMessageIndexTest, test_many_handlers_per_message) {
    uint32_t msg_type = 0x2000;

    // Create many modules
    for (int i = 0; i < 50; i++) {
        char name[64];
        snprintf(name, sizeof(name), "handler_%d", i);
        brain_kg_node_id_t node_id = brain_kg_add_node(kg, name,
            BRAIN_KG_NODE_COGNITIVE, "Handler");
        ASSERT_NE(node_id, BRAIN_KG_INVALID_NODE);
        EXPECT_EQ(brain_kg_add_message_handler(kg, node_id, msg_type), 0);
    }

    // Verify all handlers found
    brain_kg_handler_list_t* handlers = brain_kg_get_handlers_for_message_type(kg, msg_type);
    ASSERT_NE(handlers, nullptr);
    EXPECT_EQ(handlers->count, 50u);
    brain_kg_handler_list_destroy(handlers);
}
