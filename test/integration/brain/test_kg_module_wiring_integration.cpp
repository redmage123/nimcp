/**
 * @file test_kg_module_wiring_integration.cpp
 * @brief Integration tests for KG Module Wiring with Brain KG and Bio-Async
 * @version 1.0.0
 * @date 2026-01-16
 *
 * Tests the integration of kg_module_wiring with:
 * - Brain KG (node/edge creation, message handler registration)
 * - Bio-async orchestrator (module registration and wiring)
 * - Wiring diagram sync (bidirectional handler sync)
 */

#include <gtest/gtest.h>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
#include "core/brain/nimcp_kg_module_wiring.h"
}

//=============================================================================
// Mock Infrastructure for Integration Testing
//=============================================================================

// Mock brain_kg for standalone testing
struct MockBrainKG {
    struct Node {
        std::string name;
        std::string description;
        uint32_t type;
        bool in_use;
    };
    std::vector<Node> nodes;

    struct Edge {
        uint32_t from;
        uint32_t to;
        uint32_t type;
        float weight;
    };
    std::vector<Edge> edges;

    struct Handler {
        uint32_t node_id;
        std::string message_type;
        uint32_t priority;
    };
    std::vector<Handler> handlers;

    uint32_t addNode(const std::string& name, uint32_t type, const std::string& desc) {
        uint32_t id = static_cast<uint32_t>(nodes.size());
        nodes.push_back({name, desc, type, true});
        return id;
    }

    uint32_t findNode(const std::string& name) const {
        for (size_t i = 0; i < nodes.size(); i++) {
            if (nodes[i].in_use && nodes[i].name == name) {
                return static_cast<uint32_t>(i);
            }
        }
        return UINT32_MAX;
    }

    uint32_t addEdge(uint32_t from, uint32_t to, uint32_t type, float weight) {
        uint32_t id = static_cast<uint32_t>(edges.size());
        edges.push_back({from, to, type, weight});
        return id;
    }

    void addHandler(uint32_t node_id, const std::string& msg_type, uint32_t priority) {
        handlers.push_back({node_id, msg_type, priority});
    }
};

//=============================================================================
// Test Fixture
//=============================================================================

class KGModuleWiringIntegrationTest : public ::testing::Test {
protected:
    kg_module_wiring_t* wiring_ = nullptr;
    MockBrainKG mockKG_;

    void SetUp() override {
        wiring_ = kg_module_wiring_create("test_integration_module", "COGNITIVE");
        ASSERT_NE(wiring_, nullptr);
    }

    void TearDown() override {
        if (wiring_) {
            kg_module_wiring_destroy(wiring_);
            wiring_ = nullptr;
        }
    }

    // Helper to sync wiring to mock KG
    int syncWiringToKG() {
        if (!wiring_) return -1;

        // Create module node
        uint32_t module_node = mockKG_.addNode(
            wiring_->module_name,
            0,  // BRAIN_KG_NODE_CORE
            "Test module"
        );

        // Sync inputs as edges
        for (uint32_t i = 0; i < wiring_->input_count; i++) {
            const kg_input_connection_t& input = wiring_->inputs[i];
            uint32_t source_node = mockKG_.findNode(input.source_module);
            if (source_node == UINT32_MAX) {
                source_node = mockKG_.addNode(input.source_module, 0, "Input source");
            }
            mockKG_.addEdge(source_node, module_node, 0, 1.0f);
        }

        // Sync outputs
        for (uint32_t i = 0; i < wiring_->output_count; i++) {
            // Create edge to "output_sink" for each output
            uint32_t sink = mockKG_.findNode("output_sink");
            if (sink == UINT32_MAX) {
                sink = mockKG_.addNode("output_sink", 0, "Output sink");
            }
            mockKG_.addEdge(module_node, sink, 0, 1.0f);
        }

        // Sync handlers
        for (uint32_t i = 0; i < wiring_->handler_count; i++) {
            const kg_handler_registration_t& handler = wiring_->handlers[i];
            mockKG_.addHandler(module_node, handler.message_type, handler.priority);
        }

        return 0;
    }
};

//=============================================================================
// INTEGRATION TEST SUITE: Module Wiring to Brain KG Sync
//=============================================================================

/**
 * @test Create wiring descriptor and sync to mock KG
 */
TEST_F(KGModuleWiringIntegrationTest, WiringToKGNodeSync) {
    // Configure the wiring
    ASSERT_EQ(kg_module_wiring_add_input(wiring_, "prefrontal_cortex", "MEMORY_QUERY", true), 0);
    ASSERT_EQ(kg_module_wiring_add_output(wiring_, "DECISION_OUT", "Executive decisions"), 0);
    ASSERT_EQ(kg_module_wiring_add_handler(wiring_, "ATTENTION_QUERY", 100), 0);

    // Sync to mock KG
    ASSERT_EQ(syncWiringToKG(), 0);

    // Verify nodes created
    EXPECT_GE(mockKG_.nodes.size(), 2u);  // module + input source

    // Verify handler registered
    EXPECT_EQ(mockKG_.handlers.size(), 1u);
    EXPECT_EQ(mockKG_.handlers[0].message_type, "ATTENTION_QUERY");
    EXPECT_EQ(mockKG_.handlers[0].priority, 100u);
}

