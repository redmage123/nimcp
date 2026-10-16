/**
 * @file test_kg_module_wiring_e2e.cpp
 * @brief End-to-End tests for KG Module Wiring System
 * @version 1.0.0
 * @date 2026-01-16
 *
 * Tests complete workflows from module wiring creation through:
 * - Brain KG node/edge creation
 * - Message handler registration
 * - Module lifecycle state transitions
 * - Wiring diagram synchronization
 * - Full brain self-awareness scenarios
 */

#include <gtest/gtest.h>
#include <cstring>
#include <string>
#include <vector>
#include <chrono>
#include <unordered_map>

extern "C" {
#include "core/brain/nimcp_kg_module_wiring.h"
}

//=============================================================================
// Mock Infrastructure for E2E Testing
//=============================================================================

/**
 * @brief Simulated Brain KG with full operation tracking
 */
class MockBrainKGFull {
public:
    struct Node {
        std::string name;
        std::string type;
        std::string description;
        uint32_t state;
        void* module_ptr;
    };

    struct Edge {
        uint32_t from;
        uint32_t to;
        std::string type;
        float weight;
        std::string description;
    };

    struct Handler {
        uint32_t node_id;
        std::string message_type;
        uint32_t priority;
    };

    struct Operation {
        std::string op;
        std::string details;
        uint64_t timestamp;
    };

    std::vector<Node> nodes;
    std::vector<Edge> edges;
    std::vector<Handler> handlers;
    std::vector<Operation> operations;

    uint32_t addNode(const std::string& name, const std::string& type, const std::string& desc) {
        uint32_t id = static_cast<uint32_t>(nodes.size());
        nodes.push_back({name, type, desc, 0, nullptr});
        logOperation("ADD_NODE", "name=" + name + ", type=" + type);
        return id;
    }

    uint32_t findNode(const std::string& name) const {
        for (size_t i = 0; i < nodes.size(); i++) {
            if (nodes[i].name == name) {
                return static_cast<uint32_t>(i);
            }
        }
        return UINT32_MAX;
    }

    uint32_t addEdge(uint32_t from, uint32_t to, const std::string& type, float weight, const std::string& desc) {
        if (from >= nodes.size() || to >= nodes.size()) return UINT32_MAX;
        uint32_t id = static_cast<uint32_t>(edges.size());
        edges.push_back({from, to, type, weight, desc});
        logOperation("ADD_EDGE", "from=" + std::to_string(from) + ", to=" + std::to_string(to));
        return id;
    }

    void addHandler(uint32_t node_id, const std::string& msg_type, uint32_t priority) {
        handlers.push_back({node_id, msg_type, priority});
        logOperation("ADD_HANDLER", "node=" + std::to_string(node_id) + ", msg=" + msg_type);
    }

    void updateState(uint32_t node_id, uint32_t state) {
        if (node_id < nodes.size()) {
            nodes[node_id].state = state;
            logOperation("UPDATE_STATE", "node=" + std::to_string(node_id) + ", state=" + std::to_string(state));
        }
    }

    void logOperation(const std::string& op, const std::string& details) {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        operations.push_back({op, details, static_cast<uint64_t>(ms)});
    }

    // Query helpers
    uint32_t countEdgesTo(uint32_t node_id) const {
        uint32_t count = 0;
        for (const auto& edge : edges) {
            if (edge.to == node_id) count++;
        }
        return count;
    }

    uint32_t countEdgesFrom(uint32_t node_id) const {
        uint32_t count = 0;
        for (const auto& edge : edges) {
            if (edge.from == node_id) count++;
        }
        return count;
    }

    uint32_t countHandlersForNode(uint32_t node_id) const {
        uint32_t count = 0;
        for (const auto& h : handlers) {
            if (h.node_id == node_id) count++;
        }
        return count;
    }
};

/**
 * @brief Wiring registry for E2E tests
 */
class WiringRegistry {
public:
    struct Entry {
        std::string name;
        kg_module_wiring_t* wiring;
        bool synced;
    };

    std::vector<Entry> entries;

    void add(const std::string& name, kg_module_wiring_t* wiring) {
        entries.push_back({name, wiring, false});
    }

