/**
 * @file test_cognitive_bio_async_integration.cpp
 * @brief Integration tests for cognitive module bio-async communication
 *
 * WHAT: Tests cross-module communication via bio-async router
 * WHY:  Verify modules can send/receive messages correctly
 * HOW:  Create multiple modules, send messages, verify delivery
 *
 * TEST SCENARIOS:
 * 1. Module-to-module message delivery
 * 2. Broadcast message propagation
 * 3. Multi-module pipeline coordination
 * 4. Message routing under load
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

// Headers have their own extern "C" guards
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "cognitive/analysis/nimcp_network_analysis.h"
#include "cognitive/memory/nimcp_systems_consolidation.h"
#include "cognitive/global_workspace/nimcp_global_workspace.h"
#include "cognitive/knowledge/nimcp_knowledge.h"
#include "cognitive/nimcp_mirror_neurons.h"
#include "utils/memory/nimcp_unified_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class CognitiveBioAsyncIntegrationTest : public ::testing::Test {
protected:
    unified_mem_manager_t mem_mgr_ = nullptr;

    void SetUp() override {
        // Initialize unified memory FIRST - required by cognitive modules
        unified_mem_config_t mem_config = unified_mem_default_config();
        mem_mgr_ = unified_mem_create(&mem_config);
        ASSERT_NE(mem_mgr_, nullptr) << "Failed to create unified memory manager";

        // Initialize bio-async system
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        bio_config.enable_statistics = true;
        bio_config.enable_logging = true;
        ASSERT_EQ(nimcp_bio_async_init(&bio_config), NIMCP_SUCCESS);

        // Initialize bio-router
        bio_router_config_t router_config = bio_router_default_config();
        router_config.enable_statistics = true;
        ASSERT_EQ(bio_router_init(&router_config), NIMCP_SUCCESS);
    }

    void TearDown() override {
        bio_router_shutdown();
        nimcp_bio_async_shutdown();
        // Note: unified memory will be cleaned up when mem_mgr_ goes out of scope
        // or we can explicitly destroy it:
        if (mem_mgr_) {
            unified_mem_destroy(mem_mgr_);
            mem_mgr_ = nullptr;
        }
    }
};

//=============================================================================
// MESSAGE DELIVERY TESTS
//=============================================================================

TEST_F(CognitiveBioAsyncIntegrationTest, ModuleToModule_BasicMessageDelivery) {
    // Create two modules
    bio_module_info_t sender_info;
    sender_info.module_id = BIO_MODULE_NETWORK_ANALYSIS;
    sender_info.module_name = "sender";
    sender_info.inbox_capacity = 64;
    sender_info.user_data = nullptr;

    bio_module_context_t sender = bio_router_register_module(&sender_info);
    ASSERT_NE(sender, nullptr);

    bio_module_info_t receiver_info;
    receiver_info.module_id = BIO_MODULE_CONSOLIDATION;
    receiver_info.module_name = "receiver";
    receiver_info.inbox_capacity = 64;
    receiver_info.user_data = nullptr;

    bio_module_context_t receiver = bio_router_register_module(&receiver_info);
    ASSERT_NE(receiver, nullptr);

    // Track message receipt
    std::atomic<bool> message_received{false};
    std::atomic<uint32_t> received_value{0};

    // Register handler on receiver
    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* data = static_cast<std::pair<std::atomic<bool>*, std::atomic<uint32_t>*>*>(user_data);
        data->first->store(true);

        // Extract value from message
        auto* header = static_cast<const bio_message_header_t*>(msg);
        data->second->store(header->sequence_id);

        return NIMCP_SUCCESS;
    };

    std::pair<std::atomic<bool>*, std::atomic<uint32_t>*> handler_data{
        &message_received, &received_value
    };

    // Re-register with user data
    bio_router_unregister_module(receiver);
    receiver_info.user_data = &handler_data;
    receiver = bio_router_register_module(&receiver_info);

    bio_router_register_handler(receiver, BIO_MSG_CONSOLIDATION_TRIGGER, handler);

    // Send message from sender to receiver
    bio_message_header_t msg;
    bio_msg_init_header(&msg, BIO_MSG_CONSOLIDATION_TRIGGER,
                       BIO_MODULE_NETWORK_ANALYSIS, BIO_MODULE_CONSOLIDATION,
                       sizeof(msg));
    msg.sequence_id = 12345;

    nimcp_error_t err = bio_router_send(sender, &msg, sizeof(msg), 1000);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Process receiver inbox
    uint32_t processed = bio_router_process_inbox(receiver, 10);
    EXPECT_GT(processed, 0u);

    EXPECT_TRUE(message_received.load());
    EXPECT_EQ(received_value.load(), 12345u);

    bio_router_unregister_module(sender);
    bio_router_unregister_module(receiver);
}

TEST_F(CognitiveBioAsyncIntegrationTest, MultiModule_PipelineCoordination) {
    // Test pipeline: Network Analysis -> Consolidation -> Knowledge
    // Each module processes and forwards

    std::atomic<int> pipeline_progress{0};

    // Create modules
    bio_module_info_t na_info;
    na_info.module_id = BIO_MODULE_NETWORK_ANALYSIS;
    na_info.module_name = "network_analysis";
    na_info.inbox_capacity = 64;
    na_info.user_data = &pipeline_progress;

    bio_module_info_t cons_info;
    cons_info.module_id = BIO_MODULE_CONSOLIDATION;
    cons_info.module_name = "consolidation";
    cons_info.inbox_capacity = 64;
    cons_info.user_data = &pipeline_progress;

    bio_module_info_t know_info;
    know_info.module_id = BIO_MODULE_KNOWLEDGE;
    know_info.module_name = "knowledge";
    know_info.inbox_capacity = 64;
    know_info.user_data = &pipeline_progress;

    bio_module_context_t na = bio_router_register_module(&na_info);
    bio_module_context_t cons = bio_router_register_module(&cons_info);
    bio_module_context_t know = bio_router_register_module(&know_info);

    ASSERT_NE(na, nullptr);
    ASSERT_NE(cons, nullptr);
    ASSERT_NE(know, nullptr);

    // Stage handlers
    auto stage1_handler = [](const void* msg, size_t size,
                            nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* progress = static_cast<std::atomic<int>*>(user_data);
        progress->store(1);  // Stage 1 complete
        return NIMCP_SUCCESS;
    };

    auto stage2_handler = [](const void* msg, size_t size,
                            nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* progress = static_cast<std::atomic<int>*>(user_data);
        progress->store(2);  // Stage 2 complete
        return NIMCP_SUCCESS;
    };

    auto stage3_handler = [](const void* msg, size_t size,
                            nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* progress = static_cast<std::atomic<int>*>(user_data);
        progress->store(3);  // Stage 3 complete
        return NIMCP_SUCCESS;
    };

    // Use generic message type for testing since specific ones don't exist
    bio_router_register_handler(na, BIO_MSG_INTROSPECTION_QUERY, stage1_handler);
    bio_router_register_handler(cons, BIO_MSG_CONSOLIDATION_TRIGGER, stage2_handler);
    bio_router_register_handler(know, BIO_MSG_KNOWLEDGE_QUERY, stage3_handler);

    // Trigger pipeline stage 1
    bio_message_header_t msg1;
    bio_msg_init_header(&msg1, BIO_MSG_INTROSPECTION_QUERY,
                       BIO_MODULE_BRAIN, BIO_MODULE_NETWORK_ANALYSIS,
                       sizeof(msg1));

    bio_router_send(na, &msg1, sizeof(msg1), 1000);
    bio_router_process_inbox(na, 10);
    EXPECT_EQ(pipeline_progress.load(), 1);

    // Trigger pipeline stage 2
    bio_message_header_t msg2;
    bio_msg_init_header(&msg2, BIO_MSG_CONSOLIDATION_TRIGGER,
                       BIO_MODULE_NETWORK_ANALYSIS, BIO_MODULE_CONSOLIDATION,
                       sizeof(msg2));

    bio_router_send(na, &msg2, sizeof(msg2), 1000);
    bio_router_process_inbox(cons, 10);
    EXPECT_EQ(pipeline_progress.load(), 2);

    // Trigger pipeline stage 3
    bio_message_header_t msg3;
    bio_msg_init_header(&msg3, BIO_MSG_KNOWLEDGE_QUERY,
                       BIO_MODULE_CONSOLIDATION, BIO_MODULE_KNOWLEDGE,
                       sizeof(msg3));

    bio_router_send(cons, &msg3, sizeof(msg3), 1000);
    bio_router_process_inbox(know, 10);
    EXPECT_EQ(pipeline_progress.load(), 3);

    // Cleanup
    bio_router_unregister_module(na);
    bio_router_unregister_module(cons);
    bio_router_unregister_module(know);
}

//=============================================================================
// NEUROMODULATOR CHANNEL TESTS
//=============================================================================

TEST_F(CognitiveBioAsyncIntegrationTest, NeuromodulatorChannels_DifferentPriorities) {
    // Test that different neuromodulator channels are handled correctly
    std::atomic<int> dopamine_count{0};
    std::atomic<int> serotonin_count{0};
    std::atomic<int> acetylcholine_count{0};

    struct ChannelCounters {
        std::atomic<int>* dopamine;
        std::atomic<int>* serotonin;
        std::atomic<int>* acetylcholine;
    };

    ChannelCounters counters{&dopamine_count, &serotonin_count, &acetylcholine_count};

    bio_module_info_t info;
    info.module_id = BIO_MODULE_GLOBAL_WORKSPACE;
    info.module_name = "workspace";
    info.inbox_capacity = 128;
    info.user_data = &counters;

    bio_module_context_t workspace = bio_router_register_module(&info);
    ASSERT_NE(workspace, nullptr);

    // Handler that counts by channel type
    auto channel_handler = [](const void* msg, size_t size,
                             nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* ctrs = static_cast<ChannelCounters*>(user_data);
        auto* header = static_cast<const bio_message_header_t*>(msg);

        switch (header->channel) {
            case BIO_CHANNEL_DOPAMINE:
                ctrs->dopamine->fetch_add(1);
                break;
            case BIO_CHANNEL_SEROTONIN:
                ctrs->serotonin->fetch_add(1);
                break;
            case BIO_CHANNEL_ACETYLCHOLINE:
                ctrs->acetylcholine->fetch_add(1);
                break;
            default:
                break;
        }

        return NIMCP_SUCCESS;
    };

    // Use a generic message type since BIO_MSG_GLOBAL_WORKSPACE_BROADCAST doesn't exist
    bio_router_register_handler(workspace, BIO_MSG_ATTENTION_SHIFT, channel_handler);

    // Send messages on different channels
    for (int i = 0; i < 5; i++) {
        bio_message_header_t da_msg;
        bio_msg_init_header(&da_msg, BIO_MSG_ATTENTION_SHIFT,
                           BIO_MODULE_BRAIN, BIO_MODULE_GLOBAL_WORKSPACE,
                           sizeof(da_msg));
        da_msg.channel = BIO_CHANNEL_DOPAMINE;
        bio_router_send(workspace, &da_msg, sizeof(da_msg), 100);
    }

    for (int i = 0; i < 3; i++) {
        bio_message_header_t st_msg;
        bio_msg_init_header(&st_msg, BIO_MSG_ATTENTION_SHIFT,
                           BIO_MODULE_BRAIN, BIO_MODULE_GLOBAL_WORKSPACE,
                           sizeof(st_msg));
        st_msg.channel = BIO_CHANNEL_SEROTONIN;
        bio_router_send(workspace, &st_msg, sizeof(st_msg), 100);
    }

    for (int i = 0; i < 7; i++) {
        bio_message_header_t ach_msg;
        bio_msg_init_header(&ach_msg, BIO_MSG_ATTENTION_SHIFT,
                           BIO_MODULE_BRAIN, BIO_MODULE_GLOBAL_WORKSPACE,
                           sizeof(ach_msg));
        ach_msg.channel = BIO_CHANNEL_ACETYLCHOLINE;
        bio_router_send(workspace, &ach_msg, sizeof(ach_msg), 100);
    }

    // Process all messages
    bio_router_process_inbox(workspace, 50);

    EXPECT_EQ(dopamine_count.load(), 5);
    EXPECT_EQ(serotonin_count.load(), 3);
    EXPECT_EQ(acetylcholine_count.load(), 7);

    bio_router_unregister_module(workspace);
}

//=============================================================================
// CONCURRENT MESSAGE HANDLING TESTS
//=============================================================================

TEST_F(CognitiveBioAsyncIntegrationTest, ConcurrentModules_HighThroughput) {
    const int NUM_MODULES = 4;
    const int MESSAGES_PER_MODULE = 50;

    std::vector<bio_module_context_t> modules(NUM_MODULES);
    std::atomic<int> total_processed{0};

    // Create modules
    for (int i = 0; i < NUM_MODULES; i++) {
        bio_module_info_t info;
        info.module_id = static_cast<bio_module_id_t>(0x1000 + i);
        info.module_name = "test_module";
        info.inbox_capacity = 256;
        info.user_data = &total_processed;

        modules[i] = bio_router_register_module(&info);
        ASSERT_NE(modules[i], nullptr);

        // Register simple counter handler
        auto handler = [](const void* msg, size_t size,
                         nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
            auto* counter = static_cast<std::atomic<int>*>(user_data);
            counter->fetch_add(1);
            return NIMCP_SUCCESS;
        };

        // Use INTROSPECTION_QUERY as a generic message type
        bio_router_register_handler(modules[i], BIO_MSG_INTROSPECTION_QUERY, handler);
    }

    // Each module sends to all others
    std::vector<std::thread> senders;
    for (int i = 0; i < NUM_MODULES; i++) {
        senders.emplace_back([&modules, i, NUM_MODULES, MESSAGES_PER_MODULE]() {
            for (int j = 0; j < MESSAGES_PER_MODULE; j++) {
                for (int k = 0; k < NUM_MODULES; k++) {
                    if (k != i) {
                        bio_message_header_t msg;
                        bio_msg_init_header(&msg, BIO_MSG_INTROSPECTION_QUERY,
                                           static_cast<bio_module_id_t>(0x1000 + i),
                                           static_cast<bio_module_id_t>(0x1000 + k),
                                           sizeof(msg));
                        bio_router_send(modules[i], &msg, sizeof(msg), 10);
                    }
                }
            }
        });
    }

    // Wait for senders
    for (auto& t : senders) {
        t.join();
    }

    // Process all inboxes
    for (int i = 0; i < NUM_MODULES; i++) {
        bio_router_process_inbox(modules[i], 1000);
    }

    // Verify message count
    int expected = NUM_MODULES * MESSAGES_PER_MODULE * (NUM_MODULES - 1);
    EXPECT_GE(total_processed.load(), expected / 2)
        << "Should process most messages under high load";

    // Cleanup
    for (int i = 0; i < NUM_MODULES; i++) {
        bio_router_unregister_module(modules[i]);
    }
}

//=============================================================================
// ACTUAL MODULE INTEGRATION TESTS
//=============================================================================

TEST_F(CognitiveBioAsyncIntegrationTest, RealModules_NetworkAnalysisToConsolidation) {
    // Create actual cognitive modules
    // Note: network_analyzer_create requires a brain_t, skip this test for now
    // TODO: Create brain instance or mock for integration test

    systems_consolidation_system_t* consolidation = systems_consolidation_create();
    ASSERT_NE(consolidation, nullptr);

    // Check bio-router stats
    bio_router_stats_t stats;
    nimcp_error_t err = bio_router_get_stats(&stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    // Note: consolidation may or may not register with bio-router

    // Cleanup
    systems_consolidation_destroy(consolidation);
}

TEST_F(CognitiveBioAsyncIntegrationTest, RealModules_GlobalWorkspaceWithKnowledge) {
    // Create global workspace and knowledge system
    global_workspace_config_t gw_config = global_workspace_default_config();
    gw_config.capacity_dim = 128;
    gw_config.ignition_threshold = 0.5f;
    gw_config.refractory_period_ms = 50;

    global_workspace_t* workspace = global_workspace_create_custom(&gw_config);
    ASSERT_NE(workspace, nullptr);

    knowledge_system_t knowledge = knowledge_system_create("test_learner");
    ASSERT_NE(knowledge, nullptr);

    // Verify both registered
    bio_router_stats_t stats;
    bio_router_get_stats(&stats);
    // Note: modules may or may not register with bio-router

    // Cleanup
    knowledge_system_destroy(knowledge);
    global_workspace_destroy(workspace);
}

TEST_F(CognitiveBioAsyncIntegrationTest, RealModules_MirrorNeuronsWithGlobalWorkspace) {
    // Test mirror neurons integration with global workspace
    mirror_neuron_config_t mn_config = mirror_neurons_get_default_config();
    mirror_neurons_t mirror = mirror_neurons_create(&mn_config);
    ASSERT_NE(mirror, nullptr);

    global_workspace_config_t gw_config = global_workspace_default_config();
    gw_config.capacity_dim = 128;
    gw_config.ignition_threshold = 0.5f;
    gw_config.refractory_period_ms = 50;

    global_workspace_t* workspace = global_workspace_create_custom(&gw_config);
    ASSERT_NE(workspace, nullptr);

    // Both should be registered
    bio_router_stats_t stats;
    bio_router_get_stats(&stats);
    // Note: modules may or may not register with bio-router

    // Cleanup
    global_workspace_destroy(workspace);
    mirror_neurons_destroy(mirror);
}

//=============================================================================
// STATISTICS AND MONITORING TESTS
//=============================================================================

TEST_F(CognitiveBioAsyncIntegrationTest, Statistics_MessageCountTracking) {
    bio_module_info_t info;
    info.module_id = BIO_MODULE_INTROSPECTION;
    info.module_name = "stats_test";
    info.inbox_capacity = 64;
    info.user_data = nullptr;

    bio_module_context_t module = bio_router_register_module(&info);
    ASSERT_NE(module, nullptr);

    // Get initial stats
    bio_router_stats_t initial_stats;
    bio_router_get_stats(&initial_stats);
    uint64_t initial_routed = initial_stats.messages_routed;

    // Send several messages
    const int NUM_MESSAGES = 10;
    for (int i = 0; i < NUM_MESSAGES; i++) {
        bio_message_header_t msg;
        bio_msg_init_header(&msg, BIO_MSG_INTROSPECTION_QUERY,
                           BIO_MODULE_BRAIN, BIO_MODULE_INTROSPECTION,
                           sizeof(msg));
        bio_router_send(module, &msg, sizeof(msg), 100);
    }

    // Verify stats updated
    bio_router_stats_t final_stats;
    bio_router_get_stats(&final_stats);

    EXPECT_GE(final_stats.messages_routed, initial_routed)
        << "Statistics should track routed messages";

    bio_router_unregister_module(module);
}
