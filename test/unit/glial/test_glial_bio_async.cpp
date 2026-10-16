/**
 * @file test_glial_bio_async.cpp
 * @brief Unit tests for glial module bio-async integration
 *
 * Tests glial signaling via bio-async:
 * - Astrocyte calcium wave initiation and propagation
 * - Microglia alert handling and immune responses
 * - Oligodendrocyte myelination requests
 * - Metabolic coordination signals
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 */

#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"

//=============================================================================
// Test Fixture
//=============================================================================

class GlialBioAsyncTest : public ::testing::Test {
protected:
    bio_module_context_t astrocyte_module;
    bio_module_context_t microglia_module;
    bio_module_context_t oligodendrocyte_module;
    bio_module_context_t brain_module;

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

        // Register modules
        bio_module_info_t astro_info = {
            .module_id = BIO_MODULE_ASTROCYTE,
            .module_name = "astrocyte",
            .inbox_capacity = 100,
            .user_data = nullptr
        };
        astrocyte_module = bio_router_register_module(&astro_info);
        ASSERT_NE(astrocyte_module, nullptr);

        bio_module_info_t micro_info = {
            .module_id = BIO_MODULE_MICROGLIA,
            .module_name = "microglia",
            .inbox_capacity = 100,
            .user_data = nullptr
        };
        microglia_module = bio_router_register_module(&micro_info);
        ASSERT_NE(microglia_module, nullptr);

        bio_module_info_t oligo_info = {
            .module_id = BIO_MODULE_OLIGODENDROCYTE,
            .module_name = "oligodendrocyte",
            .inbox_capacity = 100,
            .user_data = nullptr
        };
        oligodendrocyte_module = bio_router_register_module(&oligo_info);
        ASSERT_NE(oligodendrocyte_module, nullptr);

        bio_module_info_t brain_info = {
            .module_id = BIO_MODULE_BRAIN,
            .module_name = "brain",
            .inbox_capacity = 100,
            .user_data = nullptr
        };
        brain_module = bio_router_register_module(&brain_info);
        ASSERT_NE(brain_module, nullptr);
    }

    void TearDown() override {
        bio_router_unregister_module(astrocyte_module);
        bio_router_unregister_module(microglia_module);
        bio_router_unregister_module(oligodendrocyte_module);
        bio_router_unregister_module(brain_module);
        bio_router_shutdown();
        nimcp_bio_async_shutdown();
    }
};

//=============================================================================
// ASTROCYTE CALCIUM WAVE TESTS
//=============================================================================

TEST_F(GlialBioAsyncTest, CalciumWaveInitiation) {
    std::atomic<bool> wave_received{false};
    std::atomic<float> calcium_level{0.0f};

    // Register astrocyte handler for calcium waves
    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* data = static_cast<std::pair<std::atomic<bool>*, std::atomic<float>*>*>(user_data);
        data->first->store(true);

        auto* wave_msg = static_cast<const bio_msg_astrocyte_wave_t*>(msg);
        EXPECT_EQ(wave_msg->header.type, BIO_MSG_ASTROCYTE_CALCIUM_WAVE);

        // Verify wave parameters
        EXPECT_GT(wave_msg->initial_calcium_um, 0.0f);
        EXPECT_GT(wave_msg->propagation_speed_um_s, 0.0f);

        data->second->store(wave_msg->initial_calcium_um);

        return NIMCP_SUCCESS;
    };

    std::pair<std::atomic<bool>*, std::atomic<float>*> handler_data{
        &wave_received, &calcium_level
    };

    // Re-register with user data
    bio_router_unregister_module(astrocyte_module);
    bio_module_info_t astro_info = {
        .module_id = BIO_MODULE_ASTROCYTE,
        .module_name = "astrocyte",
        .inbox_capacity = 100,
        .user_data = &handler_data
    };
    astrocyte_module = bio_router_register_module(&astro_info);

    bio_router_register_handler(astrocyte_module, BIO_MSG_ASTROCYTE_CALCIUM_WAVE, handler);

    // Initiate calcium wave
    bio_msg_astrocyte_wave_t wave;
    bio_msg_init_header(&wave.header, BIO_MSG_ASTROCYTE_CALCIUM_WAVE,
                       BIO_MODULE_BRAIN, BIO_MODULE_ASTROCYTE,
                       sizeof(wave));
    wave.source_region = 0;
    wave.initial_calcium_um = 2.0f;  // Peak calcium
    wave.propagation_speed_um_s = BIO_COMP_CA_WAVE_SPEED;
    wave.mode = BIO_WAVE_ISOTROPIC;

    nimcp_error_t err = bio_router_send(brain_module, &wave, sizeof(wave), 1000);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Process inbox
    bio_router_process_inbox(astrocyte_module, 10);

    EXPECT_TRUE(wave_received.load());
    EXPECT_FLOAT_EQ(calcium_level.load(), 2.0f);
}