/**
 * @test Multiple inputs/outputs sync to KG edges
 */
TEST_F(KGModuleWiringIntegrationTest, MultipleConnectionsSync) {
    // Add multiple inputs
    ASSERT_EQ(kg_module_wiring_add_input(wiring_, "source_a", "MSG_A", true), 0);
    ASSERT_EQ(kg_module_wiring_add_input(wiring_, "source_b", "MSG_B", false), 0);
    ASSERT_EQ(kg_module_wiring_add_input(wiring_, "source_c", "MSG_C", true), 0);

    // Add multiple outputs
    ASSERT_EQ(kg_module_wiring_add_output(wiring_, "OUT_X", "Output X"), 0);
    ASSERT_EQ(kg_module_wiring_add_output(wiring_, "OUT_Y", "Output Y"), 0);

    // Verify internal counts
    EXPECT_EQ(wiring_->input_count, 3u);
    EXPECT_EQ(wiring_->output_count, 2u);

    // Sync to KG
    ASSERT_EQ(syncWiringToKG(), 0);

    // Verify edges: 3 input edges + 2 output edges
    EXPECT_GE(mockKG_.edges.size(), 5u);
}

/**
 * @test Handler registration with priorities
 */
TEST_F(KGModuleWiringIntegrationTest, HandlerPrioritiesSync) {
    // Add handlers with different priorities
    ASSERT_EQ(kg_module_wiring_add_handler(wiring_, "LOW_PRIORITY", 1), 0);
    ASSERT_EQ(kg_module_wiring_add_handler(wiring_, "MEDIUM_PRIORITY", 50), 0);
    ASSERT_EQ(kg_module_wiring_add_handler(wiring_, "HIGH_PRIORITY", 100), 0);

    EXPECT_EQ(wiring_->handler_count, 3u);

    // Sync to KG
    ASSERT_EQ(syncWiringToKG(), 0);

    // Verify all handlers registered with correct priorities
    EXPECT_EQ(mockKG_.handlers.size(), 3u);

    // Find high priority handler
    bool found_high = false;
    for (const auto& h : mockKG_.handlers) {
        if (h.message_type == "HIGH_PRIORITY") {
            EXPECT_EQ(h.priority, 100u);
            found_high = true;
        }
    }
    EXPECT_TRUE(found_high);
}

/**
 * @test Validation before KG sync
 */
TEST_F(KGModuleWiringIntegrationTest, ValidationBeforeSync) {
    // Empty wiring should still be valid (name and type set)
    char error_buf[256] = {0};
    int result = kg_module_wiring_validate(wiring_, error_buf, sizeof(error_buf));
    EXPECT_EQ(result, 0) << "Validation error: " << error_buf;

    // Add connections
    ASSERT_EQ(kg_module_wiring_add_input(wiring_, "source", "MSG_TYPE", true), 0);
    ASSERT_EQ(kg_module_wiring_add_output(wiring_, "OUTPUT", "Description"), 0);

    // Validate again
    result = kg_module_wiring_validate(wiring_, error_buf, sizeof(error_buf));
    EXPECT_EQ(result, 0) << "Validation error: " << error_buf;
    EXPECT_EQ(wiring_->input_count, 1u);
    EXPECT_EQ(wiring_->output_count, 1u);
}

/**
 * @test Metadata propagation
 */
TEST_F(KGModuleWiringIntegrationTest, MetadataPropagation) {
    // Set metadata
    ASSERT_EQ(kg_module_wiring_set_metadata(wiring_, "TestAuthor", "TestCategory", "Test description"), 0);

    // Add custom metadata entry
    ASSERT_EQ(kg_module_wiring_add_metadata_entry(wiring_, "custom_key", "custom_value"), 0);

    // Verify metadata
    EXPECT_STREQ(wiring_->metadata.author, "TestAuthor");
    EXPECT_STREQ(wiring_->metadata.category, "TestCategory");
    EXPECT_STREQ(wiring_->metadata.description, "Test description");

    // Verify custom entries
    EXPECT_EQ(wiring_->metadata.entry_count, 1u);
    EXPECT_STREQ(wiring_->metadata.entries[0].key, "custom_key");
    EXPECT_STREQ(wiring_->metadata.entries[0].value, "custom_value");
}

/**
 * @test Weight state registration
 */
TEST_F(KGModuleWiringIntegrationTest, WeightStateRegistration) {
    // Create some mock weights
    float weights[16] = {0.1f, 0.2f, 0.3f, 0.4f};

    ASSERT_EQ(kg_module_wiring_set_weights(wiring_, KG_WEIGHT_SNN, weights, sizeof(weights)), 0);

    EXPECT_EQ(wiring_->network_type, KG_WEIGHT_SNN);
    EXPECT_NE(wiring_->initial_weights, nullptr);
    EXPECT_EQ(wiring_->weights_size, sizeof(weights));
}

//=============================================================================
// INTEGRATION TEST SUITE: Query API
//=============================================================================

/**
 * @test Input/output/handler query functions
 */
