/**
 * @file test_perirhinal_bio_async_integration.cpp
 * @brief Integration tests for Perirhinal Cortex with Bio-Async messaging system
 *
 * WHAT: Tests Perirhinal Cortex integration with bio-async neuromodulator channels
 * WHY:  Ensure proper object recognition communication via biological messaging
 * HOW:  Test router initialization, module registration, and perirhinal operations
 *
 * BIOLOGICAL BASIS:
 * Perirhinal cortex communicates via neuromodulator channels:
 * - DOPAMINE: Reward signals for recognized/familiar objects
 * - NOREPINEPHRINE: Alerting for novel/salient objects
 * - ACETYLCHOLINE: Attention modulation for object encoding
 * - SEROTONIN: Recognition confidence modulation
 *
 * INTEGRATION POINTS:
 * - Bio-async router initialization
 * - Module registration
 * - Perirhinal operations with bio-async enabled
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Include system headers first, then NIMCP headers, then region-specific
// This ensures typedef compatibility
#include "nimcp.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "utils/logging/nimcp_logging.h"
#include "core/brain/regions/perirhinal/nimcp_perirhinal.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class PerirhinalBioAsyncIntegrationTest : public ::testing::Test {
protected:
    nimcp_perirhinal_t* perirhinal;
    perirhinal_config_t config;
    bool router_initialized;

    void SetUp() override {
        router_initialized = false;

        /* Initialize bio-async router using global singleton pattern */
        bio_router_config_t router_config = bio_router_default_config();
        router_config.max_modules = 64;
        router_config.inbox_capacity = 256;
        router_config.outbox_capacity = 256;
        router_config.enable_logging = false;

        if (bio_router_init(&router_config) == NIMCP_OK) {
            router_initialized = true;
        }

        /* Configure perirhinal adapter */
        config = perirhinal_default_config();
        config.enable_bio_async = router_initialized;
        config.enable_snn = true;
        config.enable_plasticity = true;
        config.enable_cognitive = true;

        perirhinal = perirhinal_create(&config);
        ASSERT_NE(nullptr, perirhinal) << "Failed to create Perirhinal cortex";
    }

    void TearDown() override {
        if (perirhinal) {
            perirhinal_destroy(perirhinal);
            perirhinal = nullptr;
        }
        if (router_initialized) {
            bio_router_shutdown();
            router_initialized = false;
        }
    }

    /* Helper to create test visual features */
    void CreateTestFeatures(float* features, uint32_t dim, float base_value) {
        for (uint32_t i = 0; i < dim; i++) {
            features[i] = base_value + (float)i * 0.001f;
        }
    }
};

/*=============================================================================
 * BIO-ROUTER LIFECYCLE TESTS
 *===========================================================================*/

TEST_F(PerirhinalBioAsyncIntegrationTest, RouterInitialization) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not available";
    }
    EXPECT_TRUE(bio_router_is_initialized());
}

TEST_F(PerirhinalBioAsyncIntegrationTest, RouterGetGlobal) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not available";
    }

    bio_router_t global_router = bio_router_get_global();
    EXPECT_NE(nullptr, global_router);
}

TEST_F(PerirhinalBioAsyncIntegrationTest, DefaultConfig) {
    bio_router_config_t default_cfg = bio_router_default_config();
    EXPECT_GT(default_cfg.max_modules, 0u);
    EXPECT_GT(default_cfg.inbox_capacity, 0u);
}

/*=============================================================================
 * MODULE REGISTRATION TESTS
 *===========================================================================*/

TEST_F(PerirhinalBioAsyncIntegrationTest, RegisterPerirhinalModule) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not available";
    }

    bio_module_info_t info;
    memset(&info, 0, sizeof(info));
    info.module_id = BIO_MODULE_TEMPORAL_CORTEX;  /* Perirhinal is part of temporal cortex */
    info.module_name = "perirhinal_cortex";
    info.inbox_capacity = 128;
    info.user_data = perirhinal;

    bio_module_context_t ctx = bio_router_register_module(&info);
    EXPECT_NE(nullptr, ctx);

    if (ctx) {
        bio_router_unregister_module(ctx);
    }
}