    kg_module_wiring_t* find(const std::string& name) {
        for (auto& e : entries) {
            if (e.name == name) return e.wiring;
        }
        return nullptr;
    }

    void markSynced(kg_module_wiring_t* wiring) {
        for (auto& e : entries) {
            if (e.wiring == wiring) {
                e.synced = true;
                break;
            }
        }
    }

    void clear() {
        for (auto& e : entries) {
            if (e.wiring) {
                kg_module_wiring_destroy(e.wiring);
            }
        }
        entries.clear();
    }
};

//=============================================================================
// Test Fixture
//=============================================================================

class KGModuleWiringE2ETest : public ::testing::Test {
protected:
    MockBrainKGFull kg_;
    WiringRegistry registry_;

    void TearDown() override {
        registry_.clear();
    }

    /**
     * @brief Sync a module wiring to the mock brain KG
     */
    int syncWiringToKG(kg_module_wiring_t* wiring) {
        if (!wiring) return -1;

        // Create module node if not exists
        uint32_t module_node = kg_.findNode(wiring->module_name);
        if (module_node == UINT32_MAX) {
            module_node = kg_.addNode(wiring->module_name, wiring->module_type, "Module wiring");
        }

        // Sync inputs as edges
        for (uint32_t i = 0; i < wiring->input_count; i++) {
            const kg_input_connection_t& input = wiring->inputs[i];
            uint32_t source = kg_.findNode(input.source_module);
            if (source == UINT32_MAX) {
                source = kg_.addNode(input.source_module, "INPUT_SOURCE", "Input source module");
            }
            kg_.addEdge(source, module_node, "INPUT", 1.0f, input.message_type);
        }

        // Sync outputs
        for (uint32_t i = 0; i < wiring->output_count; i++) {
            const kg_output_connection_t& output = wiring->outputs[i];
            // Create a virtual output sink for each output type
            std::string sink_name = std::string("sink_") + output.message_type;
            uint32_t sink = kg_.findNode(sink_name);
            if (sink == UINT32_MAX) {
                sink = kg_.addNode(sink_name, "OUTPUT_SINK", output.description);
            }
            kg_.addEdge(module_node, sink, "OUTPUT", 1.0f, output.message_type);
        }

        // Sync handlers
        for (uint32_t i = 0; i < wiring->handler_count; i++) {
            const kg_handler_registration_t& handler = wiring->handlers[i];
            kg_.addHandler(module_node, handler.message_type, handler.priority);
        }

        registry_.markSynced(wiring);
        kg_.logOperation("SYNC_WIRING", wiring->module_name);

        return 0;
    }

    /**
     * @brief Create and register a wiring
     */
    kg_module_wiring_t* createWiring(const std::string& name, const std::string& type) {
        kg_module_wiring_t* wiring = kg_module_wiring_create(name.c_str(), type.c_str());
        if (wiring) {
            registry_.add(name, wiring);
        }
        return wiring;
    }
};

//=============================================================================
// E2E TEST SUITE: Complete Module Wiring Lifecycle
//=============================================================================

/**
 * @test Complete lifecycle: Create -> Configure -> Validate -> Sync
 */
