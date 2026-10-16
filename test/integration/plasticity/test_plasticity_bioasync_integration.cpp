/**
 * @file test_plasticity_bioasync_integration.cpp
 * @brief Integration tests for plasticity modules with bio-async
 *
 * These tests verify that multiple plasticity modules can work together
 * through the bio-async messaging system.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

// Headers have their own extern "C" guards
#include "plasticity/stdp/nimcp_stdp.h"
#include "plasticity/homeostatic/nimcp_homeostatic.h"
#include "plasticity/attention/nimcp_attention.h"
#include "plasticity/dendritic/nimcp_dendritic.h"
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "plasticity/predictive/nimcp_predictive_coding.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "plasticity/neuromodulators/nimcp_receptor_subtypes.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

class PlasticityBioAsyncIntegrationTest : public ::testing::Test {
protected:
    std::vector<bio_module_context_t> registered_modules;

    void SetUp() override {
        bio_router_config_t config;
        memset(&config, 0, sizeof(config));
        config.max_modules = 128;
        config.inbox_capacity = 64;
        bio_router_init(&config);
    }

    void TearDown() override {
        for (auto ctx : registered_modules) {
            if (ctx) {
                bio_router_unregister_module(ctx);
            }
        }
        registered_modules.clear();
        bio_router_shutdown();
    }

    bio_module_context_t RegisterModule(bio_module_id_t id, const char* name) {
        bio_module_info_t info;
        memset(&info, 0, sizeof(info));
        info.module_id = id;
        info.module_name = name;
        info.inbox_capacity = 64;
        info.user_data = nullptr;
        bio_module_context_t ctx = bio_router_register_module(&info);
        if (ctx) {
            registered_modules.push_back(ctx);
        }
        return ctx;
    }
};

TEST_F(PlasticityBioAsyncIntegrationTest, MultiModuleRegistration) {
    auto attention_ctx = RegisterModule(BIO_MODULE_ATTENTION, "attention");
    auto dendritic_ctx = RegisterModule(BIO_MODULE_DENDRITIC, "dendritic");
    auto adaptive_ctx = RegisterModule(BIO_MODULE_ADAPTIVE, "adaptive");
    auto predictive_ctx = RegisterModule(BIO_MODULE_PREDICTIVE_CODING, "predictive");

    ASSERT_NE(attention_ctx, nullptr);
    ASSERT_NE(dendritic_ctx, nullptr);
    ASSERT_NE(adaptive_ctx, nullptr);
    ASSERT_NE(predictive_ctx, nullptr);

    EXPECT_TRUE(bio_router_is_initialized());
}

TEST_F(PlasticityBioAsyncIntegrationTest, STDPToNeuromodulatorFlow) {
    auto stdp_ctx = RegisterModule(BIO_MODULE_STDP, "stdp");
    auto neuromod_ctx = RegisterModule(BIO_MODULE_NEUROMODULATOR, "neuromodulator");

    ASSERT_NE(stdp_ctx, nullptr);
    ASSERT_NE(neuromod_ctx, nullptr);

    bio_msg_salience_response_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header,
                        BIO_MSG_SALIENCE_RESPONSE,
                        bio_module_context_get_id(stdp_ctx),
                        bio_module_context_get_id(neuromod_ctx),
                        sizeof(msg));
    msg.salience_score = 0.7f;

    nimcp_error_t result = bio_router_send(stdp_ctx, &msg, sizeof(msg), 0);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(PlasticityBioAsyncIntegrationTest, PredictiveErrorToDopamine) {
    auto predictive_ctx = RegisterModule(BIO_MODULE_PREDICTIVE_CODING, "predictive");
    auto neuromod_ctx = RegisterModule(BIO_MODULE_NEUROMODULATOR, "neuromodulator");

    ASSERT_NE(predictive_ctx, nullptr);
    ASSERT_NE(neuromod_ctx, nullptr);

    bio_msg_salience_response_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header,
                        BIO_MSG_SALIENCE_RESPONSE,
                        bio_module_context_get_id(predictive_ctx),
                        BIO_MODULE_UNKNOWN,
                        sizeof(msg));
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;
    msg.salience_score = 0.9f;

    nimcp_error_t result = bio_router_broadcast(predictive_ctx, &msg, sizeof(msg));
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(PlasticityBioAsyncIntegrationTest, DendriticSpikeToSTDP) {
    auto dendritic_ctx = RegisterModule(BIO_MODULE_DENDRITIC, "dendritic");
    auto stdp_ctx = RegisterModule(BIO_MODULE_STDP, "stdp");

    ASSERT_NE(dendritic_ctx, nullptr);
    ASSERT_NE(stdp_ctx, nullptr);

    bio_msg_salience_response_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header,
                        BIO_MSG_SALIENCE_RESPONSE,
                        bio_module_context_get_id(dendritic_ctx),
                        BIO_MODULE_UNKNOWN,
                        sizeof(msg));
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;
    msg.salience_score = 0.85f;

    nimcp_error_t result = bio_router_broadcast(dendritic_ctx, &msg, sizeof(msg));
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(PlasticityBioAsyncIntegrationTest, AttentionModulatesLearning) {
    auto attention_ctx = RegisterModule(BIO_MODULE_ATTENTION, "attention");
    auto adaptive_ctx = RegisterModule(BIO_MODULE_ADAPTIVE, "adaptive");
    auto neuromod_ctx = RegisterModule(BIO_MODULE_NEUROMODULATOR, "neuromodulator");

    ASSERT_NE(attention_ctx, nullptr);
    ASSERT_NE(adaptive_ctx, nullptr);
    ASSERT_NE(neuromod_ctx, nullptr);

    bio_msg_attention_shift_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header,
                        BIO_MSG_ATTENTION_SHIFT,
                        bio_module_context_get_id(attention_ctx),
                        BIO_MODULE_UNKNOWN,
                        sizeof(msg));
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;
    msg.attention_weight = 0.8f;

    nimcp_error_t result = bio_router_broadcast(attention_ctx, &msg, sizeof(msg));
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(PlasticityBioAsyncIntegrationTest, MultiChannelBroadcast) {
    auto predictive_ctx = RegisterModule(BIO_MODULE_PREDICTIVE_CODING, "predictive");
    auto dendritic_ctx = RegisterModule(BIO_MODULE_DENDRITIC, "dendritic");
    auto attention_ctx = RegisterModule(BIO_MODULE_ATTENTION, "attention");

    ASSERT_NE(predictive_ctx, nullptr);
    ASSERT_NE(dendritic_ctx, nullptr);
    ASSERT_NE(attention_ctx, nullptr);

    bio_msg_salience_response_t msg1;
    memset(&msg1, 0, sizeof(msg1));
    bio_msg_init_header(&msg1.header,
                        BIO_MSG_SALIENCE_RESPONSE,
                        bio_module_context_get_id(predictive_ctx),
                        BIO_MODULE_UNKNOWN,
                        sizeof(msg1));
    msg1.header.flags |= BIO_MSG_FLAG_BROADCAST;
    msg1.salience_score = 0.7f;

    bio_msg_salience_response_t msg2;
    memset(&msg2, 0, sizeof(msg2));
    bio_msg_init_header(&msg2.header,
                        BIO_MSG_SALIENCE_RESPONSE,
                        bio_module_context_get_id(dendritic_ctx),
                        BIO_MODULE_UNKNOWN,
                        sizeof(msg2));
    msg2.header.flags |= BIO_MSG_FLAG_BROADCAST;
    msg2.salience_score = 0.8f;

    bio_msg_attention_shift_t msg3;
    memset(&msg3, 0, sizeof(msg3));
    bio_msg_init_header(&msg3.header,
                        BIO_MSG_ATTENTION_SHIFT,
                        bio_module_context_get_id(attention_ctx),
                        BIO_MODULE_UNKNOWN,
                        sizeof(msg3));
    msg3.header.flags |= BIO_MSG_FLAG_BROADCAST;
    msg3.attention_weight = 0.6f;

    EXPECT_EQ(bio_router_broadcast(predictive_ctx, &msg1, sizeof(msg1)), NIMCP_SUCCESS);
    EXPECT_EQ(bio_router_broadcast(dendritic_ctx, &msg2, sizeof(msg2)), NIMCP_SUCCESS);
    EXPECT_EQ(bio_router_broadcast(attention_ctx, &msg3, sizeof(msg3)), NIMCP_SUCCESS);
}

TEST_F(PlasticityBioAsyncIntegrationTest, ReceptorSubtypeModulation) {
    auto receptor_ctx = RegisterModule(BIO_MODULE_NEUROMODULATOR_RECEPTOR, "receptor");
    auto neuromod_ctx = RegisterModule(BIO_MODULE_NEUROMODULATOR, "neuromodulator");

    ASSERT_NE(receptor_ctx, nullptr);
    ASSERT_NE(neuromod_ctx, nullptr);

    bio_msg_salience_response_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header,
                        BIO_MSG_SALIENCE_RESPONSE,
                        bio_module_context_get_id(neuromod_ctx),
                        bio_module_context_get_id(receptor_ctx),
                        sizeof(msg));
    msg.salience_score = 0.75f;

    nimcp_error_t result = bio_router_send(neuromod_ctx, &msg, sizeof(msg), 0);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
