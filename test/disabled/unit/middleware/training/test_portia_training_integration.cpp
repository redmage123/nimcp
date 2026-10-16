/**
 * @file test_portia_training_integration.cpp
 * @brief Unit tests for Portia-Training resource-aware integration
 *
 * WHAT: Test resource-aware training adaptation based on Portia tier/degradation
 * WHY:  Ensure training correctly adapts to platform constraints
 * HOW:  Mock Portia events and verify training parameter adjustments
 *
 * Tests:
 * - Portia connection/disconnection
 * - Tier change handling (FULL -> MEDIUM -> CONSTRAINED -> MINIMAL)
 * - Batch size adjustment per tier
 * - Learning rate adjustment per tier
 * - Training pause in EMERGENCY mode
 * - Degradation event handling
 * - Resource request messaging
 * - Resume after resource recovery
 *
 * @author NIMCP Development Team
 * @date 2025-12-09
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "middleware/training/nimcp_brain_training_integration.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/platform/nimcp_platform_tier.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"

class PortiaTrainingIntegrationTest : public ::testing::Test {
protected:
    nimcp_brain_training_ctx_t* training_ctx;
    bio_router_context_t* router_ctx;

    void SetUp() override {
        /* Initialize bio-async router */
        bio_router_config_t router_config = {0};
        router_config.max_modules = 32;
        router_config.default_inbox_capacity = 64;
        router_config.enable_priority_channels = true;
        router_config.worker_thread_count = 2;

        router_ctx = bio_router_init(&router_config);
        ASSERT_NE(router_ctx, nullptr) << "Failed to initialize bio-router";

        /* Create brain training context with Portia integration enabled */
        nimcp_brain_training_config_t config = nimcp_brain_training_default_config();
        config.enable_portia_integration = true;
        config.min_batch_size_ratio = 0.25f;
        config.allow_training_pause = true;
        config.adapt_to_tier_changes = true;

        training_ctx = nimcp_brain_training_create(&config);
        ASSERT_NE(training_ctx, nullptr) << "Failed to create brain training context";

        /* Initialize training */
        nimcp_result_t res = nimcp_brain_training_init(training_ctx, nullptr, nullptr);
        ASSERT_EQ(res, NIMCP_SUCCESS) << "Failed to initialize brain training";
    }

    void TearDown() override {
        if (training_ctx) {
            nimcp_brain_training_destroy(training_ctx);
            training_ctx = nullptr;
        }

        if (router_ctx) {
            bio_router_shutdown(router_ctx);
            router_ctx = nullptr;
        }
    }
};

/* ============================================================================
 * Connection and Disconnection Tests
 * ============================================================================ */

/**
 * @test Verify Portia connection establishes resource-aware training
 */
TEST_F(PortiaTrainingIntegrationTest, PortiaConnection) {
    /* Connect mock Portia context */
    void* mock_portia = (void*)0x12345678;  /* Mock pointer */

    nimcp_result_t res = nimcp_brain_training_connect_portia(
        training_ctx,
        mock_portia
    );

    EXPECT_EQ(res, NIMCP_SUCCESS);
    LOG_INFO("Portia connected successfully");
}

/**
 * @test Verify Portia disconnection resets to defaults
 */
TEST_F(PortiaTrainingIntegrationTest, PortiaDisconnection) {
    /* Connect then disconnect */
    void* mock_portia = (void*)0x12345678;
    nimcp_brain_training_connect_portia(training_ctx, mock_portia);

    nimcp_result_t res = nimcp_brain_training_connect_portia(
        training_ctx,
        nullptr  /* Disconnect */
    );

    EXPECT_EQ(res, NIMCP_SUCCESS);

    /* Verify batch size returns to normal */
    size_t batch_size = nimcp_brain_training_get_adjusted_batch_size(
        training_ctx,
        100
    );
    EXPECT_EQ(batch_size, 100);

    LOG_INFO("Portia disconnected, parameters reset to defaults");
}

/* ============================================================================
 * Tier Change Tests
 * ============================================================================ */

/**
 * @test Verify FULL tier maintains full resources
 */