TEST_F(GlialBioAsyncTest, CalciumWavePropagation) {
    // Test wave reaching multiple regions
    std::atomic<int> regions_reached{0};

    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* count = static_cast<std::atomic<int>*>(user_data);
        (*count)++;
        return NIMCP_SUCCESS;
    };

    bio_router_unregister_module(astrocyte_module);
    bio_module_info_t astro_info = {
        .module_id = BIO_MODULE_ASTROCYTE,
        .module_name = "astrocyte",
        .inbox_capacity = 100,
        .user_data = &regions_reached
    };
    astrocyte_module = bio_router_register_module(&astro_info);

    bio_router_register_handler(astrocyte_module, BIO_MSG_ASTROCYTE_CALCIUM_WAVE, handler);

    // Send multiple wave messages simulating propagation
    for (int i = 0; i < 5; i++) {
        bio_msg_astrocyte_wave_t wave;
        bio_msg_init_header(&wave.header, BIO_MSG_ASTROCYTE_CALCIUM_WAVE,
                           BIO_MODULE_BRAIN, BIO_MODULE_ASTROCYTE,
                           sizeof(wave));
        wave.source_region = i;
        wave.initial_calcium_um = 1.5f;
        wave.propagation_speed_um_s = BIO_COMP_CA_WAVE_SPEED;
        wave.mode = BIO_WAVE_NETWORK;

        bio_router_send(brain_module, &wave, sizeof(wave), 1000);
    }

    // Process all waves
    bio_router_process_inbox(astrocyte_module, 10);

    EXPECT_EQ(regions_reached.load(), 5);
}

TEST_F(GlialBioAsyncTest, GlutamateUptake) {
    std::atomic<bool> uptake_triggered{false};

    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* triggered = static_cast<std::atomic<bool>*>(user_data);
        *triggered = true;

        // In real implementation, this would trigger glutamate transporter activity
        return NIMCP_SUCCESS;
    };

    bio_router_unregister_module(astrocyte_module);
    bio_module_info_t astro_info = {
        .module_id = BIO_MODULE_ASTROCYTE,
        .module_name = "astrocyte",
        .inbox_capacity = 100,
        .user_data = &uptake_triggered
    };
    astrocyte_module = bio_router_register_module(&astro_info);

    bio_router_register_handler(astrocyte_module, BIO_MSG_ASTROCYTE_GLUTAMATE_UPTAKE, handler);

    // Send glutamate uptake request
    bio_message_header_t uptake_msg;
    bio_msg_init_header(&uptake_msg, BIO_MSG_ASTROCYTE_GLUTAMATE_UPTAKE,
                       BIO_MODULE_BRAIN, BIO_MODULE_ASTROCYTE,
                       sizeof(uptake_msg));

    bio_router_send(brain_module, &uptake_msg, sizeof(uptake_msg), 1000);
    bio_router_process_inbox(astrocyte_module, 10);

    EXPECT_TRUE(uptake_triggered.load());
}

//=============================================================================
// MICROGLIA ALERT TESTS
//=============================================================================

TEST_F(GlialBioAsyncTest, MicrogliaAlertHandling) {
    std::atomic<bool> alert_received{false};
    std::atomic<uint32_t> alert_type{0};
    std::atomic<float> severity{0.0f};

    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* data = static_cast<std::tuple<std::atomic<bool>*, std::atomic<uint32_t>*, std::atomic<float>*>*>(user_data);

        std::get<0>(*data)->store(true);

        auto* alert = static_cast<const bio_msg_microglia_alert_t*>(msg);
        EXPECT_EQ(alert->header.type, BIO_MSG_MICROGLIA_ALERT);

        std::get<1>(*data)->store(alert->alert_type);
        std::get<2>(*data)->store(alert->severity);

        // In real implementation, would trigger immune response
        if (alert->alert_type == BIO_MICROGLIA_ALERT_DAMAGE) {
            // Handle damage
        } else if (alert->alert_type == BIO_MICROGLIA_ALERT_PRUNE_NEEDED) {
            // Trigger pruning
        }

        return NIMCP_SUCCESS;
    };

    std::tuple<std::atomic<bool>*, std::atomic<uint32_t>*, std::atomic<float>*> handler_data{
        &alert_received, &alert_type, &severity
    };

    bio_router_unregister_module(microglia_module);
    bio_module_info_t micro_info = {
        .module_id = BIO_MODULE_MICROGLIA,
        .module_name = "microglia",
        .inbox_capacity = 100,
        .user_data = &handler_data
    };
    microglia_module = bio_router_register_module(&micro_info);

    bio_router_register_handler(microglia_module, BIO_MSG_MICROGLIA_ALERT, handler);

    // Send alert for synaptic pruning
    bio_msg_microglia_alert_t alert;
    bio_msg_init_header(&alert.header, BIO_MSG_MICROGLIA_ALERT,
                       BIO_MODULE_BRAIN, BIO_MODULE_MICROGLIA,
                       sizeof(alert));
    alert.alert_region = 1;
    alert.alert_type = BIO_MICROGLIA_ALERT_PRUNE_NEEDED;
    alert.severity = 0.7f;
    alert.affected_synapse_count = 150;

    bio_router_send(brain_module, &alert, sizeof(alert), 1000);
    bio_router_process_inbox(microglia_module, 10);

    EXPECT_TRUE(alert_received.load());
    EXPECT_EQ(alert_type.load(), BIO_MICROGLIA_ALERT_PRUNE_NEEDED);
    EXPECT_FLOAT_EQ(severity.load(), 0.7f);
}

