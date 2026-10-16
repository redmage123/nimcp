/**
 * @file test_wiring_diagram_integration.cpp
 * @brief Integration tests for KG-Based Wiring Diagram with Bio-Async Orchestrator
 *
 * Tests wiring diagram integration with:
 * - Bio-async orchestrator module discovery
 * - Bio-router handler registration
 * - Multi-module wiring scenarios
 * - Handler callback invocation
 * - Hardware profile-based wiring selection
 *
 * @author NIMCP Development Team
 * @date 2026-01-03
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <atomic>
#include <vector>
#include <fstream>
#include <filesystem>

#include "async/nimcp_wiring_diagram.h"
#include "async/nimcp_bio_async_orchestrator.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

//=============================================================================
// Test Fixture
//=============================================================================

class WiringDiagramIntegrationTest : public ::testing::Test {
protected:
    wiring_diagram_t* wd_ = nullptr;
    bio_async_orchestrator_t* orch_ = nullptr;
    std::string temp_dir_;

    void SetUp() override {
        // Initialize bio-async system
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        bio_config.enable_statistics = false;
        bio_config.enable_logging = false;
        nimcp_bio_async_init(&bio_config);

        // Initialize bio-router
        bio_router_config_t router_config = bio_router_default_config();
        router_config.enable_statistics = false;
        router_config.enable_logging = false;
        bio_router_init(&router_config);

        // Create temporary directory for test wiring files
        temp_dir_ = "/tmp/nimcp_wiring_integration_" + std::to_string(getpid());
        std::filesystem::create_directories(temp_dir_);
        std::filesystem::create_directories(temp_dir_ + "/subsystems");
        std::filesystem::create_directories(temp_dir_ + "/platforms");
        std::filesystem::create_directories(temp_dir_ + "/hardware");

        // Create wiring diagram
        wd_ = wiring_diagram_create(temp_dir_.c_str());
        ASSERT_NE(wd_, nullptr);
        wiring_diagram_set_auto_persist(wd_, false);

        // Create orchestrator
        bio_orchestrator_config_t orch_config;
        bio_orchestrator_default_config(&orch_config);
        orch_config.enable_statistics = false;
        orch_config.enable_auto_health_check = false;
        orch_ = bio_orchestrator_create(&orch_config);
        ASSERT_NE(orch_, nullptr);
    }

    void TearDown() override {
        if (orch_) {
            bio_orchestrator_destroy(orch_);
            orch_ = nullptr;
        }

        if (wd_) {
            wiring_diagram_destroy(wd_);
            wd_ = nullptr;
        }

        bio_router_shutdown();
        nimcp_bio_async_shutdown();

        // Clean up temp directory
        if (!temp_dir_.empty()) {
            std::filesystem::remove_all(temp_dir_);
        }
    }

    // Helper: Create test wiring file with modules
    void CreateTestWiringFile(const char* subsystem, int module_count) {
        std::string filename = temp_dir_ + "/subsystems/" + subsystem + ".jsonl";
        std::ofstream file(filename);

        for (int i = 0; i < module_count; i++) {
            std::string name = std::string(subsystem) + "_module_" + std::to_string(i);
            file << R"({"type":"entity","name":")" << name
                 << R"(","entityType":"CognitiveModule","subsystem":")" << subsystem
                 << R"(","min_tier":0,"enabled":true})" << "\n";
        }
        file.close();
    }

    // Helper: Create module with message handlers
    void CreateModuleWithHandlers(const char* name, const char* subsystem,
                                   const std::vector<int>& msg_types) {
        std::string filename = temp_dir_ + "/subsystems/" + subsystem + ".jsonl";
        std::ofstream file(filename, std::ios::app);

        // Write entity
        file << R"({"type":"entity","name":")" << name
             << R"(","entityType":"CognitiveModule","subsystem":")" << subsystem
             << R"(","min_tier":0,"enabled":true})" << "\n";

        // Write HANDLES_MESSAGE relations
        for (int msg_type : msg_types) {
            file << R"({"type":"relation","from":")" << name
                 << R"(","to":"MSG_)" << msg_type
                 << R"(","relationType":"HANDLES_MESSAGE","message_type":)" << msg_type
                 << "}\n";
        }
        file.close();
    }

    // Helper: Register module with orchestrator using correct API
    int RegisterModule(bio_module_id_t id, const char* name,
                       bio_module_category_t category, uint32_t phase) {
        return bio_orchestrator_register_module(
            orch_, id, name, category, nullptr, phase);
    }
};

//=============================================================================
// Orchestrator + Wiring Diagram Integration Tests
//=============================================================================

TEST_F(WiringDiagramIntegrationTest, OrchestratorSetsAndGetsWiringDiagram) {
    // Set wiring diagram on orchestrator
    int result = bio_orchestrator_set_wiring_diagram(orch_, wd_);
    EXPECT_EQ(result, 0);

    // Get it back
    wiring_diagram_t* retrieved = bio_orchestrator_get_wiring_diagram(orch_);
    EXPECT_EQ(retrieved, wd_);
}

TEST_F(WiringDiagramIntegrationTest, DiscoverWiringForRegisteredModules) {
    // Create wiring file with test modules
    CreateTestWiringFile("core", 3);

    // Load wiring
    ASSERT_EQ(wiring_diagram_load_subsystem(wd_, WIRING_SUBSYSTEM_CORE), 0);

    // Set wiring diagram on orchestrator
    ASSERT_EQ(bio_orchestrator_set_wiring_diagram(orch_, wd_), 0);

    // Register modules with orchestrator
    ASSERT_EQ(RegisterModule(BIO_MODULE_BRAIN, "core_module_0", BIO_MODULE_CATEGORY_CORE, 1), 0);
    ASSERT_EQ(RegisterModule(BIO_MODULE_ATTENTION, "core_module_1", BIO_MODULE_CATEGORY_CORE, 1), 0);

    // Discover wiring for all modules
    int discovered = bio_orchestrator_discover_all_wiring(orch_);
    EXPECT_GE(discovered, 1) << "Should discover wiring for at least 1 module";
}

TEST_F(WiringDiagramIntegrationTest, DiscoverWiringForSingleModule) {
    // Create wiring file
    CreateTestWiringFile("cognition", 2);
    ASSERT_EQ(wiring_diagram_load_subsystem(wd_, WIRING_SUBSYSTEM_COGNITION), 0);
    ASSERT_EQ(bio_orchestrator_set_wiring_diagram(orch_, wd_), 0);

    // Register a module
    ASSERT_EQ(RegisterModule(BIO_MODULE_MEMORY, "cognition_module_0", BIO_MODULE_CATEGORY_COGNITIVE, 2), 0);

    // Discover wiring for specific module
    int result = bio_orchestrator_discover_module_wiring(orch_, BIO_MODULE_MEMORY);
    EXPECT_EQ(result, 0) << "Should successfully discover wiring for registered module";
}

TEST_F(WiringDiagramIntegrationTest, HandlerCallbackInvocation) {
    // Create wiring file with handlers
    std::vector<int> msg_types = {
        BIO_MSG_BRAIN_STATE_QUERY,
        BIO_MSG_ATTENTION_SHIFT
    };
    CreateModuleWithHandlers("callback_test_module", "core", msg_types);

    ASSERT_EQ(wiring_diagram_load_subsystem(wd_, WIRING_SUBSYSTEM_CORE), 0);
    ASSERT_EQ(bio_orchestrator_set_wiring_diagram(orch_, wd_), 0);

    // Register module
    ASSERT_EQ(RegisterModule(BIO_MODULE_BRAIN, "callback_test_module", BIO_MODULE_CATEGORY_CORE, 1), 0);

    // Track callback invocations
    static std::atomic<int> callback_count{0};
    static std::atomic<int> handler_count{0};

    // Register handler callback
    auto callback = [](bio_module_context_t ctx,
                       const bio_message_type_t* msg_types,
                       uint32_t msg_count,
                       void* user_data) -> int {
        callback_count++;
        handler_count += msg_count;
        return 0;
    };

    ASSERT_EQ(bio_orchestrator_register_handler_callback(
        orch_, BIO_MODULE_BRAIN, callback, nullptr), 0);

    // Discover wiring
    bio_orchestrator_discover_all_wiring(orch_);

    // Invoke handler callbacks
    int result = bio_orchestrator_invoke_handler_callbacks(orch_);
    EXPECT_GE(result, 0);

    // Callback should have been invoked
    EXPECT_GE(callback_count.load(), 1) << "Handler callback should be invoked";
}

TEST_F(WiringDiagramIntegrationTest, MultiModuleWiringDiscovery) {
    // Create wiring for multiple subsystems
    CreateTestWiringFile("core", 5);
    CreateTestWiringFile("cognition", 5);
    CreateTestWiringFile("ethics", 3);

    // Load all subsystems
    ASSERT_EQ(wiring_diagram_load_all_subsystems(wd_), 0);
    ASSERT_EQ(bio_orchestrator_set_wiring_diagram(orch_, wd_), 0);

    // Register modules from each subsystem
    struct { bio_module_id_t id; const char* name; bio_module_category_t cat; uint32_t phase; } modules[] = {
        {BIO_MODULE_BRAIN, "core_module_0", BIO_MODULE_CATEGORY_CORE, 1},
        {BIO_MODULE_SYNAPSE, "core_module_1", BIO_MODULE_CATEGORY_CORE, 1},
        {BIO_MODULE_ATTENTION, "cognition_module_0", BIO_MODULE_CATEGORY_COGNITIVE, 2},
        {BIO_MODULE_MEMORY, "cognition_module_1", BIO_MODULE_CATEGORY_COGNITIVE, 2},
        {BIO_MODULE_ETHICS, "ethics_module_0", BIO_MODULE_CATEGORY_COGNITIVE, 3}
    };

    for (const auto& m : modules) {
        ASSERT_EQ(RegisterModule(m.id, m.name, m.cat, m.phase), 0);
    }

    // Discover all wiring
    int discovered = bio_orchestrator_discover_all_wiring(orch_);
    EXPECT_GE(discovered, 4) << "Should discover wiring for most modules";
}

TEST_F(WiringDiagramIntegrationTest, WiringWithHardwareProfile) {
    // Create platform-specific wiring
    std::string platform_file = temp_dir_ + "/platforms/full.jsonl";
    std::ofstream file(platform_file);
    file << R"({"type":"entity","name":"gpu_accelerated_module","entityType":"CognitiveModule","subsystem":"core","required_hw":1})" << "\n";
    file.close();

    // Create hardware profile for FULL tier with CUDA
    wiring_hardware_profile_t profile;
    wiring_get_default_profile(&profile);
    profile.tier = PLATFORM_TIER_FULL;
    profile.hw_flags = WIRING_HW_CUDA;

    // Load for profile
    ASSERT_EQ(wiring_diagram_load_for_profile(wd_, &profile), 0);

    // Check module availability
    bool available = wiring_diagram_module_available(wd_, "gpu_accelerated_module", &profile);
    EXPECT_TRUE(available) << "GPU module should be available on FULL tier with CUDA";

    // Check unavailability on CONSTRAINED tier
    profile.tier = PLATFORM_TIER_CONSTRAINED;
    profile.hw_flags = WIRING_HW_NONE;
    available = wiring_diagram_module_available(wd_, "gpu_accelerated_module", &profile);
    EXPECT_FALSE(available) << "GPU module should not be available without CUDA";
}

TEST_F(WiringDiagramIntegrationTest, ConcurrentWiringDiscovery) {
    // Create wiring with many modules
    CreateTestWiringFile("core", 20);
    ASSERT_EQ(wiring_diagram_load_subsystem(wd_, WIRING_SUBSYSTEM_CORE), 0);
    ASSERT_EQ(bio_orchestrator_set_wiring_diagram(orch_, wd_), 0);

    // Register multiple modules
    for (int i = 0; i < 10; i++) {
        char name[64];
        snprintf(name, sizeof(name), "core_module_%d", i);
        RegisterModule((bio_module_id_t)(BIO_MODULE_BRAIN + i), name, BIO_MODULE_CATEGORY_CORE, 1);
    }

    // Concurrent discovery from multiple threads
    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, &success_count]() {
            // Each thread tries to discover wiring
            int result = bio_orchestrator_discover_all_wiring(orch_);
            if (result >= 0) {
                success_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // All threads should succeed (thread-safe implementation)
    EXPECT_EQ(success_count.load(), 4) << "All concurrent discoveries should succeed";
}

TEST_F(WiringDiagramIntegrationTest, GetModuleHandlersAfterDiscovery) {
    // Create wiring with handlers
    std::vector<int> msg_types = {
        BIO_MSG_BRAIN_STATE_QUERY,
        BIO_MSG_BRAIN_STATE_RESPONSE,
        BIO_MSG_NEURON_ACTIVATION_REQUEST
    };
    CreateModuleWithHandlers("handler_module", "core", msg_types);

    ASSERT_EQ(wiring_diagram_load_subsystem(wd_, WIRING_SUBSYSTEM_CORE), 0);
    ASSERT_EQ(bio_orchestrator_set_wiring_diagram(orch_, wd_), 0);

    // Register module
    ASSERT_EQ(RegisterModule(BIO_MODULE_BRAIN, "handler_module", BIO_MODULE_CATEGORY_CORE, 1), 0);

    // Discover wiring
    bio_orchestrator_discover_all_wiring(orch_);

    // Get discovered handlers
    bio_message_type_t handlers[16];
    uint32_t handler_count = bio_orchestrator_get_module_handlers(
        orch_, BIO_MODULE_BRAIN, handlers, 16);

    EXPECT_GE(handler_count, 1u) << "Should have discovered handlers";
}

TEST_F(WiringDiagramIntegrationTest, WiringDiagramPersistenceRoundTrip) {
    // Add modules dynamically
    wiring_module_config_t config;
    wiring_module_config_init(&config);
    strcpy(config.module_name, "dynamic_module");
    config.subsystem = WIRING_SUBSYSTEM_CORE;
    config.min_tier = PLATFORM_TIER_MINIMAL;
    config.enabled = true;

    ASSERT_EQ(wiring_diagram_add_module(wd_, "dynamic_module", &config), 0);
    ASSERT_EQ(wiring_diagram_add_handler(wd_, "dynamic_module", BIO_MSG_HEALTH_CHECK), 0);

    // Persist
    ASSERT_EQ(wiring_diagram_persist(wd_), 0);

    // Destroy and recreate
    wiring_diagram_destroy(wd_);
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    // Reload
    ASSERT_EQ(wiring_diagram_load_subsystem(wd_, WIRING_SUBSYSTEM_CORE), 0);

    // Verify module is still there (get_module_config returns shallow copy)
    wiring_module_config_t retrieved;
    memset(&retrieved, 0, sizeof(retrieved));
    int result = wiring_diagram_get_module_config(wd_, "dynamic_module", &retrieved);
    EXPECT_EQ(result, 0) << "Module should persist across restart";

    if (result == 0) {
        EXPECT_STREQ(retrieved.module_name, "dynamic_module");
        EXPECT_TRUE(retrieved.enabled);
    }
    // Note: Don't call cleanup on shallow-copied config - memory owned by diagram
}

//=============================================================================
// Bio-Router Integration Tests
//=============================================================================

TEST_F(WiringDiagramIntegrationTest, RouterIntegrationWithWiring) {
    // Create wiring with handlers
    std::vector<int> msg_types = {BIO_MSG_HEALTH_CHECK};
    CreateModuleWithHandlers("router_test_module", "core", msg_types);

    ASSERT_EQ(wiring_diagram_load_subsystem(wd_, WIRING_SUBSYSTEM_CORE), 0);

    // Get module config (returns shallow copy - DO NOT call cleanup)
    wiring_module_config_t config;
    memset(&config, 0, sizeof(config));
    ASSERT_EQ(wiring_diagram_get_module_config(wd_, "router_test_module", &config), 0);

    // Verify handlers were loaded
    EXPECT_GE(config.handles_message_count, 1u)
        << "Module should have handlers from wiring";
    // Note: Don't call cleanup on shallow-copied config - memory owned by diagram
}

//=============================================================================
// Router-Orchestrator Integration Tests (KG-Based Runtime Module Assembly)
//=============================================================================

TEST_F(WiringDiagramIntegrationTest, RouterOrchestratorLinking) {
    // Initially, router should have no orchestrator
    EXPECT_EQ(bio_router_get_orchestrator(), nullptr)
        << "Router should start with no orchestrator";

    // Link orchestrator to router
    EXPECT_EQ(bio_router_set_orchestrator(orch_), NIMCP_SUCCESS)
        << "Should successfully link orchestrator to router";

    // Now router should return the orchestrator
    EXPECT_EQ(bio_router_get_orchestrator(), orch_)
        << "Router should return the linked orchestrator";

    // Unlink (set to NULL)
    EXPECT_EQ(bio_router_set_orchestrator(nullptr), NIMCP_SUCCESS)
        << "Should successfully unlink orchestrator";

    EXPECT_EQ(bio_router_get_orchestrator(), nullptr)
        << "Router should have no orchestrator after unlink";
}

// Global counter for callback invocation testing
static std::atomic<int> g_callback_invocation_count{0};
static std::atomic<int> g_callback_message_count{0};

static int test_wiring_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    (void)ctx;
    (void)user_data;
    g_callback_invocation_count++;
    g_callback_message_count += message_count;

    // Log the message types we received
    for (uint32_t i = 0; i < message_count; i++) {
        (void)message_types[i];  // Would normally register handlers here
    }
    return 0;
}

TEST_F(WiringDiagramIntegrationTest, RouterWiringCallbackRegistration) {
    // Reset counters
    g_callback_invocation_count = 0;
    g_callback_message_count = 0;

    // Without orchestrator linked, callback registration should fail
    nimcp_error_t result = bio_router_register_wiring_callback(
        BIO_MODULE_ETHICS,
        (void*)test_wiring_callback,
        nullptr
    );
    EXPECT_NE(result, NIMCP_SUCCESS)
        << "Should fail without orchestrator";

    // Link orchestrator
    ASSERT_EQ(bio_router_set_orchestrator(orch_), NIMCP_SUCCESS);

    // Create wiring with handlers for ethics module
    std::vector<int> msg_types = {
        BIO_MSG_ETHICS_EVALUATION_REQUEST,
        BIO_MSG_ETHICS_EVALUATION_RESPONSE
    };
    CreateModuleWithHandlers("Ethics_Module", "ethics", msg_types);
    ASSERT_EQ(wiring_diagram_load_subsystem(wd_, WIRING_SUBSYSTEM_ETHICS), 0);

    // Set wiring diagram on orchestrator
    ASSERT_EQ(bio_orchestrator_set_wiring_diagram(orch_, wd_), 0);

    // Register the module with orchestrator
    ASSERT_EQ(RegisterModule(BIO_MODULE_ETHICS, "Ethics_Module",
                             BIO_MODULE_CATEGORY_COGNITIVE, 1), 0);

    // Now callback registration should succeed
    result = bio_router_register_wiring_callback(
        BIO_MODULE_ETHICS,
        (void*)test_wiring_callback,
        nullptr
    );
    EXPECT_EQ(result, NIMCP_SUCCESS)
        << "Should succeed with orchestrator linked";

    // Cleanup
    bio_router_set_orchestrator(nullptr);
}

TEST_F(WiringDiagramIntegrationTest, WiringCallbackInvokedDuringDiscovery) {
    // Reset counters
    g_callback_invocation_count = 0;
    g_callback_message_count = 0;

    // Link orchestrator and set up wiring
    ASSERT_EQ(bio_router_set_orchestrator(orch_), NIMCP_SUCCESS);

    // Create wiring with handlers
    std::vector<int> msg_types = {
        BIO_MSG_KNOWLEDGE_QUERY,
        BIO_MSG_BRAIN_STATE_QUERY,
        BIO_MSG_NEURON_ACTIVATION_REQUEST
    };
    CreateModuleWithHandlers("callback_test_module", "core", msg_types);
    ASSERT_EQ(wiring_diagram_load_subsystem(wd_, WIRING_SUBSYSTEM_CORE), 0);
    ASSERT_EQ(bio_orchestrator_set_wiring_diagram(orch_, wd_), 0);

    // Register module
    ASSERT_EQ(RegisterModule(BIO_MODULE_BRAIN, "callback_test_module",
                             BIO_MODULE_CATEGORY_CORE, 1), 0);

    // Register wiring callback
    ASSERT_EQ(bio_router_register_wiring_callback(
        BIO_MODULE_BRAIN,
        (void*)test_wiring_callback,
        nullptr
    ), NIMCP_SUCCESS);

    // Discover wiring first - this populates the module's handler list from KG
    int discovered = bio_orchestrator_discover_all_wiring(orch_);
    EXPECT_GE(discovered, 1) << "Should have discovered wiring for at least one module";

    // Invoke handler callbacks (simulates what orchestrator does during start)
    int invoked = bio_orchestrator_invoke_handler_callbacks(orch_);
    EXPECT_GE(invoked, 1) << "Should have invoked at least one callback";

    // Verify our callback was invoked
    EXPECT_GE(g_callback_invocation_count.load(), 1)
        << "Callback should have been invoked";
    EXPECT_GE(g_callback_message_count.load(), (int)msg_types.size())
        << "Callback should have received all message types";

    // Cleanup
    bio_router_set_orchestrator(nullptr);
}