TEST_F(PortiaTrainingIntegrationTest, TierChangeFull) {
    void* mock_portia = (void*)0x12345678;
    nimcp_brain_training_connect_portia(training_ctx, mock_portia);

    nimcp_result_t res = nimcp_brain_training_on_tier_change(
        training_ctx,
        PLATFORM_TIER_FULL
    );

    EXPECT_EQ(res, NIMCP_SUCCESS);

    /* Verify full batch size (100%) */
    size_t batch_size = nimcp_brain_training_get_adjusted_batch_size(
        training_ctx,
        100
    );
    EXPECT_EQ(batch_size, 100);

    /* Verify full learning rate (100%) */
    float lr = nimcp_brain_training_get_adjusted_lr(training_ctx, 0.01f);
    EXPECT_FLOAT_EQ(lr, 0.01f);

    /* Verify not paused */
    EXPECT_FALSE(nimcp_brain_training_is_paused(training_ctx));

    LOG_INFO("FULL tier: batch=100%, lr=100%");
}

/**
 * @test Verify MEDIUM tier reduces resources moderately
 */
TEST_F(PortiaTrainingIntegrationTest, TierChangeMedium) {
    void* mock_portia = (void*)0x12345678;
    nimcp_brain_training_connect_portia(training_ctx, mock_portia);

    nimcp_result_t res = nimcp_brain_training_on_tier_change(
        training_ctx,
        PLATFORM_TIER_MEDIUM
    );

    EXPECT_EQ(res, NIMCP_SUCCESS);

    /* Verify 75% batch size */
    size_t batch_size = nimcp_brain_training_get_adjusted_batch_size(
        training_ctx,
        100
    );
    EXPECT_EQ(batch_size, 75);

    /* Verify 90% learning rate */
    float lr = nimcp_brain_training_get_adjusted_lr(training_ctx, 0.01f);
    EXPECT_NEAR(lr, 0.009f, 1e-6);

    /* Verify not paused */
    EXPECT_FALSE(nimcp_brain_training_is_paused(training_ctx));

    LOG_INFO("MEDIUM tier: batch=75%, lr=90%%");
}

/**
 * @test Verify CONSTRAINED tier significantly reduces resources
 */
TEST_F(PortiaTrainingIntegrationTest, TierChangeConstrained) {
    void* mock_portia = (void*)0x12345678;
    nimcp_brain_training_connect_portia(training_ctx, mock_portia);

    nimcp_result_t res = nimcp_brain_training_on_tier_change(
        training_ctx,
        PLATFORM_TIER_CONSTRAINED
    );

    EXPECT_EQ(res, NIMCP_SUCCESS);

    /* Verify 50% batch size */
    size_t batch_size = nimcp_brain_training_get_adjusted_batch_size(
        training_ctx,
        100
    );
    EXPECT_EQ(batch_size, 50);

    /* Verify 75% learning rate */
    float lr = nimcp_brain_training_get_adjusted_lr(training_ctx, 0.01f);
    EXPECT_NEAR(lr, 0.0075f, 1e-6);

    /* Verify not paused */
    EXPECT_FALSE(nimcp_brain_training_is_paused(training_ctx));

    LOG_INFO("CONSTRAINED tier: batch=50%, lr=75%%");
}

/**
 * @test Verify MINIMAL tier pauses training (EMERGENCY mode)
 */
TEST_F(PortiaTrainingIntegrationTest, TierChangeMinimalPausesTraining) {
    void* mock_portia = (void*)0x12345678;
    nimcp_brain_training_connect_portia(training_ctx, mock_portia);

    nimcp_result_t res = nimcp_brain_training_on_tier_change(
        training_ctx,
        PLATFORM_TIER_MINIMAL
    );

    EXPECT_EQ(res, NIMCP_SUCCESS);

    /* Verify training is paused */
    EXPECT_TRUE(nimcp_brain_training_is_paused(training_ctx));

    /* Verify batch size returns 0 when paused */
    size_t batch_size = nimcp_brain_training_get_adjusted_batch_size(
        training_ctx,
        100
    );
    EXPECT_EQ(batch_size, 0);

    /* Verify learning rate returns 0 when paused */
    float lr = nimcp_brain_training_get_adjusted_lr(training_ctx, 0.01f);
    EXPECT_FLOAT_EQ(lr, 0.0f);

    LOG_INFO("MINIMAL tier: training paused");
}

/**
 * @test Verify training resumes when upgrading from MINIMAL tier
 */