TEST_F(GlialBioAsyncTest, MicrogliaDamageResponse) {
    std::atomic<int> damage_alerts{0};

    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* count = static_cast<std::atomic<int>*>(user_data);
        (*count)++;
        return NIMCP_SUCCESS;
    };

    bio_router_unregister_module(microglia_module);
    bio_module_info_t micro_info = {
        .module_id = BIO_MODULE_MICROGLIA,
        .module_name = "microglia",
        .inbox_capacity = 100,
        .user_data = &damage_alerts
    };
    microglia_module = bio_router_register_module(&micro_info);

    bio_router_register_handler(microglia_module, BIO_MSG_MICROGLIA_ALERT, handler);

    // Send damage alert
    bio_msg_microglia_alert_t alert;
    bio_msg_init_header(&alert.header, BIO_MSG_MICROGLIA_ALERT,
                       BIO_MODULE_BRAIN, BIO_MODULE_MICROGLIA,
                       sizeof(alert));
    alert.alert_type = BIO_MICROGLIA_ALERT_DAMAGE;
    alert.severity = 0.9f;  // High severity

    bio_router_send(brain_module, &alert, sizeof(alert), 1000);
    bio_router_process_inbox(microglia_module, 10);

    EXPECT_EQ(damage_alerts.load(), 1);
}

//=============================================================================
// OLIGODENDROCYTE MYELINATION TESTS
//=============================================================================

TEST_F(GlialBioAsyncTest, MyelinationRequest) {
    std::atomic<bool> myelination_triggered{false};
    std::atomic<uint32_t> axon_id{0};
    std::atomic<float> target_thickness{0.0f};

    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* data = static_cast<std::tuple<std::atomic<bool>*, std::atomic<uint32_t>*, std::atomic<float>*>*>(user_data);

        std::get<0>(*data)->store(true);

        auto* myelinate = static_cast<const bio_msg_oligodendrocyte_myelinate_t*>(msg);
        EXPECT_EQ(myelinate->header.type, BIO_MSG_OLIGODENDROCYTE_MYELINATE);

        std::get<1>(*data)->store(myelinate->axon_id);
        std::get<2>(*data)->store(myelinate->target_thickness);

        // Verify parameters
        EXPECT_GT(myelinate->target_thickness, 0.0f);
        EXPECT_GE(myelinate->priority, 0.0f);
        EXPECT_LE(myelinate->priority, 1.0f);

        return NIMCP_SUCCESS;
    };

    std::tuple<std::atomic<bool>*, std::atomic<uint32_t>*, std::atomic<float>*> handler_data{
        &myelination_triggered, &axon_id, &target_thickness
    };

    bio_router_unregister_module(oligodendrocyte_module);
    bio_module_info_t oligo_info = {
        .module_id = BIO_MODULE_OLIGODENDROCYTE,
        .module_name = "oligodendrocyte",
        .inbox_capacity = 100,
        .user_data = &handler_data
    };
    oligodendrocyte_module = bio_router_register_module(&oligo_info);

    bio_router_register_handler(oligodendrocyte_module, BIO_MSG_OLIGODENDROCYTE_MYELINATE, handler);

    // Send myelination request
    bio_msg_oligodendrocyte_myelinate_t request;
    bio_msg_init_header(&request.header, BIO_MSG_OLIGODENDROCYTE_MYELINATE,
                       BIO_MODULE_BRAIN, BIO_MODULE_OLIGODENDROCYTE,
                       sizeof(request));
    request.axon_id = 42;
    request.target_thickness = 0.5f;  // μm
    request.priority = 0.8f;
    request.segment_start = 0;
    request.segment_length = 1000;

    bio_router_send(brain_module, &request, sizeof(request), 1000);
    bio_router_process_inbox(oligodendrocyte_module, 10);

    EXPECT_TRUE(myelination_triggered.load());
    EXPECT_EQ(axon_id.load(), 42u);
    EXPECT_FLOAT_EQ(target_thickness.load(), 0.5f);
}