TEST_F(PerirhinalBioAsyncIntegrationTest, RegisterMultiplePerirhinalComponents) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not available";
    }

    /* Register main perirhinal module */
    bio_module_info_t pr_info;
    memset(&pr_info, 0, sizeof(pr_info));
    pr_info.module_id = BIO_MODULE_TEMPORAL_CORTEX;  /* Perirhinal is part of temporal cortex */
    pr_info.module_name = "perirhinal";
    pr_info.user_data = perirhinal;

    bio_module_context_t pr_ctx = bio_router_register_module(&pr_info);
    EXPECT_NE(nullptr, pr_ctx);

    /* Register associated module (e.g., familiarity processor) */
    bio_module_info_t fam_info;
    memset(&fam_info, 0, sizeof(fam_info));
    fam_info.module_id = BIO_MODULE_HIPPOCAMPUS;  /* Use available ID */
    fam_info.module_name = "familiarity_proc";
    fam_info.user_data = perirhinal;

    bio_module_context_t fam_ctx = bio_router_register_module(&fam_info);
    EXPECT_NE(nullptr, fam_ctx);

    if (pr_ctx) bio_router_unregister_module(pr_ctx);
    if (fam_ctx) bio_router_unregister_module(fam_ctx);
}

TEST_F(PerirhinalBioAsyncIntegrationTest, ModuleContextAccessors) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not available";
    }

    bio_module_info_t info;
    memset(&info, 0, sizeof(info));
    info.module_id = BIO_MODULE_TEMPORAL_CORTEX;  /* Perirhinal is part of temporal cortex */
    info.module_name = "perirhinal_test";
    info.user_data = perirhinal;

    bio_module_context_t ctx = bio_router_register_module(&info);
    ASSERT_NE(nullptr, ctx);

    /* Verify context accessors */
    EXPECT_EQ(BIO_MODULE_TEMPORAL_CORTEX, bio_module_context_get_id(ctx));
    const char* name = bio_module_context_get_name(ctx);
    EXPECT_NE(nullptr, name);
    EXPECT_GT(strlen(name), 0u);
    EXPECT_EQ(perirhinal, bio_module_context_get_user_data(ctx));

    bio_router_unregister_module(ctx);
}

/*=============================================================================
 * ROUTER STATISTICS TESTS
 *===========================================================================*/

TEST_F(PerirhinalBioAsyncIntegrationTest, RouterStatistics) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not available";
    }

    bio_router_stats_t stats;
    nimcp_error_t result = bio_router_get_stats(&stats);
    EXPECT_EQ(NIMCP_OK, result);

    EXPECT_GE(stats.messages_routed, 0u);
    EXPECT_GE(stats.active_modules, 0u);
}

TEST_F(PerirhinalBioAsyncIntegrationTest, RouterStatisticsAfterActivity) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not available";
    }

    bio_module_info_t info;
    memset(&info, 0, sizeof(info));
    info.module_id = BIO_MODULE_TEMPORAL_CORTEX;  /* Perirhinal is part of temporal cortex */
    info.module_name = "perirhinal";
    info.user_data = perirhinal;

    bio_module_context_t ctx = bio_router_register_module(&info);
    ASSERT_NE(nullptr, ctx);

    bio_router_stats_t stats;
    bio_router_get_stats(&stats);
    EXPECT_GE(stats.active_modules, 1u);

    bio_router_unregister_module(ctx);
}

/*=============================================================================
 * PERIRHINAL OPERATIONS WITH BIO-ASYNC TESTS
 *===========================================================================*/

TEST_F(PerirhinalBioAsyncIntegrationTest, PerirhinalBioAsyncEnabled) {
    perirhinal_config_t retrieved;
    EXPECT_EQ(0, perirhinal_get_config(perirhinal, &retrieved));
    EXPECT_EQ(router_initialized, retrieved.enable_bio_async);
}

TEST_F(PerirhinalBioAsyncIntegrationTest, ObjectEncodingWithBioAsync) {
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    int result = perirhinal_encode_object(perirhinal, features, 256, "test_object", &object_id);
    EXPECT_EQ(0, result);
    EXPECT_GE(object_id, 0u);
}

