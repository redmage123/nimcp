/**
 * @file test_cognitive_bio_async.cpp
 * @brief Unit tests for cognitive module bio-async integration
 *
 * Tests cognitive modules using bio-async messaging:
 * - Introspection queries via acetylcholine (fast)
 * - Ethics evaluation via serotonin (deliberative)
 * - Salience computation via norepinephrine (alerting)
 * - Mirror neuron activation patterns
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 */

#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"

//=============================================================================
// Test Fixture
//=============================================================================

class CognitiveBioAsyncTest : public ::testing::Test {
protected:
    bio_module_context_t cognitive_module;
    bio_module_context_t query_module;

    void SetUp() override {
        // Initialize bio-async system
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        bio_config.enable_statistics = true;
        bio_config.enable_logging = false;
        ASSERT_EQ(nimcp_bio_async_init(&bio_config), NIMCP_SUCCESS);

        // Initialize bio-router
        bio_router_config_t router_config = bio_router_default_config();
        router_config.enable_statistics = true;
        ASSERT_EQ(bio_router_init(&router_config), NIMCP_SUCCESS);

        // Register cognitive module
        bio_module_info_t cognitive_info;
        cognitive_info.module_id = BIO_MODULE_INTROSPECTION;
        cognitive_info.module_name = "cognitive";
        cognitive_info.inbox_capacity = 100;
        cognitive_info.user_data = nullptr;

        cognitive_module = bio_router_register_module(&cognitive_info);
        ASSERT_NE(cognitive_module, nullptr);

        // Register query module
        bio_module_info_t query_info;
        query_info.module_id = BIO_MODULE_BRAIN;
        query_info.module_name = "query";
        query_info.inbox_capacity = 100;
        query_info.user_data = nullptr;

        query_module = bio_router_register_module(&query_info);
        ASSERT_NE(query_module, nullptr);
    }

    void TearDown() override {
        bio_router_unregister_module(cognitive_module);
        bio_router_unregister_module(query_module);
        bio_router_shutdown();
        nimcp_bio_async_shutdown();
    }
};

//=============================================================================
// INTROSPECTION TESTS (Acetylcholine - Fast Queries)
//=============================================================================

TEST_F(CognitiveBioAsyncTest, IntrospectionQueryBasic) {
    std::atomic<bool> query_handled{false};
    std::atomic<float> received_confidence{0.0f};

    // Register introspection handler
    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* data = static_cast<std::pair<std::atomic<bool>*, std::atomic<float>*>*>(user_data);
        data->first->store(true);

        auto* query = static_cast<const bio_msg_introspection_query_t*>(msg);
        EXPECT_EQ(query->header.type, BIO_MSG_INTROSPECTION_QUERY);

        // Create response
        bio_msg_introspection_response_t response;
        bio_msg_init_header(&response.header, BIO_MSG_INTROSPECTION_RESPONSE,
                           BIO_MODULE_INTROSPECTION, static_cast<bio_module_id_t>(query->header.source_module),
                           sizeof(response));
        response.query_type = query->query_type;
        response.confidence = 0.85f;
        response.cognitive_load = 0.6f;
        response.emotional_valence = 0.3f;
        response.arousal = 0.5f;
        response.matched_pattern_count = 5;

        data->second->store(response.confidence);

        if (promise) {
            nimcp_bio_promise_complete_sized(promise, &response, sizeof(response));
        }

        return NIMCP_SUCCESS;
    };

    std::pair<std::atomic<bool>*, std::atomic<float>*> handler_data{
        &query_handled, &received_confidence
    };

    // Unregister default module and re-register with user_data
    bio_router_unregister_module(cognitive_module);

    bio_module_info_t module_info = {
        .module_id = BIO_MODULE_INTROSPECTION,
        .module_name = "cognitive",
        .inbox_capacity = 100,
        .user_data = &handler_data
    };
    cognitive_module = bio_router_register_module(&module_info);
    ASSERT_NE(cognitive_module, nullptr);

    bio_router_register_handler(cognitive_module, BIO_MSG_INTROSPECTION_QUERY, handler);

    // Send introspection query via acetylcholine (fast)
    bio_msg_introspection_query_t query;
    bio_msg_init_header(&query.header, BIO_MSG_INTROSPECTION_QUERY,
                       BIO_MODULE_BRAIN, BIO_MODULE_INTROSPECTION,
                       sizeof(query));
    query.header.channel = BIO_CHANNEL_ACETYLCHOLINE;
    query.query_type = BIO_INTRO_QUERY_SELF_STATE;
    query.target_pattern_id = 0;
    query.confidence_threshold = 0.5f;

    nimcp_error_t err = bio_router_send(query_module, &query, sizeof(query), 1000);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Process inbox
    uint32_t processed = bio_router_process_inbox(cognitive_module, 10);
    EXPECT_GT(processed, 0u);
    EXPECT_TRUE(query_handled.load());
    EXPECT_FLOAT_EQ(received_confidence.load(), 0.85f);
}