TEST_F(KGModuleWiringE2ETest, CompleteModuleLifecycle) {
    // Phase 1: Create module wiring
    kg_module_wiring_t* wiring = createWiring("prefrontal_cortex", "COGNITIVE");
    ASSERT_NE(wiring, nullptr);

    // Phase 2: Configure connections
    ASSERT_EQ(kg_module_wiring_add_input(wiring, "thalamus", "SENSORY_INPUT", true), 0);
    ASSERT_EQ(kg_module_wiring_add_input(wiring, "basal_ganglia", "REWARD_SIGNAL", false), 0);
    ASSERT_EQ(kg_module_wiring_add_output(wiring, "DECISION_OUT", "Executive decisions"), 0);
    ASSERT_EQ(kg_module_wiring_add_output(wiring, "WORKING_MEM", "Working memory updates"), 0);
    ASSERT_EQ(kg_module_wiring_add_handler(wiring, "ATTENTION_QUERY", 100), 0);
    ASSERT_EQ(kg_module_wiring_add_handler(wiring, "DECISION_REQUEST", 80), 0);

    // Phase 3: Set metadata
    ASSERT_EQ(kg_module_wiring_set_metadata(wiring, "NIMCP Team", "Executive", "Prefrontal cortex module"), 0);
    ASSERT_EQ(kg_module_wiring_set_version(wiring, 1, 0, 0), 0);

    // Phase 4: Validate configuration
    char error_buf[256] = {0};
    int result = kg_module_wiring_validate(wiring, error_buf, sizeof(error_buf));
    EXPECT_EQ(result, 0) << "Validation failed: " << error_buf;

    // Phase 5: Sync to Brain KG
    ASSERT_EQ(syncWiringToKG(wiring), 0);

    // Verify KG state
    EXPECT_GE(kg_.nodes.size(), 5u);  // PFC + 2 inputs + 2 output sinks
    EXPECT_EQ(kg_.edges.size(), 4u);   // 2 input + 2 output edges
    EXPECT_EQ(kg_.handlers.size(), 2u);

    // Verify PFC node exists and has correct connections
    uint32_t pfc_node = kg_.findNode("prefrontal_cortex");
    ASSERT_NE(pfc_node, UINT32_MAX);
    EXPECT_EQ(kg_.countEdgesTo(pfc_node), 2u);    // 2 inputs
    EXPECT_EQ(kg_.countEdgesFrom(pfc_node), 2u);  // 2 outputs
    EXPECT_EQ(kg_.countHandlersForNode(pfc_node), 2u);

    // Verify operation log
    EXPECT_GT(kg_.operations.size(), 5u);
}

/**
 * @test Multi-module brain subsystem wiring
 */
TEST_F(KGModuleWiringE2ETest, MultiModuleSubsystem) {
    // Create multiple interconnected modules
    std::vector<std::string> modules = {
        "visual_cortex",
        "auditory_cortex",
        "somatosensory_cortex",
        "prefrontal_cortex",
        "hippocampus"
    };

    std::vector<kg_module_wiring_t*> wirings;

    // Create all modules
    for (const auto& name : modules) {
        kg_module_wiring_t* w = createWiring(name, "CORTICAL");
        ASSERT_NE(w, nullptr);
        wirings.push_back(w);
    }

    // Configure sensory cortex outputs to PFC
    for (int i = 0; i < 3; i++) {
        kg_module_wiring_add_output(wirings[i], "SENSORY_OUT", "Sensory output");
    }

    // Configure PFC inputs from sensory areas
    kg_module_wiring_add_input(wirings[3], "visual_cortex", "SENSORY_OUT", true);
    kg_module_wiring_add_input(wirings[3], "auditory_cortex", "SENSORY_OUT", true);
    kg_module_wiring_add_input(wirings[3], "somatosensory_cortex", "SENSORY_OUT", false);

    // Configure PFC <-> Hippocampus bidirectional
    kg_module_wiring_add_output(wirings[3], "MEMORY_QUERY", "Memory retrieval query");
    kg_module_wiring_add_input(wirings[3], "hippocampus", "MEMORY_RESULT", true);
    kg_module_wiring_add_output(wirings[4], "MEMORY_RESULT", "Memory retrieval result");
    kg_module_wiring_add_input(wirings[4], "prefrontal_cortex", "MEMORY_QUERY", true);

    // Sync all to KG
    for (auto* w : wirings) {
        ASSERT_EQ(syncWiringToKG(w), 0);
    }

    // Verify complete topology
    // 5 modules + 3 unique output sinks (SENSORY_OUT, MEMORY_QUERY, MEMORY_RESULT)
    EXPECT_EQ(kg_.nodes.size(), 5u + 3u);

    // Count edges to PFC
    uint32_t pfc_node = kg_.findNode("prefrontal_cortex");
    ASSERT_NE(pfc_node, UINT32_MAX);
    EXPECT_EQ(kg_.countEdgesTo(pfc_node), 4u);    // 3 sensory + hippocampus
    EXPECT_EQ(kg_.countEdgesFrom(pfc_node), 2u);  // 2 outputs (SENSORY_OUT sink + MEMORY_QUERY sink)
}

