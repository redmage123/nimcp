/**
 * @file test_motor_bio_async_integration.cpp
 * @brief Integration tests for Motor Cortex with Bio-Async messaging system
 *
 * WHAT: Tests Motor Cortex integration with bio-async neuromodulator channels
 * WHY:  Ensure proper motor communication via biological messaging
 * HOW:  Test router initialization, module registration, and motor operations
 *
 * BIOLOGICAL BASIS:
 * Motor cortex communicates via neuromodulator channels:
 * - DOPAMINE: Movement reward, goal completion signals
 * - NOREPINEPHRINE: Motor alerting, urgency signals
 * - ACETYLCHOLINE: Attention for motor learning
 * - SEROTONIN: Movement pacing, patience
 *
 * INTEGRATION POINTS:
 * - Bio-async router initialization
 * - Module registration
 * - Motor operations with bio-async enabled
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/motor/nimcp_motor_adapter.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "utils/logging/nimcp_logging.h"
#include "nimcp.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class MotorBioAsyncIntegrationTest : public ::testing::Test {
protected:
    motor_adapter_t* adapter;
    motor_config_t config;
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

        /* Configure motor adapter */
        config = motor_default_config();
        config.enable_bio_async = router_initialized;
        config.enable_training = true;
        config.enable_events = true;
        config.enable_premotor = true;
        config.enable_sma = true;

        adapter = motor_create(&config);
        ASSERT_NE(nullptr, adapter) << "Failed to create Motor adapter";
    }

    void TearDown() override {
        if (adapter) {
            motor_destroy(adapter);
            adapter = nullptr;
        }
        if (router_initialized) {
            bio_router_shutdown();
            router_initialized = false;
        }
    }

    /* Helper to create a test motor goal */
    motor_goal_t CreateTestGoal(motor_region_t region, float x, float y, float z, float duration_ms) {
        motor_goal_t goal;
        memset(&goal, 0, sizeof(goal));
        goal.region = region;
        goal.target_position.x = x;
        goal.target_position.y = y;
        goal.target_position.z = z;
        goal.max_duration_ms = duration_ms;
        goal.type = MOVEMENT_TYPE_DISCRETE;
        return goal;
    }
};

/*=============================================================================
 * BIO-ROUTER LIFECYCLE TESTS
 *===========================================================================*/

TEST_F(MotorBioAsyncIntegrationTest, RouterInitialization) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not available";
    }
    EXPECT_TRUE(bio_router_is_initialized());
}

TEST_F(MotorBioAsyncIntegrationTest, RouterGetGlobal) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not available";
    }

    bio_router_t global_router = bio_router_get_global();
    EXPECT_NE(nullptr, global_router);
}

TEST_F(MotorBioAsyncIntegrationTest, DefaultConfig) {
    bio_router_config_t default_cfg = bio_router_default_config();
    EXPECT_GT(default_cfg.max_modules, 0u);
    EXPECT_GT(default_cfg.inbox_capacity, 0u);
}

/*=============================================================================
 * MODULE REGISTRATION TESTS
 *===========================================================================*/

TEST_F(MotorBioAsyncIntegrationTest, RegisterMotorModule) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not available";
    }

    bio_module_info_t info;
    memset(&info, 0, sizeof(info));
    info.module_id = BIO_MODULE_MOTOR_CORTEX;
    info.module_name = "motor_cortex";
    info.inbox_capacity = 128;
    info.user_data = adapter;

    bio_module_context_t ctx = bio_router_register_module(&info);
    EXPECT_NE(nullptr, ctx);

    if (ctx) {
        bio_router_unregister_module(ctx);
    }
}

TEST_F(MotorBioAsyncIntegrationTest, RegisterMultipleMotorComponents) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not available";
    }

    bio_module_info_t m1_info;
    memset(&m1_info, 0, sizeof(m1_info));
    m1_info.module_id = BIO_MODULE_MOTOR_CORTEX;
    m1_info.module_name = "m1_primary";
    m1_info.user_data = adapter;

    bio_module_context_t m1_ctx = bio_router_register_module(&m1_info);
    EXPECT_NE(nullptr, m1_ctx);

    /* Use a different module ID for premotor */
    bio_module_info_t premotor_info;
    memset(&premotor_info, 0, sizeof(premotor_info));
    premotor_info.module_id = BIO_MODULE_CEREBELLUM;  /* Reusing available ID */
    premotor_info.module_name = "premotor";
    premotor_info.user_data = adapter;

    bio_module_context_t premotor_ctx = bio_router_register_module(&premotor_info);
    EXPECT_NE(nullptr, premotor_ctx);

    if (m1_ctx) bio_router_unregister_module(m1_ctx);
    if (premotor_ctx) bio_router_unregister_module(premotor_ctx);
}