//=============================================================================
// METABOLIC COORDINATION TESTS
//=============================================================================

TEST_F(GlialBioAsyncTest, MetabolicDemandSignal) {
    std::atomic<bool> demand_received{false};
    std::atomic<float> glucose_demand{0.0f};

    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* data = static_cast<std::pair<std::atomic<bool>*, std::atomic<float>*>*>(user_data);
        data->first->store(true);

        auto* demand = static_cast<const bio_msg_metabolic_demand_t*>(msg);
        data->second->store(demand->glucose_demand);

        // In real implementation, astrocytes would respond with supply
        return NIMCP_SUCCESS;
    };

    std::pair<std::atomic<bool>*, std::atomic<float>*> handler_data{
        &demand_received, &glucose_demand
    };

    bio_router_unregister_module(astrocyte_module);
    bio_module_info_t astro_info = {
        .module_id = BIO_MODULE_ASTROCYTE,
        .module_name = "astrocyte",
        .inbox_capacity = 100,
        .user_data = &handler_data
    };
    astrocyte_module = bio_router_register_module(&astro_info);

    bio_router_register_handler(astrocyte_module, BIO_MSG_METABOLIC_DEMAND, handler);

    // Send metabolic demand
    bio_msg_metabolic_demand_t demand;
    bio_msg_init_header(&demand.header, BIO_MSG_METABOLIC_DEMAND,
                       BIO_MODULE_BRAIN, BIO_MODULE_ASTROCYTE,
                       sizeof(demand));
    demand.region_id = 0;
    demand.glucose_demand = 5.0f;  // μmol
    demand.oxygen_demand = 2.0f;
    demand.atp_deficit = 1.5f;
    demand.urgency = 0.7f;

    bio_router_send(brain_module, &demand, sizeof(demand), 1000);
    bio_router_process_inbox(astrocyte_module, 10);

    EXPECT_TRUE(demand_received.load());
    EXPECT_FLOAT_EQ(glucose_demand.load(), 5.0f);
}

//=============================================================================
// GLIAL INTEGRATION TESTS
//=============================================================================

TEST_F(GlialBioAsyncTest, GlialSyncRequest) {
    std::atomic<int> sync_count{0};

    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* count = static_cast<std::atomic<int>*>(user_data);
        (*count)++;
        return NIMCP_SUCCESS;
    };

    // Register handler on all glial modules
    bio_router_unregister_module(astrocyte_module);
    bio_router_unregister_module(microglia_module);
    bio_router_unregister_module(oligodendrocyte_module);

    bio_module_info_t astro_info = {
        .module_id = BIO_MODULE_ASTROCYTE,
        .module_name = "astrocyte",
        .inbox_capacity = 100,
        .user_data = &sync_count
    };
    astrocyte_module = bio_router_register_module(&astro_info);
    bio_router_register_handler(astrocyte_module, BIO_MSG_GLIAL_SYNC_REQUEST, handler);

    bio_module_info_t micro_info = {
        .module_id = BIO_MODULE_MICROGLIA,
        .module_name = "microglia",
        .inbox_capacity = 100,
        .user_data = &sync_count
    };
    microglia_module = bio_router_register_module(&micro_info);
    bio_router_register_handler(microglia_module, BIO_MSG_GLIAL_SYNC_REQUEST, handler);

    bio_module_info_t oligo_info = {
        .module_id = BIO_MODULE_OLIGODENDROCYTE,
        .module_name = "oligodendrocyte",
        .inbox_capacity = 100,
        .user_data = &sync_count
    };
    oligodendrocyte_module = bio_router_register_module(&oligo_info);
    bio_router_register_handler(oligodendrocyte_module, BIO_MSG_GLIAL_SYNC_REQUEST, handler);

    // Broadcast sync request to all glial cells
    bio_message_header_t sync_msg;
    bio_msg_init_header(&sync_msg, BIO_MSG_GLIAL_SYNC_REQUEST,
                       BIO_MODULE_BRAIN, (bio_module_id_t)0,  // Broadcast
                       sizeof(sync_msg));
    sync_msg.flags |= BIO_MSG_FLAG_BROADCAST;

    bio_router_broadcast(brain_module, &sync_msg, sizeof(sync_msg));

    // Process all inboxes
    bio_router_process_inbox(astrocyte_module, 10);
    bio_router_process_inbox(microglia_module, 10);
    bio_router_process_inbox(oligodendrocyte_module, 10);

    EXPECT_EQ(sync_count.load(), 3);  // All three glial types
}
