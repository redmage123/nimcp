/**
 * @file e2e_test_bio_async_cognitive_pipeline.cpp
 * @brief E2E Tests for Bio-Async Cognitive Pipeline
 *
 * WHAT: Complete cognitive processing pipelines using bio-async messaging
 * WHY:  Verify reasoning, ethics, salience, attention coordination via bio-async
 * HOW:  Test cognitive modules communicating through bio-router with appropriate channels
 *
 * TEST PIPELINES:
 * - ReasoningPipeline: Symbolic reasoning with async query/response
 * - EthicsPipeline: Ethical evaluation with deliberative serotonin channel
 * - AttentionPipeline: Attention shifts via fast acetylcholine channel
 * - CognitiveFusionPipeline: Multi-module cognitive coordination
 *
 * @author NIMCP Development Team
 * @date 2025-12-03
 * @version 1.0.0
 */

#include "e2e_test_framework.h"
#include <thread>
#include <atomic>
#include <vector>
#include <cmath>
#include <string>

// Headers have their own extern "C" guards
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "cognitive/introspection/nimcp_introspection.h"
#include "cognitive/ethics/nimcp_ethics.h"
#include "cognitive/salience/nimcp_salience.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BioAsyncCognitiveE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize bio-async system
        nimcp_error_t err = nimcp_bio_async_init(NULL);
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Bio-async initialization failed";

        // Initialize router
        err = bio_router_init(NULL);
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Router initialization failed";

        // Register introspection module
        bio_module_info_t intro_info = {
            .module_id = BIO_MODULE_INTROSPECTION,
            .module_name = "introspection_test",
            .inbox_capacity = 100,
            .user_data = nullptr
        };
        intro_ctx_ = bio_router_register_module(&intro_info);
        ASSERT_NE(intro_ctx_, nullptr) << "Failed to register introspection module";

        // Register ethics module
        bio_module_info_t ethics_info = {
            .module_id = BIO_MODULE_ETHICS,
            .module_name = "ethics_test",
            .inbox_capacity = 100,
            .user_data = nullptr
        };
        ethics_ctx_ = bio_router_register_module(&ethics_info);
        ASSERT_NE(ethics_ctx_, nullptr) << "Failed to register ethics module";

        // Register salience module
        bio_module_info_t salience_info = {
            .module_id = BIO_MODULE_SALIENCE,
            .module_name = "salience_test",
            .inbox_capacity = 100,
            .user_data = nullptr
        };
        salience_ctx_ = bio_router_register_module(&salience_info);
        ASSERT_NE(salience_ctx_, nullptr) << "Failed to register salience module";

        // Register attention module
        bio_module_info_t attention_info = {
            .module_id = BIO_MODULE_ATTENTION,
            .module_name = "attention_test",
            .inbox_capacity = 100,
            .user_data = nullptr
        };
        attention_ctx_ = bio_router_register_module(&attention_info);
        ASSERT_NE(attention_ctx_, nullptr) << "Failed to register attention module";

        // Reset counters
        introspection_queries_.store(0);
        ethics_evaluations_.store(0);
        salience_queries_.store(0);
        attention_shifts_.store(0);
    }

    void TearDown() override {
        if (intro_ctx_) bio_router_unregister_module(intro_ctx_);
        if (ethics_ctx_) bio_router_unregister_module(ethics_ctx_);
        if (salience_ctx_) bio_router_unregister_module(salience_ctx_);
        if (attention_ctx_) bio_router_unregister_module(attention_ctx_);

        bio_router_shutdown();
        nimcp_bio_async_shutdown();
    }

    bio_module_context_t intro_ctx_{nullptr};
    bio_module_context_t ethics_ctx_{nullptr};
    bio_module_context_t salience_ctx_{nullptr};
    bio_module_context_t attention_ctx_{nullptr};

public:
    static std::atomic<int> introspection_queries_;
    static std::atomic<int> ethics_evaluations_;
    static std::atomic<int> salience_queries_;
    static std::atomic<int> attention_shifts_;
    static std::atomic<float> last_ethics_score_;
    static std::atomic<float> last_salience_score_;
};