/**
 * @test Dynamic reconfiguration scenario
 */
TEST_F(KGModuleWiringE2ETest, DynamicReconfiguration) {
    // Initial configuration
    kg_module_wiring_t* wiring = createWiring("adaptive_module", "PLASTIC");
    ASSERT_NE(wiring, nullptr);

    kg_module_wiring_add_input(wiring, "source_a", "MSG_A", true);
    kg_module_wiring_add_output(wiring, "OUT_X", "Output X");

    syncWiringToKG(wiring);
    uint32_t initial_edge_count = static_cast<uint32_t>(kg_.edges.size());

    // Simulate learning: add new connections
    kg_module_wiring_add_input(wiring, "source_b", "MSG_B", true);
    kg_module_wiring_add_input(wiring, "source_c", "MSG_C", false);
    kg_module_wiring_add_output(wiring, "OUT_Y", "Output Y");

    // Re-sync to KG
    syncWiringToKG(wiring);

    // Verify new topology (new edges added)
    EXPECT_GT(kg_.edges.size(), initial_edge_count);

    // Verify new connections in wiring
    EXPECT_EQ(wiring->input_count, 3u);
    EXPECT_EQ(wiring->output_count, 2u);
}

/**
 * @test Message handler priority ordering in brain KG
 */
TEST_F(KGModuleWiringE2ETest, HandlerPriorityOrdering) {
    kg_module_wiring_t* wiring = createWiring("handler_module", "PROCESSING");
    ASSERT_NE(wiring, nullptr);

    // Add handlers with various priorities
    ASSERT_EQ(kg_module_wiring_add_handler(wiring, "LOW", 10), 0);
    ASSERT_EQ(kg_module_wiring_add_handler(wiring, "MEDIUM", 50), 0);
    ASSERT_EQ(kg_module_wiring_add_handler(wiring, "HIGH", 100), 0);
    ASSERT_EQ(kg_module_wiring_add_handler(wiring, "CRITICAL", 200), 0);

    syncWiringToKG(wiring);

    // Verify all handlers registered
    EXPECT_EQ(kg_.handlers.size(), 4u);

    // Verify priorities via query API
    EXPECT_EQ(kg_module_wiring_get_handler_priority(wiring, "LOW"), 10u);
    EXPECT_EQ(kg_module_wiring_get_handler_priority(wiring, "MEDIUM"), 50u);
    EXPECT_EQ(kg_module_wiring_get_handler_priority(wiring, "HIGH"), 100u);
    EXPECT_EQ(kg_module_wiring_get_handler_priority(wiring, "CRITICAL"), 200u);
}

/**
 * @test Brain self-awareness query simulation
 */
TEST_F(KGModuleWiringE2ETest, BrainSelfAwareness) {
    // Build a small brain topology
    std::vector<std::pair<std::string, std::string>> brain_modules = {
        {"thalamus", "RELAY"},
        {"visual_cortex", "PERCEPTION"},
        {"prefrontal_cortex", "EXECUTIVE"},
        {"hippocampus", "MEMORY"},
        {"amygdala", "EMOTION"}
    };

    for (const auto& [name, type] : brain_modules) {
        kg_module_wiring_t* w = createWiring(name, type);
        ASSERT_NE(w, nullptr);

        // Add handler for its own message type
        std::string handler_type = name + "_MSG";
        kg_module_wiring_add_handler(w, handler_type.c_str(), 50);

        syncWiringToKG(w);
    }

    // Simulate self-awareness queries

    // Query 1: "What modules exist?"
    EXPECT_EQ(kg_.nodes.size(), 5u);

    // Query 2: "What handlers does visual_cortex have?"
    uint32_t visual_node = kg_.findNode("visual_cortex");
    ASSERT_NE(visual_node, UINT32_MAX);
    EXPECT_EQ(kg_.countHandlersForNode(visual_node), 1u);

    // Query 3: "Find handler for visual_cortex_MSG"
    bool found = false;
    for (const auto& h : kg_.handlers) {
        if (h.message_type == "visual_cortex_MSG") {
            found = true;
            EXPECT_EQ(h.node_id, visual_node);
            break;
        }
    }
    EXPECT_TRUE(found);

    // Query 4: "What's the module type of prefrontal_cortex?"
    uint32_t pfc_node = kg_.findNode("prefrontal_cortex");
    ASSERT_NE(pfc_node, UINT32_MAX);
    EXPECT_EQ(kg_.nodes[pfc_node].type, "EXECUTIVE");
}

