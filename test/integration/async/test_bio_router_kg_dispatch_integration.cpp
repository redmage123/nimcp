/**
 * @file test_bio_router_kg_dispatch_integration.cpp
 * @brief Integration tests for Phase 7: KG-driven message dispatch
 *
 * Tests KG dispatch in a realistic multi-module scenario with:
 * - Brain KG with wiring data
 * - Multiple modules with handlers
 * - Cross-module message routing via KG
 *
 * @author NIMCP Development Team
 * @date 2026-01-03
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <cstring>

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "core/brain/nimcp_brain_kg.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Fixture - Multi-Module Integration
//=============================================================================

class BioRouterKGDispatchIntegration : public ::testing::Test {
protected:
    brain_kg_t* kg = nullptr;
    std::vector<bio_module_context_t> module_contexts;

    // Message counters per module
    static std::atomic<int> s_attention_count;
    static std::atomic<int> s_memory_count;
    static std::atomic<int> s_training_count;
    static std::atomic<int> s_executive_count;
    static std::atomic<int> s_total_count;

    void SetUp() override {
        ResetCounters();

        // Initialize bio-async
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        bio_config.enable_statistics = true;
        bio_config.enable_logging = false;
        ASSERT_EQ(nimcp_bio_async_init(&bio_config), NIMCP_SUCCESS);

        // Initialize router
        bio_router_config_t router_config = bio_router_default_config();
        router_config.enable_statistics = true;
        router_config.enable_logging = false;
        ASSERT_EQ(bio_router_init(&router_config), NIMCP_SUCCESS);

        // Create brain KG
        brain_kg_config_t kg_config;
        brain_kg_default_config(&kg_config);
        kg_config.enable_security = false;
        kg_config.enable_access_control = false;
        kg = brain_kg_create(&kg_config);
        ASSERT_NE(kg, nullptr);

        // Connect KG to router
        bio_router_set_brain_kg(kg);

        // Setup modules
        SetupModules();
        SetupKGWiring();
    }

    void TearDown() override {
        bio_router_set_brain_kg(nullptr);

        for (auto ctx : module_contexts) {
            if (ctx) bio_router_unregister_module(ctx);
        }
        module_contexts.clear();

        bio_router_shutdown();
        nimcp_bio_async_shutdown();

        if (kg) {
            brain_kg_destroy(kg);
            kg = nullptr;
        }
    }

    void ResetCounters() {
        s_attention_count = 0;
        s_memory_count = 0;
        s_training_count = 0;
        s_executive_count = 0;
        s_total_count = 0;
    }

    void SetupModules() {
        struct ModuleSetup {
            bio_module_id_t id;
            const char* name;
            nimcp_error_t (*handler)(const void*, size_t, nimcp_bio_promise_t, void*);
        };

        ModuleSetup modules[] = {
            {BIO_MODULE_BRAIN, "brain", nullptr},  // Sender
            {BIO_MODULE_ATTENTION, "attention", attention_handler},
            {BIO_MODULE_MEMORY, "memory", memory_handler},
            {BIO_MODULE_TRAINING, "training", training_handler},
            {BIO_MODULE_EXECUTIVE, "executive", executive_handler}
        };

        for (const auto& m : modules) {
            bio_module_info_t info = {
                .module_id = m.id,
                .module_name = m.name,
                .inbox_capacity = 64,
                .user_data = nullptr
            };
            bio_module_context_t ctx = bio_router_register_module(&info);
            ASSERT_NE(ctx, nullptr) << "Failed to register " << m.name;
            module_contexts.push_back(ctx);

            if (m.handler) {
                bio_router_register_handler(ctx, BIO_MSG_BRAIN_STATE_QUERY, m.handler);
                bio_router_register_handler(ctx, BIO_MSG_ATTENTION_SHIFT, m.handler);
            }
        }
    }

    void SetupKGWiring() {
        // Add KG nodes
        brain_kg_add_node(kg, "attention_module", BRAIN_KG_NODE_COGNITIVE, "Attention");
        brain_kg_add_node(kg, "memory_module", BRAIN_KG_NODE_COGNITIVE, "Memory");
        brain_kg_add_node(kg, "training_module", BRAIN_KG_NODE_COGNITIVE, "Training");
        brain_kg_add_node(kg, "executive_module", BRAIN_KG_NODE_COGNITIVE, "Executive");

        // Setup message handlers in KG
        // BIO_MSG_BRAIN_STATE_QUERY -> attention, memory
        brain_kg_add_message_handler(kg, BIO_MODULE_ATTENTION, BIO_MSG_BRAIN_STATE_QUERY);
        brain_kg_add_message_handler(kg, BIO_MODULE_MEMORY, BIO_MSG_BRAIN_STATE_QUERY);

        // BIO_MSG_ATTENTION_SHIFT -> attention, executive
        brain_kg_add_message_handler(kg, BIO_MODULE_ATTENTION, BIO_MSG_ATTENTION_SHIFT);
        brain_kg_add_message_handler(kg, BIO_MODULE_EXECUTIVE, BIO_MSG_ATTENTION_SHIFT);
    }

    // Message handlers
    static nimcp_error_t attention_handler(const void* msg, size_t, nimcp_bio_promise_t, void*) {
        (void)msg;
        s_attention_count++;
        s_total_count++;
        return NIMCP_SUCCESS;
    }

    static nimcp_error_t memory_handler(const void* msg, size_t, nimcp_bio_promise_t, void*) {
        (void)msg;
        s_memory_count++;
        s_total_count++;
        return NIMCP_SUCCESS;
    }

    static nimcp_error_t training_handler(const void* msg, size_t, nimcp_bio_promise_t, void*) {
        (void)msg;
        s_training_count++;
        s_total_count++;
        return NIMCP_SUCCESS;
    }

    static nimcp_error_t executive_handler(const void* msg, size_t, nimcp_bio_promise_t, void*) {
        (void)msg;
        s_executive_count++;
        s_total_count++;
        return NIMCP_SUCCESS;
    }

    bio_module_context_t GetSenderContext() { return module_contexts[0]; }

    void ProcessAllInboxes() {
        for (auto ctx : module_contexts) {
            bio_router_process_inbox(ctx, 100);
        }
    }

    // Create KG-dispatched messages
    bio_msg_brain_state_query_t CreateBrainStateQuery() {
        bio_msg_brain_state_query_t msg;
        memset(&msg, 0, sizeof(msg));
        bio_msg_init_header(&msg.header, BIO_MSG_BRAIN_STATE_QUERY,
                           BIO_MODULE_BRAIN, BIO_MODULE_KG_DISPATCH,
                           sizeof(msg));
        msg.query_flags = BIO_BRAIN_QUERY_NEURON_COUNT;
        return msg;
    }

    bio_msg_attention_shift_t CreateAttentionShift() {
        bio_msg_attention_shift_t msg;
        memset(&msg, 0, sizeof(msg));
        bio_msg_init_header(&msg.header, BIO_MSG_ATTENTION_SHIFT,
                           BIO_MODULE_BRAIN, BIO_MODULE_KG_DISPATCH,
                           sizeof(msg));
        msg.target_id = 1;
        msg.attention_weight = 0.8f;
        msg.duration_ms = 100;
        msg.preemptive = false;
        return msg;
    }
};

std::atomic<int> BioRouterKGDispatchIntegration::s_attention_count{0};
std::atomic<int> BioRouterKGDispatchIntegration::s_memory_count{0};
std::atomic<int> BioRouterKGDispatchIntegration::s_training_count{0};
std::atomic<int> BioRouterKGDispatchIntegration::s_executive_count{0};
std::atomic<int> BioRouterKGDispatchIntegration::s_total_count{0};

//=============================================================================
// INTEGRATION TESTS
//=============================================================================

TEST_F(BioRouterKGDispatchIntegration, BrainStateQueryRoutesToCorrectModules) {
    // BIO_MSG_BRAIN_STATE_QUERY should go to attention and memory
    auto msg = CreateBrainStateQuery();
    EXPECT_EQ(bio_router_send(GetSenderContext(), &msg, sizeof(msg), 100), NIMCP_SUCCESS);

    ProcessAllInboxes();

    EXPECT_EQ(s_attention_count.load(), 1);
    EXPECT_EQ(s_memory_count.load(), 1);
    EXPECT_EQ(s_training_count.load(), 0);  // Not subscribed
    EXPECT_EQ(s_executive_count.load(), 0);  // Not subscribed
    EXPECT_EQ(s_total_count.load(), 2);
}

TEST_F(BioRouterKGDispatchIntegration, AttentionShiftRoutesToCorrectModules) {
    // BIO_MSG_ATTENTION_SHIFT should go to attention and executive
    auto msg = CreateAttentionShift();
    EXPECT_EQ(bio_router_send(GetSenderContext(), &msg, sizeof(msg), 100), NIMCP_SUCCESS);

    ProcessAllInboxes();

    EXPECT_EQ(s_attention_count.load(), 1);
    EXPECT_EQ(s_memory_count.load(), 0);      // Not subscribed
    EXPECT_EQ(s_training_count.load(), 0);    // Not subscribed
    EXPECT_EQ(s_executive_count.load(), 1);
    EXPECT_EQ(s_total_count.load(), 2);
}

TEST_F(BioRouterKGDispatchIntegration, MultipleMessageTypesInSequence) {
    // Send both message types
    auto query = CreateBrainStateQuery();
    auto shift = CreateAttentionShift();

    EXPECT_EQ(bio_router_send(GetSenderContext(), &query, sizeof(query), 100), NIMCP_SUCCESS);
    EXPECT_EQ(bio_router_send(GetSenderContext(), &shift, sizeof(shift), 100), NIMCP_SUCCESS);

    ProcessAllInboxes();

    // attention: 1 from query + 1 from shift = 2
    // memory: 1 from query = 1
    // executive: 1 from shift = 1
    EXPECT_EQ(s_attention_count.load(), 2);
    EXPECT_EQ(s_memory_count.load(), 1);
    EXPECT_EQ(s_executive_count.load(), 1);
    EXPECT_EQ(s_total_count.load(), 4);
}

TEST_F(BioRouterKGDispatchIntegration, HighVolumeKGDispatch) {
    const int MSG_COUNT = 100;

    for (int i = 0; i < MSG_COUNT; i++) {
        auto msg = CreateBrainStateQuery();
        EXPECT_EQ(bio_router_send(GetSenderContext(), &msg, sizeof(msg), 100), NIMCP_SUCCESS);
    }

    ProcessAllInboxes();

    // Each message goes to attention and memory
    EXPECT_EQ(s_attention_count.load(), MSG_COUNT);
    EXPECT_EQ(s_memory_count.load(), MSG_COUNT);
    EXPECT_EQ(s_total_count.load(), MSG_COUNT * 2);
}

TEST_F(BioRouterKGDispatchIntegration, DynamicWiringUpdate) {
    // Send initial message
    auto msg1 = CreateBrainStateQuery();
    bio_router_send(GetSenderContext(), &msg1, sizeof(msg1), 100);
    ProcessAllInboxes();

    EXPECT_EQ(s_training_count.load(), 0);  // Not yet subscribed

    // Add training to brain state query handlers
    brain_kg_add_message_handler(kg, BIO_MODULE_TRAINING, BIO_MSG_BRAIN_STATE_QUERY);

    // Send another message
    ResetCounters();
    auto msg2 = CreateBrainStateQuery();
    bio_router_send(GetSenderContext(), &msg2, sizeof(msg2), 100);
    ProcessAllInboxes();

    // Now training should receive it too
    EXPECT_EQ(s_attention_count.load(), 1);
    EXPECT_EQ(s_memory_count.load(), 1);
    EXPECT_EQ(s_training_count.load(), 1);
    EXPECT_EQ(s_total_count.load(), 3);
}

TEST_F(BioRouterKGDispatchIntegration, ConcurrentKGDispatch) {
    const int THREADS = 4;
    const int MSGS_PER_THREAD = 25;
    std::vector<std::thread> threads;

    for (int t = 0; t < THREADS; t++) {
        threads.emplace_back([this]() {
            for (int i = 0; i < MSGS_PER_THREAD; i++) {
                auto msg = CreateBrainStateQuery();
                bio_router_send(GetSenderContext(), &msg, sizeof(msg), 100);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Allow processing
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ProcessAllInboxes();

    int expected_per_module = THREADS * MSGS_PER_THREAD;
    EXPECT_EQ(s_attention_count.load(), expected_per_module);
    EXPECT_EQ(s_memory_count.load(), expected_per_module);
}

TEST_F(BioRouterKGDispatchIntegration, KGDisconnectMidOperation) {
    // Send first message successfully
    auto msg1 = CreateBrainStateQuery();
    EXPECT_EQ(bio_router_send(GetSenderContext(), &msg1, sizeof(msg1), 100), NIMCP_SUCCESS);

    // Disconnect KG
    bio_router_set_brain_kg(nullptr);

    // Next dispatch should fail
    auto msg2 = CreateBrainStateQuery();
    EXPECT_EQ(bio_router_send(GetSenderContext(), &msg2, sizeof(msg2), 100),
              NIMCP_ERROR_NOT_INITIALIZED);

    // Reconnect
    bio_router_set_brain_kg(kg);

    // Should work again
    auto msg3 = CreateBrainStateQuery();
    EXPECT_EQ(bio_router_send(GetSenderContext(), &msg3, sizeof(msg3), 100), NIMCP_SUCCESS);
}

TEST_F(BioRouterKGDispatchIntegration, StatsAfterKGDispatch) {
    bio_router_reset_stats();

    // Send several messages
    for (int i = 0; i < 10; i++) {
        auto msg = CreateBrainStateQuery();
        bio_router_send(GetSenderContext(), &msg, sizeof(msg), 100);
    }

    ProcessAllInboxes();

    // Check router stats
    bio_router_stats_t stats;
    EXPECT_EQ(bio_router_get_stats(&stats), NIMCP_SUCCESS);

    // Each message dispatches to 2 handlers = 20 routed messages
    EXPECT_GE(stats.messages_routed, 20u);
    EXPECT_EQ(stats.messages_dropped, 0u);
}
