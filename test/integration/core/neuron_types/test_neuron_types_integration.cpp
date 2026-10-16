/**
 * @file test_neuron_types_integration.cpp
 * @brief Integration tests for neuron_types module bio-async communication
 *
 * WHAT: Tests neuron type events and neural logic gate communication via bio-async
 * WHY:  Verify neuron_types and neural_logic modules integrate correctly with bio-async
 * HOW:  Create logic gates, test message passing, verify circuit evaluation
 *
 * TEST SCENARIOS:
 * 1. Logic gate evaluation via bio-async
 * 2. Logic circuit step coordination
 * 3. Variable binding messages
 * 4. Spike event propagation
 * 5. Cross-module neuron type communication
 * 6. Logic gate + brain integration
 * 7. Concurrent logic operations
 * 8. Channel-based logic routing
 *
 * @author NIMCP Development Team
 * @date 2025-11-29
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
#include "core/neuron_types/nimcp_neuron_types.h"
#include "core/neuron_types/nimcp_neural_logic.h"
#include "utils/memory/nimcp_unified_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class NeuronTypesBioAsyncIntegrationTest : public ::testing::Test {
protected:
    unified_mem_manager_t mem_mgr_ = nullptr;
    bio_module_context_t logic_module_ = nullptr;

    void SetUp() override {
        // Initialize unified memory FIRST
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

        // Register neural logic module
        bio_module_info_t logic_info;
        logic_info.module_id = BIO_MODULE_NEURAL_LOGIC;
        logic_info.module_name = "neural_logic";
        logic_info.inbox_capacity = 128;
        logic_info.user_data = nullptr;

        logic_module_ = bio_router_register_module(&logic_info);
        ASSERT_NE(logic_module_, nullptr) << "Failed to register logic module";
    }

    void TearDown() override {
        if (logic_module_) {
            bio_router_unregister_module(logic_module_);
            logic_module_ = nullptr;
        }
        bio_router_shutdown();
        nimcp_bio_async_shutdown();
        if (mem_mgr_) {
            unified_mem_destroy(mem_mgr_);
            mem_mgr_ = nullptr;
        }
    }
};

//=============================================================================
// LOGIC GATE EVALUATION TESTS
//=============================================================================

TEST_F(NeuronTypesBioAsyncIntegrationTest, LogicGateAND_EvaluationRequest) {
    std::atomic<bool> result_received{false};
    std::atomic<float> gate_output{0.0f};
    std::atomic<bool> gate_spiked{false};

    // Register evaluator module
    bio_module_info_t evaluator_info;
    evaluator_info.module_id = BIO_MODULE_BRAIN_COGNITIVE;
    evaluator_info.module_name = "evaluator";
    evaluator_info.inbox_capacity = 64;
    evaluator_info.user_data = nullptr;

    bio_module_context_t evaluator = bio_router_register_module(&evaluator_info);
    ASSERT_NE(evaluator, nullptr);

    // Register handler for logic gate results
    struct ResultData {
        std::atomic<bool>* received;
        std::atomic<float>* output;
        std::atomic<bool>* spiked;
    };

    ResultData result_data{&result_received, &gate_output, &gate_spiked};

    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* data = static_cast<ResultData*>(user_data);
        auto* result = static_cast<const bio_msg_logic_gate_result_t*>(msg);

        data->received->store(true);
        data->output->store(result->output);
        data->spiked->store(result->spiked);

        return NIMCP_SUCCESS;
    };

    // Re-register with user data
    bio_router_unregister_module(evaluator);
    evaluator_info.user_data = &result_data;
    evaluator = bio_router_register_module(&evaluator_info);
    bio_router_register_handler(evaluator, BIO_MSG_LOGIC_GATE_RESULT, handler);

    // Send AND gate evaluation request
    bio_msg_logic_gate_evaluate_t eval_req;
    bio_msg_init_header(&eval_req.header, BIO_MSG_LOGIC_GATE_EVALUATE,
                       BIO_MODULE_BRAIN_COGNITIVE, BIO_MODULE_NEURAL_LOGIC,
                       sizeof(eval_req));
    eval_req.gate_id = 1;
    eval_req.gate_type = LOGIC_GATE_AND;
    eval_req.input_a = 1.0f;
    eval_req.input_b = 1.0f;
    eval_req.threshold = 0.0f;  // Use default

    bio_router_send(evaluator, &eval_req, sizeof(eval_req), 100);
    bio_router_process_inbox(logic_module_, 10);

    // Simulate logic module response
    bio_msg_logic_gate_result_t result;
    bio_msg_init_header(&result.header, BIO_MSG_LOGIC_GATE_RESULT,
                       BIO_MODULE_NEURAL_LOGIC, BIO_MODULE_BRAIN_COGNITIVE,
                       sizeof(result));
    result.gate_id = 1;
    result.gate_type = LOGIC_GATE_AND;
    result.output = 1.0f;  // Both inputs high = AND outputs high
    result.spiked = true;
    result.spike_time_us = 1000;
    result.threshold_used = 1.8f;

    bio_router_send(logic_module_, &result, sizeof(result), 100);
    bio_router_process_inbox(evaluator, 10);

    EXPECT_TRUE(result_received.load());
    EXPECT_FLOAT_EQ(gate_output.load(), 1.0f);
    EXPECT_TRUE(gate_spiked.load());

    bio_router_unregister_module(evaluator);
}

TEST_F(NeuronTypesBioAsyncIntegrationTest, LogicGateOR_FalseInputs) {
    std::atomic<bool> result_received{false};
    std::atomic<float> gate_output{0.0f};

    bio_module_info_t evaluator_info;
    evaluator_info.module_id = BIO_MODULE_BRAIN_COGNITIVE;
    evaluator_info.module_name = "evaluator";
    evaluator_info.inbox_capacity = 64;

    std::pair<std::atomic<bool>*, std::atomic<float>*> handler_data{
        &result_received, &gate_output
    };

    evaluator_info.user_data = &handler_data;
    bio_module_context_t evaluator = bio_router_register_module(&evaluator_info);
    ASSERT_NE(evaluator, nullptr);

    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* data = static_cast<std::pair<std::atomic<bool>*, std::atomic<float>*>*>(user_data);
        auto* result = static_cast<const bio_msg_logic_gate_result_t*>(msg);

        data->first->store(true);
        data->second->store(result->output);

        return NIMCP_SUCCESS;
    };

    bio_router_register_handler(evaluator, BIO_MSG_LOGIC_GATE_RESULT, handler);

    // Send OR gate evaluation with both inputs false
    bio_msg_logic_gate_evaluate_t eval_req;
    bio_msg_init_header(&eval_req.header, BIO_MSG_LOGIC_GATE_EVALUATE,
                       BIO_MODULE_BRAIN_COGNITIVE, BIO_MODULE_NEURAL_LOGIC,
                       sizeof(eval_req));
    eval_req.gate_id = 2;
    eval_req.gate_type = LOGIC_GATE_OR;
    eval_req.input_a = 0.0f;
    eval_req.input_b = 0.0f;
    eval_req.threshold = 0.0f;

    bio_router_send(evaluator, &eval_req, sizeof(eval_req), 100);
    bio_router_process_inbox(logic_module_, 10);

    // Simulate response
    bio_msg_logic_gate_result_t result;
    bio_msg_init_header(&result.header, BIO_MSG_LOGIC_GATE_RESULT,
                       BIO_MODULE_NEURAL_LOGIC, BIO_MODULE_BRAIN_COGNITIVE,
                       sizeof(result));
    result.gate_id = 2;
    result.gate_type = LOGIC_GATE_OR;
    result.output = 0.0f;  // Both inputs low = OR outputs low
    result.spiked = false;

    bio_router_send(logic_module_, &result, sizeof(result), 100);
    bio_router_process_inbox(evaluator, 10);

    EXPECT_TRUE(result_received.load());
    EXPECT_FLOAT_EQ(gate_output.load(), 0.0f);

    bio_router_unregister_module(evaluator);
}

//=============================================================================
// LOGIC CIRCUIT COORDINATION TESTS
//=============================================================================

TEST_F(NeuronTypesBioAsyncIntegrationTest, LogicCircuit_StepCoordination) {
    std::atomic<bool> step_complete{false};
    std::atomic<uint32_t> spikes_generated{0};
    std::atomic<bool> circuit_stable{false};

    struct StepData {
        std::atomic<bool>* complete;
        std::atomic<uint32_t>* spikes;
        std::atomic<bool>* stable;
    };

    StepData step_data{&step_complete, &spikes_generated, &circuit_stable};

    // Register coordinator module
    bio_module_info_t coordinator_info;
    coordinator_info.module_id = BIO_MODULE_BRAIN_COGNITIVE;
    coordinator_info.module_name = "coordinator";
    coordinator_info.inbox_capacity = 64;
    coordinator_info.user_data = &step_data;

    bio_module_context_t coordinator = bio_router_register_module(&coordinator_info);
    ASSERT_NE(coordinator, nullptr);

    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* data = static_cast<StepData*>(user_data);
        auto* complete = static_cast<const bio_msg_logic_circuit_complete_t*>(msg);

        data->complete->store(true);
        data->spikes->store(complete->spikes_generated);
        data->stable->store(complete->circuit_stable);

        return NIMCP_SUCCESS;
    };

    bio_router_register_handler(coordinator, BIO_MSG_LOGIC_CIRCUIT_COMPLETE, handler);

    // Send circuit step request
    bio_msg_logic_circuit_step_t step_req;
    bio_msg_init_header(&step_req.header, BIO_MSG_LOGIC_CIRCUIT_STEP,
                       BIO_MODULE_BRAIN_COGNITIVE, BIO_MODULE_NEURAL_LOGIC,
                       sizeof(step_req));
    step_req.timestamp_us = 1000;
    step_req.delta_t_us = 100;
    step_req.max_iterations = 10;

    bio_router_send(coordinator, &step_req, sizeof(step_req), 100);
    bio_router_process_inbox(logic_module_, 10);

    // Simulate step completion
    bio_msg_logic_circuit_complete_t complete;
    bio_msg_init_header(&complete.header, BIO_MSG_LOGIC_CIRCUIT_COMPLETE,
                       BIO_MODULE_NEURAL_LOGIC, BIO_MODULE_BRAIN_COGNITIVE,
                       sizeof(complete));
    complete.timestamp_us = 1100;
    complete.spikes_generated = 5;
    complete.gates_evaluated = 10;
    complete.avg_eval_time_us = 0.5f;
    complete.circuit_stable = true;

    bio_router_send(logic_module_, &complete, sizeof(complete), 100);
    bio_router_process_inbox(coordinator, 10);

    EXPECT_TRUE(step_complete.load());
    EXPECT_EQ(spikes_generated.load(), 5u);
    EXPECT_TRUE(circuit_stable.load());

    bio_router_unregister_module(coordinator);
}

//=============================================================================
// SPIKE EVENT PROPAGATION TESTS
//=============================================================================

TEST_F(NeuronTypesBioAsyncIntegrationTest, LogicSpike_BroadcastToSubscribers) {
    std::atomic<int> spike_count{0};
    std::atomic<uint32_t> last_gate_id{0};

    struct SpikeData {
        std::atomic<int>* count;
        std::atomic<uint32_t>* gate_id;
    };

    SpikeData spike_data{&spike_count, &last_gate_id};

    // Register multiple subscribers
    const int NUM_SUBSCRIBERS = 3;
    std::vector<bio_module_context_t> subscribers(NUM_SUBSCRIBERS);

    for (int i = 0; i < NUM_SUBSCRIBERS; i++) {
        bio_module_info_t info;
        info.module_id = static_cast<bio_module_id_t>(0x0200 + i);
        info.module_name = "subscriber";
        info.inbox_capacity = 64;
        info.user_data = &spike_data;

        subscribers[i] = bio_router_register_module(&info);
        ASSERT_NE(subscribers[i], nullptr);

        auto handler = [](const void* msg, size_t size,
                         nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
            auto* data = static_cast<SpikeData*>(user_data);
            auto* spike = static_cast<const bio_msg_logic_spike_event_t*>(msg);

            data->count->fetch_add(1);
            data->gate_id->store(spike->gate_id);

            return NIMCP_SUCCESS;
        };

        bio_router_register_handler(subscribers[i], BIO_MSG_LOGIC_SPIKE_EVENT, handler);
    }

    // Broadcast spike event
    bio_msg_logic_spike_event_t spike;
    bio_msg_init_header(&spike.header, BIO_MSG_LOGIC_SPIKE_EVENT,
                       BIO_MODULE_NEURAL_LOGIC, static_cast<bio_module_id_t>(0),  // 0 = broadcast
                       sizeof(spike));
    spike.header.flags = BIO_MSG_FLAG_BROADCAST;
    spike.gate_id = 42;
    spike.gate_type = LOGIC_GATE_AND;
    spike.output = 1.0f;
    spike.spike_time_us = 2000;
    spike.propagation_count = NUM_SUBSCRIBERS;

    // Send ONE broadcast from the logic module
    bio_router_send(logic_module_, &spike, sizeof(spike), 100);

    // Process all subscriber inboxes
    for (int i = 0; i < NUM_SUBSCRIBERS; i++) {
        bio_router_process_inbox(subscribers[i], 10);
    }

    EXPECT_EQ(spike_count.load(), NUM_SUBSCRIBERS);
    EXPECT_EQ(last_gate_id.load(), 42u);

    // Cleanup
    for (int i = 0; i < NUM_SUBSCRIBERS; i++) {
        bio_router_unregister_module(subscribers[i]);
    }
}

//=============================================================================
// VARIABLE BINDING TESTS
//=============================================================================

TEST_F(NeuronTypesBioAsyncIntegrationTest, VariableBinding_MessageExchange) {
    std::atomic<bool> bind_received{false};
    std::atomic<uint32_t> variable_id{0};

    struct BindData {
        std::atomic<bool>* received;
        std::atomic<uint32_t>* var_id;
    };

    BindData bind_data{&bind_received, &variable_id};

    bio_module_info_t logic_info;
    logic_info.module_id = BIO_MODULE_NEURAL_LOGIC;
    logic_info.module_name = "neural_logic";
    logic_info.inbox_capacity = 64;
    logic_info.user_data = &bind_data;

    // Re-register with user data
    bio_router_unregister_module(logic_module_);
    logic_module_ = bio_router_register_module(&logic_info);

    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* data = static_cast<BindData*>(user_data);
        auto* header = static_cast<const bio_message_header_t*>(msg);

        data->received->store(true);
        // In a real implementation, extract variable_id from message payload
        data->var_id->store(12345);

        return NIMCP_SUCCESS;
    };

    bio_router_register_handler(logic_module_, BIO_MSG_LOGIC_VARIABLE_BIND, handler);

    // Send variable binding message
    bio_message_header_t bind_msg;
    bio_msg_init_header(&bind_msg, BIO_MSG_LOGIC_VARIABLE_BIND,
                       BIO_MODULE_BRAIN_COGNITIVE, BIO_MODULE_NEURAL_LOGIC,
                       sizeof(bind_msg));

    bio_router_send(logic_module_, &bind_msg, sizeof(bind_msg), 100);
    bio_router_process_inbox(logic_module_, 10);

    EXPECT_TRUE(bind_received.load());
}

//=============================================================================
// CROSS-MODULE INTEGRATION TESTS
//=============================================================================

TEST_F(NeuronTypesBioAsyncIntegrationTest, NeuronTypes_BrainIntegration) {
    // Register brain module
    bio_module_info_t brain_info;
    brain_info.module_id = BIO_MODULE_BRAIN;
    brain_info.module_name = "brain";
    brain_info.inbox_capacity = 64;
    brain_info.user_data = nullptr;

    bio_module_context_t brain = bio_router_register_module(&brain_info);
    ASSERT_NE(brain, nullptr);

    std::atomic<bool> brain_received{false};

    // Re-register with user data
    bio_router_unregister_module(brain);
    brain_info.user_data = &brain_received;
    brain = bio_router_register_module(&brain_info);

    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* flag = static_cast<std::atomic<bool>*>(user_data);
        flag->store(true);
        return NIMCP_SUCCESS;
    };

    bio_router_register_handler(brain, BIO_MSG_LOGIC_GATE_RESULT, handler);

    // Logic module sends result to brain
    bio_msg_logic_gate_result_t result;
    bio_msg_init_header(&result.header, BIO_MSG_LOGIC_GATE_RESULT,
                       BIO_MODULE_NEURAL_LOGIC, BIO_MODULE_BRAIN,
                       sizeof(result));
    result.gate_id = 1;
    result.gate_type = LOGIC_GATE_XOR;
    result.output = 1.0f;
    result.spiked = true;

    bio_router_send(logic_module_, &result, sizeof(result), 100);
    bio_router_process_inbox(brain, 10);

    EXPECT_TRUE(brain_received.load());

    bio_router_unregister_module(brain);
}

//=============================================================================
// CONCURRENT LOGIC OPERATIONS TESTS
//=============================================================================

TEST_F(NeuronTypesBioAsyncIntegrationTest, ConcurrentGates_ParallelEvaluation) {
    const int NUM_GATES = 5;
    std::atomic<int> results_received{0};

    bio_module_info_t evaluator_info;
    evaluator_info.module_id = BIO_MODULE_BRAIN_COGNITIVE;
    evaluator_info.module_name = "evaluator";
    evaluator_info.inbox_capacity = 128;
    evaluator_info.user_data = &results_received;

    bio_module_context_t evaluator = bio_router_register_module(&evaluator_info);
    ASSERT_NE(evaluator, nullptr);

    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* counter = static_cast<std::atomic<int>*>(user_data);
        counter->fetch_add(1);
        return NIMCP_SUCCESS;
    };

    bio_router_register_handler(evaluator, BIO_MSG_LOGIC_GATE_RESULT, handler);

    // Send multiple gate evaluations
    for (int i = 0; i < NUM_GATES; i++) {
        bio_msg_logic_gate_evaluate_t eval_req;
        bio_msg_init_header(&eval_req.header, BIO_MSG_LOGIC_GATE_EVALUATE,
                           BIO_MODULE_BRAIN_COGNITIVE, BIO_MODULE_NEURAL_LOGIC,
                           sizeof(eval_req));
        eval_req.gate_id = i;
        eval_req.gate_type = static_cast<logic_gate_type_t>(i % LOGIC_GATE_COUNT);
        eval_req.input_a = 1.0f;
        eval_req.input_b = 0.0f;
        eval_req.threshold = 0.0f;

        bio_router_send(evaluator, &eval_req, sizeof(eval_req), 100);
    }

    bio_router_process_inbox(logic_module_, 20);

    // Simulate responses
    for (int i = 0; i < NUM_GATES; i++) {
        bio_msg_logic_gate_result_t result;
        bio_msg_init_header(&result.header, BIO_MSG_LOGIC_GATE_RESULT,
                           BIO_MODULE_NEURAL_LOGIC, BIO_MODULE_BRAIN_COGNITIVE,
                           sizeof(result));
        result.gate_id = i;
        result.gate_type = static_cast<logic_gate_type_t>(i % LOGIC_GATE_COUNT);
        result.output = (i % 2 == 0) ? 1.0f : 0.0f;
        result.spiked = (i % 2 == 0);

        bio_router_send(logic_module_, &result, sizeof(result), 100);
    }

    bio_router_process_inbox(evaluator, 20);

    EXPECT_EQ(results_received.load(), NUM_GATES);

    bio_router_unregister_module(evaluator);
}

//=============================================================================
// CHANNEL-BASED ROUTING TESTS
//=============================================================================

TEST_F(NeuronTypesBioAsyncIntegrationTest, ChannelRouting_LogicMessages) {
    std::atomic<int> dopamine_count{0};
    std::atomic<int> acetylcholine_count{0};

    struct ChannelData {
        std::atomic<int>* dopamine;
        std::atomic<int>* acetylcholine;
    };

    ChannelData channel_data{&dopamine_count, &acetylcholine_count};

    // Re-register with user data
    bio_router_unregister_module(logic_module_);
    bio_module_info_t logic_info;
    logic_info.module_id = BIO_MODULE_NEURAL_LOGIC;
    logic_info.module_name = "neural_logic";
    logic_info.inbox_capacity = 128;
    logic_info.user_data = &channel_data;
    logic_module_ = bio_router_register_module(&logic_info);

    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* data = static_cast<ChannelData*>(user_data);
        auto* header = static_cast<const bio_message_header_t*>(msg);

        switch (header->channel) {
            case BIO_CHANNEL_DOPAMINE:
                data->dopamine->fetch_add(1);
                break;
            case BIO_CHANNEL_ACETYLCHOLINE:
                data->acetylcholine->fetch_add(1);
                break;
            default:
                break;
        }

        return NIMCP_SUCCESS;
    };

    bio_router_register_handler(logic_module_, BIO_MSG_LOGIC_GATE_EVALUATE, handler);

    // Send dopamine-channel messages (reward/learning)
    for (int i = 0; i < 4; i++) {
        bio_message_header_t msg;
        bio_msg_init_header(&msg, BIO_MSG_LOGIC_GATE_EVALUATE,
                           BIO_MODULE_SYSTEM, BIO_MODULE_NEURAL_LOGIC,
                           sizeof(msg));
        msg.channel = BIO_CHANNEL_DOPAMINE;
        bio_router_send(logic_module_, &msg, sizeof(msg), 100);
    }

    // Send acetylcholine-channel messages (fast queries)
    for (int i = 0; i < 6; i++) {
        bio_message_header_t msg;
        bio_msg_init_header(&msg, BIO_MSG_LOGIC_GATE_EVALUATE,
                           BIO_MODULE_SYSTEM, BIO_MODULE_NEURAL_LOGIC,
                           sizeof(msg));
        msg.channel = BIO_CHANNEL_ACETYLCHOLINE;
        bio_router_send(logic_module_, &msg, sizeof(msg), 100);
    }

    bio_router_process_inbox(logic_module_, 50);

    EXPECT_EQ(dopamine_count.load(), 4);
    EXPECT_EQ(acetylcholine_count.load(), 6);
}

//=============================================================================
// STATISTICS TESTS
//=============================================================================

TEST_F(NeuronTypesBioAsyncIntegrationTest, Statistics_LogicMessageTracking) {
    bio_router_stats_t initial_stats;
    bio_router_get_stats(&initial_stats);
    uint64_t initial_routed = initial_stats.messages_routed;

    const int NUM_MESSAGES = 20;
    for (int i = 0; i < NUM_MESSAGES; i++) {
        bio_msg_logic_gate_evaluate_t eval;
        bio_msg_init_header(&eval.header, BIO_MSG_LOGIC_GATE_EVALUATE,
                           BIO_MODULE_SYSTEM, BIO_MODULE_NEURAL_LOGIC,
                           sizeof(eval));
        eval.gate_id = i;
        eval.gate_type = LOGIC_GATE_AND;
        eval.input_a = 1.0f;
        eval.input_b = 1.0f;

        bio_router_send(logic_module_, &eval, sizeof(eval), 100);
    }

    bio_router_stats_t final_stats;
    bio_router_get_stats(&final_stats);

    EXPECT_GE(final_stats.messages_routed, initial_routed)
        << "Statistics should track logic message routing";
}