TEST_F(PerirhinalBioAsyncIntegrationTest, ObjectRecognitionWithBioAsync) {
    /* First encode an object */
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    ASSERT_EQ(0, perirhinal_encode_object(perirhinal, features, 256, "known_object", &object_id));

    /* Try to recognize it */
    perirhinal_recognition_result_t result;
    memset(&result, 0, sizeof(result));
    int recognition_result = perirhinal_recognize_object(perirhinal, features, 256, &result);
    EXPECT_EQ(0, recognition_result);
    EXPECT_GT(result.match_confidence, 0.0f);
}

TEST_F(PerirhinalBioAsyncIntegrationTest, FamiliarityComputationWithBioAsync) {
    /* Encode object to make it familiar */
    float features[256];
    CreateTestFeatures(features, 256, 0.3f);

    uint32_t object_id = 0;
    ASSERT_EQ(0, perirhinal_encode_object(perirhinal, features, 256, "familiar_obj", &object_id));

    /* Compute familiarity */
    float familiarity = perirhinal_compute_familiarity(perirhinal, features, 256);
    EXPECT_GE(familiarity, 0.0f);
    EXPECT_LE(familiarity, 1.0f);
}

TEST_F(PerirhinalBioAsyncIntegrationTest, NoveltyDetectionWithBioAsync) {
    /* Novel features (never seen before) */
    float novel_features[256];
    CreateTestFeatures(novel_features, 256, 0.9f);

    float novelty = perirhinal_compute_novelty(perirhinal, novel_features, 256);
    EXPECT_GE(novelty, 0.0f);

    bool is_novel = perirhinal_is_novel(perirhinal, novel_features, 256);
    /* Novel input should be detected as novel */
    EXPECT_TRUE(is_novel);
}

TEST_F(PerirhinalBioAsyncIntegrationTest, MultipleEncodingsWithBioAsync) {
    /* Encode several objects */
    for (int i = 0; i < 10; i++) {
        float features[256];
        CreateTestFeatures(features, 256, (float)i * 0.1f);

        char name[32];
        snprintf(name, sizeof(name), "object_%d", i);

        uint32_t object_id = 0;
        int result = perirhinal_encode_object(perirhinal, features, 256, name, &object_id);
        EXPECT_EQ(0, result);
        EXPECT_GE(object_id, 0u);
    }

    perirhinal_stats_t stats;
    EXPECT_EQ(0, perirhinal_get_stats(perirhinal, &stats));
    EXPECT_GE(stats.objects_encoded, 10u);
}

/*=============================================================================
 * BIO-ASYNC NEUROMODULATOR CHANNEL TESTS
 *===========================================================================*/

TEST_F(PerirhinalBioAsyncIntegrationTest, PromiseCreation) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(
        BIO_CHANNEL_DOPAMINE, sizeof(float));

    if (promise) {
        nimcp_bio_promise_destroy(promise);
    }
}

TEST_F(PerirhinalBioAsyncIntegrationTest, PromiseWithFuture) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(
        BIO_CHANNEL_NOREPINEPHRINE, sizeof(float));

    if (!promise) {
        GTEST_SKIP() << "Promise creation not available";
    }

    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
    EXPECT_NE(nullptr, future);

    /* Complete promise with novelty signal */
    float novelty_signal = 0.9f;
    nimcp_bio_promise_complete(promise, &novelty_signal);

    if (future) {
        nimcp_bio_future_destroy(future);
    }
    nimcp_bio_promise_destroy(promise);
}

TEST_F(PerirhinalBioAsyncIntegrationTest, AllNeuromodulatorChannels) {
    /* Test promises for all object-recognition-relevant channels */
    nimcp_bio_channel_type_t channels[] = {
        BIO_CHANNEL_DOPAMINE,        /* Reward for recognition */
        BIO_CHANNEL_NOREPINEPHRINE,  /* Novelty alerting */
        BIO_CHANNEL_ACETYLCHOLINE,   /* Attention/encoding */
        BIO_CHANNEL_SEROTONIN        /* Confidence modulation */
    };

    for (auto channel : channels) {
        nimcp_bio_promise_t promise = nimcp_bio_promise_create(channel, sizeof(float));
        if (promise) {
            float value = 0.7f;
            nimcp_bio_promise_complete(promise, &value);
            nimcp_bio_promise_destroy(promise);
        }
    }
}