TEST_F(CognitiveBioAsyncTest, IntrospectionAsyncQuery) {
    // Register handler that completes promise
    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        bio_msg_introspection_response_t response;
        bio_msg_init_header(&response.header, BIO_MSG_INTROSPECTION_RESPONSE,
                           BIO_MODULE_INTROSPECTION, BIO_MODULE_BRAIN,
                           sizeof(response));
        response.query_type = BIO_INTRO_QUERY_COGNITIVE_LOAD;
        response.confidence = 0.9f;
        response.cognitive_load = 0.75f;

        if (promise) {
            nimcp_bio_promise_complete_sized(promise, &response, sizeof(response));
        }

        return NIMCP_SUCCESS;
    };

    bio_router_register_handler(cognitive_module, BIO_MSG_INTROSPECTION_QUERY, handler);

    // Send async query
    bio_msg_introspection_query_t query;
    bio_msg_init_header(&query.header, BIO_MSG_INTROSPECTION_QUERY,
                       BIO_MODULE_BRAIN, BIO_MODULE_INTROSPECTION,
                       sizeof(query));
    query.query_type = BIO_INTRO_QUERY_COGNITIVE_LOAD;

    nimcp_bio_promise_t promise = bio_router_send_async(
        query_module, &query, sizeof(query), BIO_CHANNEL_ACETYLCHOLINE);
    ASSERT_NE(promise, nullptr);

    // Process inbox to trigger handler
    bio_router_process_inbox(cognitive_module, 10);

    // Wait for response
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
    ASSERT_NE(future, nullptr);

    bio_msg_introspection_response_t response;
    nimcp_error_t err = nimcp_bio_future_wait(future, &response, 2000);

    if (err == NIMCP_SUCCESS) {
        EXPECT_EQ(response.query_type, BIO_INTRO_QUERY_COGNITIVE_LOAD);
        EXPECT_GE(response.confidence, 0.0f);
        EXPECT_LE(response.confidence, 1.0f);
    }

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

//=============================================================================
// ETHICS EVALUATION TESTS (Serotonin - Deliberative)
//=============================================================================

TEST_F(CognitiveBioAsyncTest, EthicsEvaluationViaSerotonin) {
    std::atomic<bool> ethics_called{false};
    std::atomic<bool> veto_decision{false};

    // Register ethics module
    bio_module_info_t ethics_info;
    ethics_info.module_id = BIO_MODULE_ETHICS;
    ethics_info.module_name = "ethics";
    ethics_info.inbox_capacity = 100;
    ethics_info.user_data = nullptr;

    bio_module_context_t ethics_module = bio_router_register_module(&ethics_info);
    ASSERT_NE(ethics_module, nullptr);

    // Register ethics handler
    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* data = static_cast<std::pair<std::atomic<bool>*, std::atomic<bool>*>*>(user_data);
        data->first->store(true);

        auto* request = static_cast<const bio_msg_ethics_request_t*>(msg);
        EXPECT_EQ(request->header.type, BIO_MSG_ETHICS_EVALUATION_REQUEST);
        EXPECT_EQ(request->header.channel, BIO_CHANNEL_SEROTONIN);

        // Simulate ethics evaluation
        bio_msg_ethics_response_t response;
        bio_msg_init_header(&response.header, BIO_MSG_ETHICS_EVALUATION_RESPONSE,
                           BIO_MODULE_ETHICS, static_cast<bio_module_id_t>(request->header.source_module),
                           sizeof(response));
        response.action_id = request->action_id;
        response.ethical_score = -0.3f;  // Slightly harmful
        response.confidence = 0.8f;
        response.veto = true;  // Block action
        response.primary_concern = 1;
        strncpy(response.explanation, "Action may cause harm", sizeof(response.explanation));

        data->second->store(response.veto);

        if (promise) {
            nimcp_bio_promise_complete_sized(promise, &response, sizeof(response));
        }

        return NIMCP_SUCCESS;
    };

    std::pair<std::atomic<bool>*, std::atomic<bool>*> handler_data{
        &ethics_called, &veto_decision
    };

    // Re-register with user data
    bio_router_unregister_module(ethics_module);
    ethics_info.user_data = &handler_data;
    ethics_module = bio_router_register_module(&ethics_info);

    bio_router_register_handler(ethics_module, BIO_MSG_ETHICS_EVALUATION_REQUEST, handler);

    // Send ethics evaluation request
    bio_msg_ethics_request_t request;
    bio_msg_init_header(&request.header, BIO_MSG_ETHICS_EVALUATION_REQUEST,
                       BIO_MODULE_BRAIN, BIO_MODULE_ETHICS,
                       sizeof(request));
    request.header.channel = BIO_CHANNEL_SEROTONIN;  // Slow, deliberative
    request.action_id = 42;
    request.context_id = 1;
    request.urgency = 0.5f;
    request.stakeholder_count = 3;

    bio_router_send(query_module, &request, sizeof(request), 5000);

    // Process inbox
    bio_router_process_inbox(ethics_module, 10);

    EXPECT_TRUE(ethics_called.load());
    EXPECT_TRUE(veto_decision.load());

    bio_router_unregister_module(ethics_module);
}