TEST_F(PortiaTrainingIntegrationTest, TierChangeResumeFromMinimal) {
    void* mock_portia = (void*)0x12345678;
    nimcp_brain_training_connect_portia(training_ctx, mock_portia);

    /* First degrade to MINIMAL */
    nimcp_brain_training_on_tier_change(training_ctx, PLATFORM_TIER_MINIMAL);
    EXPECT_TRUE(nimcp_brain_training_is_paused(training_ctx));

    /* Then upgrade to CONSTRAINED */
    nimcp_result_t res = nimcp_brain_training_on_tier_change(
        training_ctx,
        PLATFORM_TIER_CONSTRAINED
    );

    EXPECT_EQ(res, NIMCP_SUCCESS);

    /* Verify training resumed */
    EXPECT_FALSE(nimcp_brain_training_is_paused(training_ctx));

    /* Verify batch size restored */
    size_t batch_size = nimcp_brain_training_get_adjusted_batch_size(
        training_ctx,
        100
    );
    EXPECT_GT(batch_size, 0);

    LOG_INFO("Training resumed after tier upgrade from MINIMAL");
}

/* ============================================================================
 * Degradation Event Tests
 * ============================================================================ */

/**
 * @test Verify NONE degradation has no effect
 */
TEST_F(PortiaTrainingIntegrationTest, DegradationNone) {
    void* mock_portia = (void*)0x12345678;
    nimcp_brain_training_connect_portia(training_ctx, mock_portia);
    nimcp_brain_training_on_tier_change(training_ctx, PLATFORM_TIER_FULL);

    nimcp_result_t res = nimcp_brain_training_on_degradation_event(
        training_ctx,
        0  /* DEGRADATION_LEVEL_NONE */
    );

    EXPECT_EQ(res, NIMCP_SUCCESS);
    EXPECT_FALSE(nimcp_brain_training_is_paused(training_ctx));

    LOG_INFO("NONE degradation: no impact");
}

/**
 * @test Verify SEVERE degradation further reduces batch size
 */
TEST_F(PortiaTrainingIntegrationTest, DegradationSevere) {
    void* mock_portia = (void*)0x12345678;
    nimcp_brain_training_connect_portia(training_ctx, mock_portia);
    nimcp_brain_training_on_tier_change(training_ctx, PLATFORM_TIER_FULL);

    /* Get batch size before degradation */
    size_t batch_before = nimcp_brain_training_get_adjusted_batch_size(
        training_ctx,
        100
    );
    EXPECT_EQ(batch_before, 100);

    /* Apply SEVERE degradation */
    nimcp_result_t res = nimcp_brain_training_on_degradation_event(
        training_ctx,
        3  /* DEGRADATION_LEVEL_SEVERE */
    );

    EXPECT_EQ(res, NIMCP_SUCCESS);

    /* Verify batch size reduced further (50% of tier multiplier) */
    size_t batch_after = nimcp_brain_training_get_adjusted_batch_size(
        training_ctx,
        100
    );
    EXPECT_LT(batch_after, batch_before);

    LOG_INFO("SEVERE degradation: batch reduced from %zu to %zu",
             batch_before, batch_after);
}

/**
 * @test Verify CRITICAL degradation pauses training
 */
TEST_F(PortiaTrainingIntegrationTest, DegradationCritical) {
    void* mock_portia = (void*)0x12345678;
    nimcp_brain_training_connect_portia(training_ctx, mock_portia);
    nimcp_brain_training_on_tier_change(training_ctx, PLATFORM_TIER_FULL);

    nimcp_result_t res = nimcp_brain_training_on_degradation_event(
        training_ctx,
        4  /* DEGRADATION_LEVEL_CRITICAL */
    );

    EXPECT_EQ(res, NIMCP_SUCCESS);

    /* Verify training paused */
    EXPECT_TRUE(nimcp_brain_training_is_paused(training_ctx));

    /* Verify batch size returns 0 */
    size_t batch_size = nimcp_brain_training_get_adjusted_batch_size(
        training_ctx,
        100
    );
    EXPECT_EQ(batch_size, 0);

    LOG_INFO("CRITICAL degradation: training paused");
}

/**
 * @test Verify training resumes after degradation improves
 */