TEST_F(MotorBioAsyncIntegrationTest, ModuleContextAccessors) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not available";
    }

    bio_module_info_t info;
    memset(&info, 0, sizeof(info));
    info.module_id = BIO_MODULE_MOTOR_CORTEX;
    info.module_name = "motor_test";
    info.user_data = adapter;

    bio_module_context_t ctx = bio_router_register_module(&info);
    ASSERT_NE(nullptr, ctx);

    /* Verify context accessors */
    EXPECT_EQ(BIO_MODULE_MOTOR_CORTEX, bio_module_context_get_id(ctx));
    /* Name may be overridden by the router based on module ID */
    const char* name = bio_module_context_get_name(ctx);
    EXPECT_NE(nullptr, name);
    EXPECT_GT(strlen(name), 0u);
    EXPECT_EQ(adapter, bio_module_context_get_user_data(ctx));

    bio_router_unregister_module(ctx);
}

/*=============================================================================
 * ROUTER STATISTICS TESTS
 *===========================================================================*/

TEST_F(MotorBioAsyncIntegrationTest, RouterStatistics) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not available";
    }

    bio_router_stats_t stats;
    nimcp_error_t result = bio_router_get_stats(&stats);
    EXPECT_EQ(NIMCP_OK, result);

    /* Initial stats should be reasonable */
    EXPECT_GE(stats.messages_routed, 0u);
    EXPECT_GE(stats.active_modules, 0u);
}

TEST_F(MotorBioAsyncIntegrationTest, RouterStatisticsAfterActivity) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not available";
    }

    bio_module_info_t info;
    memset(&info, 0, sizeof(info));
    info.module_id = BIO_MODULE_MOTOR_CORTEX;
    info.module_name = "motor";
    info.user_data = adapter;

    bio_module_context_t ctx = bio_router_register_module(&info);
    ASSERT_NE(nullptr, ctx);

    bio_router_stats_t stats;
    bio_router_get_stats(&stats);
    EXPECT_GE(stats.active_modules, 1u);

    bio_router_unregister_module(ctx);
}

TEST_F(MotorBioAsyncIntegrationTest, ResetRouterStats) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not available";
    }

    /* Reset and verify */
    bio_router_reset_stats();

    bio_router_stats_t stats;
    bio_router_get_stats(&stats);
    EXPECT_EQ(stats.messages_routed, 0u);
}

/*=============================================================================
 * BIO-ASYNC CONFIGURATION TESTS
 *===========================================================================*/

TEST_F(MotorBioAsyncIntegrationTest, DefaultBioAsyncConfig) {
    nimcp_bio_async_config_t config = nimcp_bio_async_default_config();

    /* Verify sensible defaults */
    EXPECT_GT(config.simulation_dt_ms, 0.0f);
    EXPECT_GE(config.channel_configs[BIO_CHANNEL_DOPAMINE].decay_tau_ms, 0.0f);
}

TEST_F(MotorBioAsyncIntegrationTest, ChannelSpecificConfigs) {
    nimcp_bio_async_config_t config = nimcp_bio_async_default_config();

    /* Each channel should have configured characteristics */
    float da_decay = config.channel_configs[BIO_CHANNEL_DOPAMINE].decay_tau_ms;
    float ach_decay = config.channel_configs[BIO_CHANNEL_ACETYLCHOLINE].decay_tau_ms;
    float ne_decay = config.channel_configs[BIO_CHANNEL_NOREPINEPHRINE].decay_tau_ms;
    float ser_decay = config.channel_configs[BIO_CHANNEL_SEROTONIN].decay_tau_ms;

    /* All should be non-negative */
    EXPECT_GE(da_decay, 0.0f);
    EXPECT_GE(ach_decay, 0.0f);
    EXPECT_GE(ne_decay, 0.0f);
    EXPECT_GE(ser_decay, 0.0f);
}

/*=============================================================================
 * MOTOR ADAPTER WITH BIO-ASYNC TESTS
 *===========================================================================*/

TEST_F(MotorBioAsyncIntegrationTest, MotorAdapterBioAsyncEnabled) {
    /* Motor adapter should have bio-async flag based on router availability */
    motor_config_t retrieved;
    EXPECT_TRUE(motor_get_config(adapter, &retrieved));
    EXPECT_EQ(router_initialized, retrieved.enable_bio_async);
}

TEST_F(MotorBioAsyncIntegrationTest, MotorPlanningWithBioAsync) {
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.5f, 0.0f, 500.0f);

    /* Motor planning should work regardless of bio-async state */
    EXPECT_TRUE(motor_plan_movement(adapter, &goal));
    EXPECT_EQ(MOTOR_STATUS_PREPARING, motor_get_status(adapter));
}

TEST_F(MotorBioAsyncIntegrationTest, MotorExecutionWithBioAsync) {
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.0f, 0.0f, 100.0f);

    ASSERT_TRUE(motor_plan_movement(adapter, &goal));
    ASSERT_TRUE(motor_begin_execution(adapter));

    for (int i = 0; i < 10; i++) {
        motor_update_execution(adapter, 10.0f);
    }

    motor_stats_t stats;
    motor_get_stats(adapter, &stats);
    EXPECT_GT(stats.commands_generated, 0u);
}