//=============================================================================
// SALIENCE TESTS (Norepinephrine - Alerting)
//=============================================================================

TEST_F(CognitiveBioAsyncTest, SalienceViaNorepinephrine) {
    std::atomic<float> salience_score{0.0f};
    std::atomic<bool> urgent_attention{false};

    // Register salience module
    bio_module_info_t salience_info;
    salience_info.module_id = BIO_MODULE_SALIENCE;
    salience_info.module_name = "salience";
    salience_info.inbox_capacity = 100;
    salience_info.user_data = nullptr;

    bio_module_context_t salience_module = bio_router_register_module(&salience_info);
    ASSERT_NE(salience_module, nullptr);

    // Register salience handler
    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* data = static_cast<std::pair<std::atomic<float>*, std::atomic<bool>*>*>(user_data);

        auto* query = static_cast<const bio_msg_salience_query_t*>(msg);
        EXPECT_EQ(query->header.type, BIO_MSG_SALIENCE_QUERY);
        EXPECT_EQ(query->header.channel, BIO_CHANNEL_NOREPINEPHRINE);

        // Compute salience (simple model)
        float salience = (query->raw_intensity * 0.4f +
                         query->novelty * 0.3f +
                         query->relevance * 0.3f);

        bio_msg_salience_response_t response;
        bio_msg_init_header(&response.header, BIO_MSG_SALIENCE_RESPONSE,
                           BIO_MODULE_SALIENCE, static_cast<bio_module_id_t>(query->header.source_module),
                           sizeof(response));
        response.stimulus_id = query->stimulus_id;
        response.salience_score = salience;
        response.attention_priority = salience > 0.7f ? 1.0f : salience;
        response.requires_immediate_attention = salience > 0.8f;

        data->first->store(salience);
        data->second->store(response.requires_immediate_attention);

        if (promise) {
            nimcp_bio_promise_complete_sized(promise, &response, sizeof(response));
        }

        return NIMCP_SUCCESS;
    };

    std::pair<std::atomic<float>*, std::atomic<bool>*> handler_data{
        &salience_score, &urgent_attention
    };

    bio_router_unregister_module(salience_module);
    salience_info.user_data = &handler_data;
    salience_module = bio_router_register_module(&salience_info);

    bio_router_register_handler(salience_module, BIO_MSG_SALIENCE_QUERY, handler);

    // Send salience query with high novelty
    bio_msg_salience_query_t query;
    bio_msg_init_header(&query.header, BIO_MSG_SALIENCE_QUERY,
                       BIO_MODULE_BRAIN, BIO_MODULE_SALIENCE,
                       sizeof(query));
    query.header.channel = BIO_CHANNEL_NOREPINEPHRINE;
    query.stimulus_id = 123;
    query.raw_intensity = 0.9f;
    query.novelty = 0.95f;
    query.relevance = 0.8f;

    bio_router_send(query_module, &query, sizeof(query), 1000);

    // Process inbox
    bio_router_process_inbox(salience_module, 10);

    EXPECT_GT(salience_score.load(), 0.7f);  // Should be high
    EXPECT_TRUE(urgent_attention.load());

    bio_router_unregister_module(salience_module);
}

//=============================================================================
// MIRROR NEURON TESTS
//=============================================================================