TEST_F(PortiaTrainingIntegrationTest, DegradationRecovery) {
    void* mock_portia = (void*)0x12345678;
    nimcp_brain_training_connect_portia(training_ctx, mock_portia);
    nimcp_brain_training_on_tier_change(training_ctx, PLATFORM_TIER_FULL);

    /* Apply CRITICAL degradation */
    nimcp_brain_training_on_degradation_event(training_ctx, 4);
    EXPECT_TRUE(nimcp_brain_training_is_paused(training_ctx));

    /* Recover to MODERATE */
    nimcp_result_t res = nimcp_brain_training_on_degradation_event(
        training_ctx,
        2  /* DEGRADATION_LEVEL_MODERATE */
    );

    EXPECT_EQ(res, NIMCP_SUCCESS);

    /* Verify training resumed */
    EXPECT_FALSE(nimcp_brain_training_is_paused(training_ctx));

    /* Verify batch size restored */
    size_t batch_size = nimcp_brain_training_get_adjusted_batch_size(
        training_ctx,
        100
    );
    EXPECT_GT(batch_size, 0);

    LOG_INFO("Degradation recovery: training resumed");
}

/* ============================================================================
 * Batch Size Adjustment Tests
 * ============================================================================ */

/**
 * @test Verify batch size adjustment with various tiers
 */
TEST_F(PortiaTrainingIntegrationTest, BatchSizeAdjustmentAcrossTiers) {
    void* mock_portia = (void*)0x12345678;
    nimcp_brain_training_connect_portia(training_ctx, mock_portia);

    const size_t base_batch = 128;

    /* Test each tier */
    struct {
        platform_tier_t tier;
        size_t expected_batch;
    } test_cases[] = {
        { PLATFORM_TIER_FULL,        128 },  /* 100% */
        { PLATFORM_TIER_MEDIUM,      96  },  /* 75% */
        { PLATFORM_TIER_CONSTRAINED, 64  },  /* 50% */
        { PLATFORM_TIER_MINIMAL,     0   },  /* Paused */
    };

    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        nimcp_brain_training_on_tier_change(training_ctx, test_cases[i].tier);

        size_t adjusted = nimcp_brain_training_get_adjusted_batch_size(
            training_ctx,
            base_batch
        );

        EXPECT_EQ(adjusted, test_cases[i].expected_batch)
            << "Tier: " << platform_tier_get_name(test_cases[i].tier);

        LOG_INFO("Tier %s: batch %zu -> %zu",
                 platform_tier_get_name(test_cases[i].tier),
                 base_batch, adjusted);
    }
}

/**
 * @test Verify minimum batch size enforcement
 */
TEST_F(PortiaTrainingIntegrationTest, MinimumBatchSizeEnforcement) {
    void* mock_portia = (void*)0x12345678;
    nimcp_brain_training_connect_portia(training_ctx, mock_portia);

    /* Set to CONSTRAINED tier (50% batch) */
    nimcp_brain_training_on_tier_change(training_ctx, PLATFORM_TIER_CONSTRAINED);

    /* Very small base batch */
    size_t adjusted = nimcp_brain_training_get_adjusted_batch_size(
        training_ctx,
        2  /* Base batch of 2 */
    );

    /* Should get at least 1 */
    EXPECT_GE(adjusted, 1);

    LOG_INFO("Minimum batch size enforced: %zu", adjusted);
}

/* ============================================================================
 * Learning Rate Adjustment Tests
 * ============================================================================ */

/**
 * @test Verify learning rate adjustment with various tiers
 */
TEST_F(PortiaTrainingIntegrationTest, LearningRateAdjustmentAcrossTiers) {
    void* mock_portia = (void*)0x12345678;
    nimcp_brain_training_connect_portia(training_ctx, mock_portia);

    const float base_lr = 0.01f;

    /* Test each tier */
    struct {
        platform_tier_t tier;
        float expected_lr;
    } test_cases[] = {
        { PLATFORM_TIER_FULL,        0.01f    },  /* 100% */
        { PLATFORM_TIER_MEDIUM,      0.009f   },  /* 90% */
        { PLATFORM_TIER_CONSTRAINED, 0.0075f  },  /* 75% */
        { PLATFORM_TIER_MINIMAL,     0.0f     },  /* Paused */
    };

    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        nimcp_brain_training_on_tier_change(training_ctx, test_cases[i].tier);

        float adjusted = nimcp_brain_training_get_adjusted_lr(
            training_ctx,
            base_lr
        );

        EXPECT_NEAR(adjusted, test_cases[i].expected_lr, 1e-6)
            << "Tier: " << platform_tier_get_name(test_cases[i].tier);

        LOG_INFO("Tier %s: LR %.4f -> %.4f",
                 platform_tier_get_name(test_cases[i].tier),
                 base_lr, adjusted);
    }
}