// Static member initialization
std::atomic<int> BioAsyncCognitiveE2ETest::introspection_queries_{0};
std::atomic<int> BioAsyncCognitiveE2ETest::ethics_evaluations_{0};
std::atomic<int> BioAsyncCognitiveE2ETest::salience_queries_{0};
std::atomic<int> BioAsyncCognitiveE2ETest::attention_shifts_{0};
std::atomic<float> BioAsyncCognitiveE2ETest::last_ethics_score_{0.0f};
std::atomic<float> BioAsyncCognitiveE2ETest::last_salience_score_{0.0f};

//=============================================================================
// Static Handler Functions (No Lambda Captures)
//=============================================================================

static nimcp_error_t intro_query_handler_static(const void* msg, size_t msg_size,
                                                  nimcp_bio_promise_t response_promise,
                                                  void* user_data) {
    const bio_msg_introspection_query_t* query =
        static_cast<const bio_msg_introspection_query_t*>(msg);

    BioAsyncCognitiveE2ETest::introspection_queries_.fetch_add(1);

    if (response_promise) {
        bio_msg_introspection_response_t response;
        bio_msg_init_header(&response.header, BIO_MSG_INTROSPECTION_RESPONSE,
                            BIO_MODULE_INTROSPECTION, static_cast<bio_module_id_t>(query->header.source_module),
                            sizeof(response));
        response.query_type = query->query_type;
        response.confidence = 0.85f;
        response.cognitive_load = 0.65f;
        response.emotional_valence = 0.3f;
        response.arousal = 0.5f;
        response.matched_pattern_count = 3;

        nimcp_bio_promise_complete(response_promise, &response);
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t ethics_handler_static(const void* msg, size_t msg_size,
                                            nimcp_bio_promise_t response_promise,
                                            void* user_data) {
    const bio_msg_ethics_request_t* request =
        reinterpret_cast<const bio_msg_ethics_request_t*>(msg);

    BioAsyncCognitiveE2ETest::ethics_evaluations_.fetch_add(1);

    if (response_promise) {
        bio_msg_ethics_response_t response;
        bio_msg_init_header(&response.header, BIO_MSG_ETHICS_EVALUATION_RESPONSE,
                            BIO_MODULE_ETHICS, static_cast<bio_module_id_t>(request->header.source_module),
                            sizeof(response));
        response.action_id = request->action_id;

        // Simulate ethics evaluation
        float score = (request->action_id % 2 == 0) ? 0.7f : -0.3f;
        response.ethical_score = score;
        response.confidence = 0.8f;
        response.veto = (score < 0);
        response.primary_concern = 1; // Harm principle
        snprintf(response.explanation, sizeof(response.explanation),
                 "Action %u evaluated with score %.2f", request->action_id, score);

        BioAsyncCognitiveE2ETest::last_ethics_score_.store(score);

        nimcp_bio_promise_complete(response_promise, &response);
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t salience_handler_static(const void* msg, size_t msg_size,
                                              nimcp_bio_promise_t response_promise,
                                              void* user_data) {
    const bio_msg_salience_query_t* query =
        static_cast<const bio_msg_salience_query_t*>(msg);

    BioAsyncCognitiveE2ETest::salience_queries_.fetch_add(1);

    if (response_promise) {
        bio_msg_salience_response_t response;
        bio_msg_init_header(&response.header, BIO_MSG_SALIENCE_RESPONSE,
                            BIO_MODULE_SALIENCE, static_cast<bio_module_id_t>(query->header.source_module),
                            sizeof(response));
        response.stimulus_id = query->stimulus_id;

        // Calculate salience from intensity, novelty, relevance
        float salience = (query->raw_intensity * 0.3f +
                          query->novelty * 0.4f +
                          query->relevance * 0.3f);
        response.salience_score = salience;
        response.attention_priority = salience > 0.7f ? 0.9f : 0.5f;
        response.requires_immediate_attention = (salience > 0.8f);

        BioAsyncCognitiveE2ETest::last_salience_score_.store(salience);

        nimcp_bio_promise_complete(response_promise, &response);
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t attention_handler_static(const void* msg, size_t msg_size,
                                               nimcp_bio_promise_t response_promise,
                                               void* user_data) {
    BioAsyncCognitiveE2ETest::attention_shifts_.fetch_add(1);

    if (response_promise) {
        float ack = 1.0f;
        nimcp_bio_promise_complete(response_promise, &ack);
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t unified_cognitive_handler_static(const void* msg, size_t msg_size,
                                                       nimcp_bio_promise_t response_promise,
                                                       void* user_data) {
    std::atomic<int>* counter = static_cast<std::atomic<int>*>(user_data);
    if (counter) {
        counter->fetch_add(1);
    }

    if (response_promise) {
        float ack = 1.0f;
        nimcp_bio_promise_complete(response_promise, &ack);
    }

    return NIMCP_SUCCESS;
}

//=============================================================================
// Pipeline 1: Introspection Query/Response
//=============================================================================

TEST_F(BioAsyncCognitiveE2ETest, IntrospectionPipeline) {
    E2E_PIPELINE_START("Introspection Query/Response via Bio-Async");

    // Stage 1: Register introspection handlers
    E2E_STAGE_BEGIN("Register introspection handlers", 100);

    bio_router_register_handler(intro_ctx_, BIO_MSG_INTROSPECTION_QUERY,
                                 intro_query_handler_static);

    E2E_STAGE_END();

    // Stage 2: Send introspection queries
    E2E_STAGE_BEGIN("Send introspection queries", 300);

    const int NUM_QUERIES = 10;
    std::vector<nimcp_bio_promise_t> query_promises;

    for (int i = 0; i < NUM_QUERIES; i++) {
        bio_msg_introspection_query_t query;
        bio_msg_init_header(&query.header, BIO_MSG_INTROSPECTION_QUERY,
                            BIO_MODULE_BRAIN, BIO_MODULE_INTROSPECTION, sizeof(query));
        query.query_type = BIO_INTRO_QUERY_PATTERN_MATCH + (i % 4);
        query.target_pattern_id = i;
        query.confidence_threshold = 0.7f;

        nimcp_bio_promise_t promise = bio_router_send_async(
            intro_ctx_, &query, sizeof(query),
            BIO_CHANNEL_ACETYLCHOLINE); // Fast channel for queries
        E2E_ASSERT_NOT_NULL(promise, "Failed to send introspection query");

        query_promises.push_back(promise);
    }

    E2E_STAGE_END();

    // Stage 3: Process introspection queries
    E2E_STAGE_BEGIN("Process introspection queries", 500);

    auto start = std::chrono::steady_clock::now();
    while (introspection_queries_.load() < NUM_QUERIES &&
           std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start).count() < 1000) {
        bio_router_process_inbox(intro_ctx_, 10);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    E2E_ASSERT(introspection_queries_.load() >= NUM_QUERIES / 2,
               "Expected at least half of introspection queries to be processed");

    E2E_STAGE_END();

    // Stage 4: Verify acetylcholine channel used (fast decay)
    E2E_STAGE_BEGIN("Verify fast channel characteristics", 200);

    nimcp_bio_async_stats_t stats;
    nimcp_error_t err = nimcp_bio_async_get_stats(&stats);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Failed to get bio-async stats");

    // Acetylcholine should show activity
    E2E_ASSERT(stats.channel_stats[BIO_CHANNEL_ACETYLCHOLINE].releases > 0,
               "Expected acetylcholine channel activity for queries");

    E2E_STAGE_END();

    // Cleanup
    for (auto promise : query_promises) {
        nimcp_bio_promise_destroy(promise);
    }

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 2: Ethics Evaluation
//=============================================================================

TEST_F(BioAsyncCognitiveE2ETest, EthicsPipeline) {
    E2E_PIPELINE_START("Ethics Evaluation via Bio-Async");

    // Stage 1: Register ethics handlers
    E2E_STAGE_BEGIN("Register ethics handlers", 100);

    bio_router_register_handler(ethics_ctx_, BIO_MSG_ETHICS_EVALUATION_REQUEST,
                                 ethics_handler_static);

    E2E_STAGE_END();

    // Stage 2: Send ethics evaluation requests
    E2E_STAGE_BEGIN("Send ethics evaluation requests", 400);

    const int NUM_EVALUATIONS = 8;
    std::vector<nimcp_bio_promise_t> ethics_promises;

    for (int i = 0; i < NUM_EVALUATIONS; i++) {
        bio_msg_ethics_request_t request;
        bio_msg_init_header(&request.header, BIO_MSG_ETHICS_EVALUATION_REQUEST,
                            BIO_MODULE_EXECUTIVE, BIO_MODULE_ETHICS, sizeof(request));
        request.action_id = i;
        request.context_id = 100 + i;
        request.urgency = 0.5f + i * 0.05f;
        request.stakeholder_count = 3;

        nimcp_bio_promise_t promise = bio_router_send_async(
            ethics_ctx_, &request, sizeof(request),
            BIO_CHANNEL_SEROTONIN); // Slow, deliberative channel for ethics
        E2E_ASSERT_NOT_NULL(promise, "Failed to send ethics request");

        ethics_promises.push_back(promise);
    }

    E2E_STAGE_END();

    // Stage 3: Process ethics evaluations
    E2E_STAGE_BEGIN("Process ethics evaluations", 800);

    auto start = std::chrono::steady_clock::now();
    while (ethics_evaluations_.load() < NUM_EVALUATIONS &&
           std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start).count() < 1500) {
        bio_router_process_inbox(ethics_ctx_, 10);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    E2E_ASSERT(ethics_evaluations_.load() >= NUM_EVALUATIONS / 2,
               "Expected at least half of ethics evaluations to be processed");

    E2E_STAGE_END();

    // Stage 4: Verify ethics scores
    E2E_STAGE_BEGIN("Verify ethics evaluation results", 100);

    float last_score = last_ethics_score_.load();
    E2E_ASSERT(std::abs(last_score) <= 1.0f, "Ethics score out of range [-1, 1]");

    // Verify serotonin channel characteristics (slow decay)
    nimcp_bio_async_stats_t stats;
    nimcp_error_t err = nimcp_bio_async_get_stats(&stats);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Failed to get bio-async stats");

    E2E_ASSERT(stats.channel_stats[BIO_CHANNEL_SEROTONIN].releases > 0,
               "Expected serotonin channel activity for ethics");

    E2E_STAGE_END();

    // Cleanup
    for (auto promise : ethics_promises) {
        nimcp_bio_promise_destroy(promise);
    }

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 3: Salience and Attention Coordination
//=============================================================================

TEST_F(BioAsyncCognitiveE2ETest, AttentionPipeline) {
    E2E_PIPELINE_START("Salience and Attention Coordination via Bio-Async");

    // Stage 1: Register salience and attention handlers
    E2E_STAGE_BEGIN("Register salience and attention handlers", 100);

    bio_router_register_handler(salience_ctx_, BIO_MSG_SALIENCE_QUERY, salience_handler_static);
    bio_router_register_handler(attention_ctx_, BIO_MSG_ATTENTION_SHIFT, attention_handler_static);

    E2E_STAGE_END();

    // Stage 2: Send salience queries
    E2E_STAGE_BEGIN("Send salience queries", 300);

    const int NUM_STIMULI = 15;
    std::vector<nimcp_bio_promise_t> salience_promises;

    for (int i = 0; i < NUM_STIMULI; i++) {
        bio_msg_salience_query_t query;
        bio_msg_init_header(&query.header, BIO_MSG_SALIENCE_QUERY,
                            BIO_MODULE_BRAIN, BIO_MODULE_SALIENCE, sizeof(query));
        query.stimulus_id = i;
        query.raw_intensity = 0.5f + (i % 5) * 0.1f;
        query.novelty = (i % 3 == 0) ? 0.9f : 0.3f;
        query.relevance = 0.6f + (i % 4) * 0.1f;

        nimcp_bio_promise_t promise = bio_router_send_async(
            salience_ctx_, &query, sizeof(query),
            BIO_CHANNEL_NOREPINEPHRINE); // Alerting channel for salience
        E2E_ASSERT_NOT_NULL(promise, "Failed to send salience query");

        salience_promises.push_back(promise);
    }

    E2E_STAGE_END();

    // Stage 3: Process salience queries
    E2E_STAGE_BEGIN("Process salience queries", 500);

    auto start = std::chrono::steady_clock::now();
    while (salience_queries_.load() < NUM_STIMULI &&
           std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start).count() < 1000) {
        bio_router_process_inbox(salience_ctx_, 10);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    E2E_ASSERT(salience_queries_.load() >= NUM_STIMULI / 2,
               "Expected at least half of salience queries to be processed");

    E2E_STAGE_END();

    // Stage 4: Send attention shift commands for high-salience stimuli
    E2E_STAGE_BEGIN("Send attention shift commands", 300);

    const int NUM_SHIFTS = 5;
    std::vector<nimcp_bio_promise_t> attention_promises;

    for (int i = 0; i < NUM_SHIFTS; i++) {
        bio_msg_attention_shift_t shift;
        bio_msg_init_header(&shift.header, BIO_MSG_ATTENTION_SHIFT,
                            BIO_MODULE_SALIENCE, BIO_MODULE_ATTENTION, sizeof(shift));
        shift.target_id = i * 3; // High salience stimuli
        shift.attention_weight = 0.8f + i * 0.04f;
        shift.duration_ms = 500;
        shift.preemptive = (i % 2 == 0);

        nimcp_bio_promise_t promise = bio_router_send_async(
            attention_ctx_, &shift, sizeof(shift),
            BIO_CHANNEL_ACETYLCHOLINE); // Fast channel for attention shifts
        E2E_ASSERT_NOT_NULL(promise, "Failed to send attention shift");

        attention_promises.push_back(promise);
    }

    E2E_STAGE_END();

    // Stage 5: Process attention shifts
    E2E_STAGE_BEGIN("Process attention shifts", 500);

    start = std::chrono::steady_clock::now();
    while (attention_shifts_.load() < NUM_SHIFTS &&
           std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start).count() < 1000) {
        bio_router_process_inbox(attention_ctx_, 10);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    E2E_ASSERT(attention_shifts_.load() >= NUM_SHIFTS / 2,
               "Expected at least half of attention shifts to be processed");

    E2E_STAGE_END();

    // Cleanup
    for (auto promise : salience_promises) {
        nimcp_bio_promise_destroy(promise);
    }
    for (auto promise : attention_promises) {
        nimcp_bio_promise_destroy(promise);
    }

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 4: Cognitive Fusion (Multi-Module Coordination)
//=============================================================================

TEST_F(BioAsyncCognitiveE2ETest, CognitiveFusionPipeline) {
    E2E_PIPELINE_START("Cognitive Fusion: Multi-Module Coordination");

    static std::atomic<int> total_cognitive_messages{0};

    // Stage 1: Re-register with user_data
    E2E_STAGE_BEGIN("Register unified cognitive handlers", 100);

    // Unregister and re-register with user_data
    bio_router_unregister_module(intro_ctx_);
    bio_module_info_t intro_info = {
        .module_id = BIO_MODULE_INTROSPECTION,
        .module_name = "introspection_test",
        .inbox_capacity = 100,
        .user_data = &total_cognitive_messages
    };
    intro_ctx_ = bio_router_register_module(&intro_info);
    ASSERT_NE(intro_ctx_, nullptr);

    // Register category handler for all cognitive messages (0x0300-0x03FF)
    bio_router_register_category_handler(intro_ctx_, 0x0300, unified_cognitive_handler_static);

    E2E_STAGE_END();

    // Stage 2: Send mixed cognitive messages
    E2E_STAGE_BEGIN("Send mixed cognitive messages", 500);

    std::vector<nimcp_bio_promise_t> mixed_promises;

    // Send introspection queries - all to intro_ctx_ since it has the category handler
    for (int i = 0; i < 3; i++) {
        bio_msg_introspection_query_t query;
        bio_msg_init_header(&query.header, BIO_MSG_INTROSPECTION_QUERY,
                            BIO_MODULE_EXECUTIVE, BIO_MODULE_INTROSPECTION, sizeof(query));
        query.query_type = BIO_INTRO_QUERY_COGNITIVE_LOAD;
        query.target_pattern_id = i;
        query.confidence_threshold = 0.7f;

        nimcp_bio_promise_t promise = bio_router_send_async(
            intro_ctx_, &query, sizeof(query), BIO_CHANNEL_ACETYLCHOLINE);
        if (promise) mixed_promises.push_back(promise);
    }

    // Send ethics evaluations - send to intro_ctx_ which has category handler for 0x0300
    for (int i = 0; i < 3; i++) {
        bio_msg_ethics_request_t request;
        bio_msg_init_header(&request.header, BIO_MSG_ETHICS_EVALUATION_REQUEST,
                            BIO_MODULE_EXECUTIVE, BIO_MODULE_INTROSPECTION, sizeof(request));
        request.action_id = i;
        request.context_id = 200 + i;
        request.urgency = 0.6f;
        request.stakeholder_count = 2;

        nimcp_bio_promise_t promise = bio_router_send_async(
            intro_ctx_, &request, sizeof(request), BIO_CHANNEL_SEROTONIN);
        if (promise) mixed_promises.push_back(promise);
    }

    // Send salience queries - send to intro_ctx_ which has category handler for 0x0300
    for (int i = 0; i < 4; i++) {
        bio_msg_salience_query_t query;
        bio_msg_init_header(&query.header, BIO_MSG_SALIENCE_QUERY,
                            BIO_MODULE_BRAIN, BIO_MODULE_INTROSPECTION, sizeof(query));
        query.stimulus_id = i;
        query.raw_intensity = 0.7f;
        query.novelty = 0.8f;
        query.relevance = 0.6f;

        nimcp_bio_promise_t promise = bio_router_send_async(
            intro_ctx_, &query, sizeof(query), BIO_CHANNEL_NOREPINEPHRINE);
        if (promise) mixed_promises.push_back(promise);
    }

    E2E_STAGE_END();

    // Stage 3: Process all cognitive messages
    E2E_STAGE_BEGIN("Process all cognitive messages", 1000);

    auto start = std::chrono::steady_clock::now();
    while (total_cognitive_messages.load() < static_cast<int>(mixed_promises.size()) &&
           std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start).count() < 2000) {
        bio_router_process_inbox(intro_ctx_, 20);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    E2E_ASSERT(total_cognitive_messages.load() >= static_cast<int>(mixed_promises.size()) / 2,
               "Expected at least half of cognitive messages to be processed");

    E2E_STAGE_END();

    // Stage 4: Verify multi-channel usage
    E2E_STAGE_BEGIN("Verify multi-channel usage", 100);

    nimcp_bio_async_stats_t stats;
    nimcp_error_t err = nimcp_bio_async_get_stats(&stats);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Failed to get bio-async stats");

    // Verify multiple channels were used
    int active_channels = 0;
    for (int i = 0; i < BIO_CHANNEL_COUNT; i++) {
        if (stats.channel_stats[i].releases > 0) {
            active_channels++;
        }
    }

    E2E_ASSERT(active_channels >= 2, "Expected at least 2 active channels");

    E2E_STAGE_END();

    // Stage 5: Verify router statistics
    E2E_STAGE_BEGIN("Verify router statistics", 100);

    bio_router_stats_t router_stats;
    err = bio_router_get_stats(&router_stats);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Failed to get router stats");

    E2E_ASSERT(router_stats.messages_routed >= static_cast<uint64_t>(mixed_promises.size()) / 2,
               "Expected messages to be routed through bio-router");

    E2E_STAGE_END();

    // Cleanup
    for (auto promise : mixed_promises) {
        nimcp_bio_promise_destroy(promise);
    }

    E2E_PIPELINE_END();
}