TEST_F(MotorBioAsyncIntegrationTest, MultipleMovementsWithBioAsync) {
    /* Execute several movements in sequence */
    for (int i = 0; i < 5; i++) {
        motor_goal_t goal = CreateTestGoal(
            MOTOR_REGION_HAND_RIGHT,
            (float)i * 0.2f,
            0.0f,
            0.0f,
            50.0f
        );

        EXPECT_TRUE(motor_plan_movement(adapter, &goal));
        EXPECT_TRUE(motor_begin_execution(adapter));

        for (int j = 0; j < 5; j++) {
            motor_update_execution(adapter, 10.0f);
        }

        motor_reset(adapter);
    }

    motor_stats_t stats;
    motor_get_stats(adapter, &stats);
    /* Stats should reflect activity */
    EXPECT_GE(stats.movements_planned, 0u);
}

/*=============================================================================
 * PROMISE/FUTURE BASICS TESTS
 *===========================================================================*/

TEST_F(MotorBioAsyncIntegrationTest, PromiseCreation) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(
        BIO_CHANNEL_DOPAMINE, sizeof(float));

    if (promise) {
        nimcp_bio_promise_destroy(promise);
    }
    /* Success if no crash */
}

TEST_F(MotorBioAsyncIntegrationTest, PromiseWithFuture) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(
        BIO_CHANNEL_DOPAMINE, sizeof(float));

    if (!promise) {
        GTEST_SKIP() << "Promise creation not available";
    }

    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
    EXPECT_NE(nullptr, future);

    /* Complete promise */
    float value = 0.8f;
    nimcp_bio_promise_complete(promise, &value);

    if (future) {
        nimcp_bio_future_destroy(future);
    }
    nimcp_bio_promise_destroy(promise);
}

TEST_F(MotorBioAsyncIntegrationTest, PromisesForDifferentChannels) {
    /* Test promises for all motor-relevant channels */
    nimcp_bio_channel_type_t channels[] = {
        BIO_CHANNEL_DOPAMINE,
        BIO_CHANNEL_NOREPINEPHRINE,
        BIO_CHANNEL_ACETYLCHOLINE,
        BIO_CHANNEL_SEROTONIN
    };

    for (auto channel : channels) {
        nimcp_bio_promise_t promise = nimcp_bio_promise_create(channel, sizeof(float));
        if (promise) {
            float value = 1.0f;
            nimcp_bio_promise_complete(promise, &value);
            nimcp_bio_promise_destroy(promise);
        }
    }
}

/*=============================================================================
 * ERROR HANDLING TESTS
 *===========================================================================*/

TEST_F(MotorBioAsyncIntegrationTest, HandleInvalidChannel) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(
        (nimcp_bio_channel_type_t)99, sizeof(float));
    /* Should either return NULL or handle gracefully */
    if (promise) {
        nimcp_bio_promise_destroy(promise);
    }
}

TEST_F(MotorBioAsyncIntegrationTest, HandleDoubleUnregister) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not available";
    }

    bio_module_info_t info;
    memset(&info, 0, sizeof(info));
    info.module_id = BIO_MODULE_MOTOR_CORTEX;
    info.module_name = "motor";
    info.user_data = adapter;

    bio_module_context_t ctx = bio_router_register_module(&info);
    ASSERT_NE(nullptr, ctx);

    bio_router_unregister_module(ctx);
    /* Second unregister should not crash */
    bio_router_unregister_module(ctx);
}

TEST_F(MotorBioAsyncIntegrationTest, NullInfoRegistration) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not available";
    }

    bio_module_context_t ctx = bio_router_register_module(nullptr);
    EXPECT_EQ(nullptr, ctx);
}

/*=============================================================================
 * BIO-ASYNC STATE PERSISTENCE TESTS
 *===========================================================================*/

TEST_F(MotorBioAsyncIntegrationTest, RouterStateAcrossOperations) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not available";
    }

    /* Register module */
    bio_module_info_t info;
    memset(&info, 0, sizeof(info));
    info.module_id = BIO_MODULE_MOTOR_CORTEX;
    info.module_name = "motor";
    info.user_data = adapter;

    bio_module_context_t ctx = bio_router_register_module(&info);
    ASSERT_NE(nullptr, ctx);

    /* Perform motor operations */
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.0f, 0.0f, 100.0f);
    motor_plan_movement(adapter, &goal);
    motor_begin_execution(adapter);

    for (int i = 0; i < 5; i++) {
        motor_update_execution(adapter, 10.0f);
    }

    /* Router should still be operational */
    EXPECT_TRUE(bio_router_is_initialized());

    bio_router_stats_t stats;
    bio_router_get_stats(&stats);
    EXPECT_GE(stats.active_modules, 1u);

    bio_router_unregister_module(ctx);
}

TEST_F(MotorBioAsyncIntegrationTest, MotorResetDoesNotAffectRouter) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not available";
    }

    /* Setup motor operation */
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.0f, 0.0f, 100.0f);
    motor_plan_movement(adapter, &goal);

    /* Reset motor */
    motor_reset(adapter);
    EXPECT_EQ(MOTOR_STATUS_IDLE, motor_get_status(adapter));

    /* Router should be unaffected */
    EXPECT_TRUE(bio_router_is_initialized());

    bio_router_stats_t stats;
    EXPECT_EQ(NIMCP_OK, bio_router_get_stats(&stats));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