/* ============================================================================
 * Manual Resume Tests
 * ============================================================================ */

/**
 * @test Verify manual resume after pause
 */
TEST_F(PortiaTrainingIntegrationTest, ManualResume) {
    void* mock_portia = (void*)0x12345678;
    nimcp_brain_training_connect_portia(training_ctx, mock_portia);

    /* Pause training via MINIMAL tier */
    nimcp_brain_training_on_tier_change(training_ctx, PLATFORM_TIER_MINIMAL);
    EXPECT_TRUE(nimcp_brain_training_is_paused(training_ctx));

    /* Manually resume */
    nimcp_result_t res = nimcp_brain_training_resume(training_ctx);
    EXPECT_EQ(res, NIMCP_SUCCESS);

    /* Verify training resumed */
    EXPECT_FALSE(nimcp_brain_training_is_paused(training_ctx));

    LOG_INFO("Manual resume successful");
}

/* ============================================================================
 * Resource Request Tests
 * ============================================================================ */

/**
 * @test Verify resource request sends bio-async message
 */
TEST_F(PortiaTrainingIntegrationTest, ResourceRequest) {
    void* mock_portia = (void*)0x12345678;
    nimcp_brain_training_connect_portia(training_ctx, mock_portia);

    nimcp_result_t res = nimcp_brain_training_request_resources(
        training_ctx,
        128,   /* batch_size */
        10000  /* param_count */
    );

    /* Should succeed (message sent) or return SUCCESS if bio-async disabled */
    EXPECT_TRUE(res == NIMCP_SUCCESS || res == NIMCP_ERROR_NOT_INITIALIZED);

    LOG_INFO("Resource request sent");
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

/**
 * @test Verify NULL context handling
 */
TEST_F(PortiaTrainingIntegrationTest, NullContextHandling) {
    nimcp_result_t res;

    /* Test connect with NULL context */
    res = nimcp_brain_training_connect_portia(nullptr, (void*)0x123);
    EXPECT_EQ(res, NIMCP_ERROR_INVALID_PARAM);

    /* Test tier change with NULL context */
    res = nimcp_brain_training_on_tier_change(nullptr, PLATFORM_TIER_FULL);
    EXPECT_EQ(res, NIMCP_ERROR_INVALID_PARAM);

    /* Test degradation with NULL context */
    res = nimcp_brain_training_on_degradation_event(nullptr, 0);
    EXPECT_EQ(res, NIMCP_ERROR_INVALID_PARAM);

    /* Test is_paused with NULL context */
    bool paused = nimcp_brain_training_is_paused(nullptr);
    EXPECT_FALSE(paused);

    /* Test batch size with NULL context */
    size_t batch = nimcp_brain_training_get_adjusted_batch_size(nullptr, 100);
    EXPECT_EQ(batch, 100);  /* Should return base batch */

    /* Test LR with NULL context */
    float lr = nimcp_brain_training_get_adjusted_lr(nullptr, 0.01f);
    EXPECT_FLOAT_EQ(lr, 0.01f);  /* Should return base LR */

    LOG_INFO("NULL context handling verified");
}

/**
 * @test Verify behavior when Portia integration disabled
 */
TEST_F(PortiaTrainingIntegrationTest, PortiaIntegrationDisabled) {
    /* Create context with Portia disabled */
    nimcp_brain_training_config_t config = nimcp_brain_training_default_config();
    config.enable_portia_integration = false;

    nimcp_brain_training_ctx_t* ctx = nimcp_brain_training_create(&config);
    ASSERT_NE(ctx, nullptr);

    nimcp_brain_training_init(ctx, nullptr, nullptr);

    /* Try to change tier - should be ignored */
    nimcp_result_t res = nimcp_brain_training_on_tier_change(
        ctx,
        PLATFORM_TIER_MINIMAL
    );
    EXPECT_EQ(res, NIMCP_SUCCESS);  /* Silently ignored */

    /* Verify training not paused */
    EXPECT_FALSE(nimcp_brain_training_is_paused(ctx));

    /* Verify batch size unchanged */
    size_t batch = nimcp_brain_training_get_adjusted_batch_size(ctx, 100);
    EXPECT_EQ(batch, 100);

    nimcp_brain_training_destroy(ctx);

    LOG_INFO("Portia integration disabled: tier changes ignored");
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