/*=============================================================================
 * UPDATE CYCLE WITH BIO-ASYNC TESTS
 *===========================================================================*/

TEST_F(PerirhinalBioAsyncIntegrationTest, UpdateCycleWithBioAsync) {
    /* Run several update cycles */
    for (int i = 0; i < 100; i++) {
        int result = perirhinal_update(perirhinal, 10.0f);
        EXPECT_EQ(0, result);
    }

    perirhinal_stats_t stats;
    EXPECT_EQ(0, perirhinal_get_stats(perirhinal, &stats));
    EXPECT_GE(stats.updates_processed, 100u);
}

TEST_F(PerirhinalBioAsyncIntegrationTest, BidirectionalUpdateWithBioAsync) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not available for bidirectional test";
    }

    /* Run bidirectional updates (processes incoming and outgoing messages) */
    for (int i = 0; i < 50; i++) {
        int result = perirhinal_bidirectional_update(perirhinal, 10.0f);
        EXPECT_EQ(0, result);
    }
}

TEST_F(PerirhinalBioAsyncIntegrationTest, ProcessIncomingOutgoingWithBioAsync) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not available";
    }

    /* Process incoming data from bridges */
    int result = perirhinal_process_incoming(perirhinal);
    EXPECT_EQ(0, result);

    /* Send outgoing data to bridges */
    result = perirhinal_send_outgoing(perirhinal);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * ERROR HANDLING TESTS
 *===========================================================================*/

TEST_F(PerirhinalBioAsyncIntegrationTest, HandleInvalidChannel) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(
        (nimcp_bio_channel_type_t)99, sizeof(float));
    if (promise) {
        nimcp_bio_promise_destroy(promise);
    }
}

TEST_F(PerirhinalBioAsyncIntegrationTest, HandleDoubleUnregister) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not available";
    }

    bio_module_info_t info;
    memset(&info, 0, sizeof(info));
    info.module_id = BIO_MODULE_TEMPORAL_CORTEX;  /* Perirhinal is part of temporal cortex */
    info.module_name = "perirhinal";
    info.user_data = perirhinal;

    bio_module_context_t ctx = bio_router_register_module(&info);
    ASSERT_NE(nullptr, ctx);

    bio_router_unregister_module(ctx);
    bio_router_unregister_module(ctx);  /* Should not crash */
}

/*=============================================================================
 * STATE PERSISTENCE TESTS
 *===========================================================================*/

TEST_F(PerirhinalBioAsyncIntegrationTest, RouterStateAcrossOperations) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not available";
    }

    /* Register module */
    bio_module_info_t info;
    memset(&info, 0, sizeof(info));
    info.module_id = BIO_MODULE_TEMPORAL_CORTEX;  /* Perirhinal is part of temporal cortex */
    info.module_name = "perirhinal";
    info.user_data = perirhinal;

    bio_module_context_t ctx = bio_router_register_module(&info);
    ASSERT_NE(nullptr, ctx);

    /* Perform perirhinal operations */
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    perirhinal_encode_object(perirhinal, features, 256, "test", &object_id);

    /* Run update cycles */
    for (int i = 0; i < 10; i++) {
        perirhinal_update(perirhinal, 10.0f);
    }

    /* Router should still be operational */
    EXPECT_TRUE(bio_router_is_initialized());

    bio_router_stats_t stats;
    bio_router_get_stats(&stats);
    EXPECT_GE(stats.active_modules, 1u);

    bio_router_unregister_module(ctx);
}

TEST_F(PerirhinalBioAsyncIntegrationTest, PerirhinalResetDoesNotAffectRouter) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not available";
    }

    /* Perform operations */
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    perirhinal_encode_object(perirhinal, features, 256, "test", &object_id);

    /* Reset perirhinal */
    perirhinal_reset(perirhinal);
    /* Status should be valid after reset */
    perirhinal_status_t status = perirhinal_get_status(perirhinal);
    EXPECT_GE((int)status, 0);

    /* Router should be unaffected */
    EXPECT_TRUE(bio_router_is_initialized());

    bio_router_stats_t stats;
    EXPECT_EQ(NIMCP_OK, bio_router_get_stats(&stats));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