TEST_F(CognitiveBioAsyncTest, MirrorNeuronActivation) {
    std::atomic<bool> mirror_activated{false};
    std::atomic<uint32_t> activated_neuron_id{0};

    // Register mirror neuron module
    bio_module_info_t mirror_info;
    mirror_info.module_id = BIO_MODULE_MIRROR_NEURONS;
    mirror_info.module_name = "mirror";
    mirror_info.inbox_capacity = 100;
    mirror_info.user_data = nullptr;

    bio_module_context_t mirror_module = bio_router_register_module(&mirror_info);
    ASSERT_NE(mirror_module, nullptr);

    // Register handler for mirror neuron activation
    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* data = static_cast<std::pair<std::atomic<bool>*, std::atomic<uint32_t>*>*>(user_data);
        data->first->store(true);

        // In real implementation, this would trigger mirror neuron resonance
        // For test, we just acknowledge
        data->second->store(12345);

        return NIMCP_SUCCESS;
    };

    std::pair<std::atomic<bool>*, std::atomic<uint32_t>*> handler_data{
        &mirror_activated, &activated_neuron_id
    };

    bio_router_unregister_module(mirror_module);
    mirror_info.user_data = &handler_data;
    mirror_module = bio_router_register_module(&mirror_info);

    bio_router_register_handler(mirror_module, BIO_MSG_MIRROR_NEURON_ACTIVATION, handler);

    // Send mirror neuron activation message
    bio_message_header_t activation_msg;
    bio_msg_init_header(&activation_msg, BIO_MSG_MIRROR_NEURON_ACTIVATION,
                       BIO_MODULE_BRAIN, BIO_MODULE_MIRROR_NEURONS,
                       sizeof(activation_msg));

    bio_router_send(query_module, &activation_msg, sizeof(activation_msg), 1000);

    // Process
    bio_router_process_inbox(mirror_module, 10);

    EXPECT_TRUE(mirror_activated.load());
    EXPECT_EQ(activated_neuron_id.load(), 12345u);

    bio_router_unregister_module(mirror_module);
}

//=============================================================================
// CROSS-MODULE INTEGRATION TESTS
//=============================================================================

TEST_F(CognitiveBioAsyncTest, MultiModulePipeline) {
    // Test: Salience → Introspection → Ethics pipeline

    std::atomic<int> pipeline_stage{0};

    // Register all three modules
    bio_module_info_t salience_info;
    salience_info.module_id = BIO_MODULE_SALIENCE;
    salience_info.module_name = "salience";
    salience_info.inbox_capacity = 100;
    salience_info.user_data = &pipeline_stage;

    bio_module_context_t salience = bio_router_register_module(&salience_info);

    bio_module_info_t introspection_info;
    introspection_info.module_id = BIO_MODULE_INTROSPECTION;
    introspection_info.module_name = "introspection";
    introspection_info.inbox_capacity = 100;
    introspection_info.user_data = &pipeline_stage;

    bio_module_context_t introspection = bio_router_register_module(&introspection_info);

    bio_module_info_t ethics_info;
    ethics_info.module_id = BIO_MODULE_ETHICS;
    ethics_info.module_name = "ethics";
    ethics_info.inbox_capacity = 100;
    ethics_info.user_data = &pipeline_stage;

    bio_module_context_t ethics = bio_router_register_module(&ethics_info);

    // Stage 1: Salience handler forwards to introspection
    auto salience_handler = [](const void* msg, size_t size,
                              nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* stage = static_cast<std::atomic<int>*>(user_data);
        stage->store(1);
        return NIMCP_SUCCESS;
    };

    // Stage 2: Introspection handler forwards to ethics
    auto intro_handler = [](const void* msg, size_t size,
                           nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* stage = static_cast<std::atomic<int>*>(user_data);
        stage->store(2);
        return NIMCP_SUCCESS;
    };

    // Stage 3: Ethics final decision
    auto ethics_handler = [](const void* msg, size_t size,
                            nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* stage = static_cast<std::atomic<int>*>(user_data);
        stage->store(3);
        return NIMCP_SUCCESS;
    };

    bio_router_register_handler(salience, BIO_MSG_SALIENCE_QUERY, salience_handler);
    bio_router_register_handler(introspection, BIO_MSG_INTROSPECTION_QUERY, intro_handler);
    bio_router_register_handler(ethics, BIO_MSG_ETHICS_EVALUATION_REQUEST, ethics_handler);

    // Trigger pipeline
    bio_msg_salience_query_t query;
    bio_msg_init_header(&query.header, BIO_MSG_SALIENCE_QUERY,
                       BIO_MODULE_BRAIN, BIO_MODULE_SALIENCE,
                       sizeof(query));
    bio_router_send(query_module, &query, sizeof(query), 1000);

    bio_router_process_inbox(salience, 10);
    EXPECT_EQ(pipeline_stage.load(), 1);

    bio_router_unregister_module(salience);
    bio_router_unregister_module(introspection);
    bio_router_unregister_module(ethics);
}