TEST_F(KGModuleWiringIntegrationTest, QueryFunctions) {
    // Configure wiring
    ASSERT_EQ(kg_module_wiring_add_input(wiring_, "source_module", "INPUT_MSG", true), 0);
    ASSERT_EQ(kg_module_wiring_add_output(wiring_, "OUTPUT_MSG", "Output description"), 0);
    ASSERT_EQ(kg_module_wiring_add_handler(wiring_, "HANDLER_MSG", 50), 0);

    // Test has_input
    EXPECT_TRUE(kg_module_wiring_has_input(wiring_, "source_module", "INPUT_MSG"));
    EXPECT_TRUE(kg_module_wiring_has_input(wiring_, "source_module", nullptr));  // Any msg from source
    EXPECT_FALSE(kg_module_wiring_has_input(wiring_, "nonexistent", "INPUT_MSG"));

    // Test has_output
    EXPECT_TRUE(kg_module_wiring_has_output(wiring_, "OUTPUT_MSG"));
    EXPECT_FALSE(kg_module_wiring_has_output(wiring_, "NONEXISTENT"));

    // Test has_handler
    EXPECT_TRUE(kg_module_wiring_has_handler(wiring_, "HANDLER_MSG"));
    EXPECT_FALSE(kg_module_wiring_has_handler(wiring_, "NONEXISTENT"));

    // Test get_handler_priority
    EXPECT_EQ(kg_module_wiring_get_handler_priority(wiring_, "HANDLER_MSG"), 50u);
    EXPECT_EQ(kg_module_wiring_get_handler_priority(wiring_, "NONEXISTENT"), 0u);
}

//=============================================================================
// INTEGRATION TEST SUITE: Version API
//=============================================================================

/**
 * @test Version setting and encoding
 */
TEST_F(KGModuleWiringIntegrationTest, VersionSetting) {
    ASSERT_EQ(kg_module_wiring_set_version(wiring_, 2, 5, 3), 0);

    // Version should be encoded in wiring_->version field
    EXPECT_NE(wiring_->version, 0u);
}

//=============================================================================
// INTEGRATION TEST SUITE: String Conversion
//=============================================================================

/**
 * @test Weight type string conversion
 */
TEST_F(KGModuleWiringIntegrationTest, WeightTypeStrings) {
    EXPECT_STREQ(kg_weight_type_to_string(KG_WEIGHT_NONE), "NONE");
    EXPECT_STREQ(kg_weight_type_to_string(KG_WEIGHT_SNN), "SNN");
    EXPECT_STREQ(kg_weight_type_to_string(KG_WEIGHT_LNN), "LNN");
    EXPECT_STREQ(kg_weight_type_to_string(KG_WEIGHT_CNN), "CNN");
    EXPECT_STREQ(kg_weight_type_to_string(KG_WEIGHT_TRANSFORMER), "TRANSFORMER");

    // Parse back
    EXPECT_EQ(kg_weight_type_from_string("SNN"), KG_WEIGHT_SNN);
    EXPECT_EQ(kg_weight_type_from_string("snn"), KG_WEIGHT_SNN);  // Case insensitive
    EXPECT_EQ(kg_weight_type_from_string("INVALID"), KG_WEIGHT_NONE);
}

//=============================================================================
// INTEGRATION TEST SUITE: Error Handling
//=============================================================================

/**
 * @test NULL pointer handling
 */
TEST_F(KGModuleWiringIntegrationTest, NullPointerHandling) {
    // All functions should handle NULL gracefully
    kg_module_wiring_destroy(nullptr);  // Should not crash

    EXPECT_EQ(kg_module_wiring_add_input(nullptr, "src", "msg", true), -1);
    EXPECT_EQ(kg_module_wiring_add_output(nullptr, "msg", "desc"), -1);
    EXPECT_EQ(kg_module_wiring_add_handler(nullptr, "msg", 10), -1);

    EXPECT_FALSE(kg_module_wiring_has_input(nullptr, "src", "msg"));
    EXPECT_FALSE(kg_module_wiring_has_output(nullptr, "msg"));
    EXPECT_FALSE(kg_module_wiring_has_handler(nullptr, "msg"));
}

/**
 * @test Empty/NULL string parameters
 */
TEST_F(KGModuleWiringIntegrationTest, EmptyStringHandling) {
    // Create with empty name should fail
    kg_module_wiring_t* bad_wiring = kg_module_wiring_create("", "TYPE");
    EXPECT_EQ(bad_wiring, nullptr);

    // Create with NULL name should fail
    bad_wiring = kg_module_wiring_create(nullptr, "TYPE");
    EXPECT_EQ(bad_wiring, nullptr);

    // NULL source in add_input should fail
    EXPECT_EQ(kg_module_wiring_add_input(wiring_, nullptr, "msg", true), -1);

    // NULL msg_type in add_output should fail
    EXPECT_EQ(kg_module_wiring_add_output(wiring_, nullptr, "desc"), -1);

    // NULL msg_type in add_handler should fail
    EXPECT_EQ(kg_module_wiring_add_handler(wiring_, nullptr, 10), -1);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