/**
 * @test Performance under load
 */
TEST_F(KGModuleWiringE2ETest, PerformanceUnderLoad) {
    const int MODULE_COUNT = 50;
    const int INPUTS_PER_MODULE = 5;
    const int HANDLERS_PER_MODULE = 3;

    auto start = std::chrono::high_resolution_clock::now();

    // Create many modules with connections
    for (int m = 0; m < MODULE_COUNT; m++) {
        std::string name = "perf_module_" + std::to_string(m);
        kg_module_wiring_t* w = createWiring(name, "PERF_TEST");
        ASSERT_NE(w, nullptr);

        // Add inputs
        for (int i = 0; i < INPUTS_PER_MODULE; i++) {
            std::string src = "src_" + std::to_string(m) + "_" + std::to_string(i);
            std::string msg = "MSG_" + std::to_string(i);
            kg_module_wiring_add_input(w, src.c_str(), msg.c_str(), true);
        }

        // Add handlers
        for (int h = 0; h < HANDLERS_PER_MODULE; h++) {
            std::string msg = "HANDLER_" + std::to_string(m) + "_" + std::to_string(h);
            kg_module_wiring_add_handler(w, msg.c_str(), static_cast<uint32_t>(h * 10));
        }

        syncWiringToKG(w);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Verify all created
    EXPECT_EQ(registry_.entries.size(), static_cast<size_t>(MODULE_COUNT));
    EXPECT_EQ(kg_.handlers.size(), static_cast<size_t>(MODULE_COUNT * HANDLERS_PER_MODULE));

    std::cout << "\n  [E2E Performance] Created and synced " << MODULE_COUNT << " modules in "
              << duration << " ms" << std::endl;
    std::cout << "  [E2E Performance] Total nodes: " << kg_.nodes.size()
              << ", edges: " << kg_.edges.size()
              << ", handlers: " << kg_.handlers.size() << std::endl;

    // Performance assertion: should complete in < 5 seconds
    EXPECT_LT(duration, 5000);
}

/**
 * @test Weight registration and type tracking
 */
TEST_F(KGModuleWiringE2ETest, WeightRegistration) {
    kg_module_wiring_t* wiring = createWiring("neural_module", "SNN");
    ASSERT_NE(wiring, nullptr);

    // Create mock weights
    float snn_weights[64];
    for (int i = 0; i < 64; i++) {
        snn_weights[i] = static_cast<float>(i) / 64.0f;
    }

    // Register weights
    ASSERT_EQ(kg_module_wiring_set_weights(wiring, KG_WEIGHT_SNN, snn_weights, sizeof(snn_weights)), 0);

    // Verify
    EXPECT_EQ(wiring->network_type, KG_WEIGHT_SNN);
    EXPECT_NE(wiring->initial_weights, nullptr);
    EXPECT_EQ(wiring->weights_size, sizeof(snn_weights));

    // Verify weight type string conversion
    EXPECT_STREQ(kg_weight_type_to_string(wiring->network_type), "SNN");
}

/**
 * @test Version tracking across modules
 */
TEST_F(KGModuleWiringE2ETest, VersionTracking) {
    // Create modules with different versions
    kg_module_wiring_t* v1 = createWiring("module_v1", "VERSIONED");
    kg_module_wiring_t* v2 = createWiring("module_v2", "VERSIONED");

    ASSERT_NE(v1, nullptr);
    ASSERT_NE(v2, nullptr);

    ASSERT_EQ(kg_module_wiring_set_version(v1, 1, 0, 0), 0);
    ASSERT_EQ(kg_module_wiring_set_version(v2, 2, 1, 3), 0);

    // Versions should be different
    EXPECT_NE(v1->version, v2->version);

    // Sync both
    syncWiringToKG(v1);
    syncWiringToKG(v2);

    EXPECT_EQ(kg_.nodes.size(), 2u);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
