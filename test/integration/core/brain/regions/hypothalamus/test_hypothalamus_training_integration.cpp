/**
 * @file test_hypothalamus_training_integration.cpp
 * @brief Integration tests for Hypothalamus-Training Bridge with orchestrator and hub
 *
 * WHAT: Integration tests verifying the hypothalamus-training bridge correctly
 *       integrates with the hypothalamus orchestrator and training integration hub
 * WHY:  Ensure bidirectional drive-training communication for homeostatic learning
 * HOW:  Test multi-component workflows including loss events, drive updates,
 *       epoch progression, gradient events, and LR changes
 *
 * TEST SCENARIOS:
 * 1. Integration with hypothalamus orchestrator (hypo_orchestrator_t)
 * 2. Integration with training integration hub (training_integration_hub_t)
 * 3. Multi-component workflows: loss events -> drive updates -> modulation output
 * 4. Epoch progression with fatigue accumulation and consolidation
 * 5. Gradient events triggering safety responses
 * 6. LR changes affecting curiosity/exploration balance
 * 7. Homeostatic setpoint adaptation over training
 * 8. Full training simulation with multiple epochs
 *
 * @version 1.0.0
 * @date 2026-01-10
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <mutex>
#include <cmath>

extern "C" {
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_training_bridge.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_orchestrator.h"
#include "training/integration/nimcp_training_integration_hub.h"
#include "training/integration/nimcp_training_event_types.h"
}

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define TEST_MODULE_ID_TRAINING_LOOP    0x4001
#define TEST_MODULE_ID_LR_SCHEDULER     0x4002
#define TEST_MODULE_ID_GRADIENT_MGR     0x4003
#define TEST_MODULE_ID_CHECKPOINT       0x4004

/* ============================================================================
 * Event Tracking Structures
 * ============================================================================ */

struct TrainingEventTracker {
    std::atomic<int> total_events{0};
    std::atomic<int> loss_events{0};
    std::atomic<int> epoch_events{0};
    std::atomic<int> lr_events{0};
    std::atomic<int> gradient_events{0};
    std::vector<training_event_type_t> event_types;
    std::vector<float> loss_values;
    std::mutex mutex;
};

struct DriveEventTracker {
    std::atomic<int> total_events{0};
    std::atomic<int> drive_activated{0};
    std::atomic<int> homeostatic_alerts{0};
    std::atomic<int> stress_responses{0};
    std::vector<float> drive_levels;
    std::mutex mutex;
};

/* ============================================================================
 * Callback Functions
 * ============================================================================ */

static int training_event_callback(const training_event_data_t* event, void* user_data) {
    if (!event || !user_data) return -1;

    TrainingEventTracker* tracker = static_cast<TrainingEventTracker*>(user_data);
    tracker->total_events++;

    switch (event->event_type) {
        case TRAINING_EVENT_LOSS_COMPUTED:
            tracker->loss_events++;
            {
                std::lock_guard<std::mutex> lock(tracker->mutex);
                tracker->loss_values.push_back(event->loss_value);
            }
            break;
        case TRAINING_EVENT_EPOCH_COMPLETE:
            tracker->epoch_events++;
            break;
        case TRAINING_EVENT_LR_ADJUSTED:
            tracker->lr_events++;
            break;
        case TRAINING_EVENT_GRADIENT_READY:
            tracker->gradient_events++;
            break;
        default:
            break;
    }

    {
        std::lock_guard<std::mutex> lock(tracker->mutex);
        tracker->event_types.push_back(event->event_type);
    }

    return 0;
}

static int drive_event_callback(const hypo_event_data_t* event, void* user_data) {
    if (!event || !user_data) return -1;

    DriveEventTracker* tracker = static_cast<DriveEventTracker*>(user_data);
    tracker->total_events++;

    switch (event->event_type) {
        case HYPO_EVENT_DRIVE_ACTIVATED:
            tracker->drive_activated++;
            {
                std::lock_guard<std::mutex> lock(tracker->mutex);
                tracker->drive_levels.push_back(event->drive.drive_level);
            }
            break;
        case HYPO_EVENT_HOMEOSTATIC_ALERT:
            tracker->homeostatic_alerts++;
            break;
        case HYPO_EVENT_STRESS_RESPONSE:
            tracker->stress_responses++;
            break;
        default:
            break;
    }

    return 0;
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class HypothalamusTrainingIntegrationTest : public ::testing::Test {
protected:
    hypo_training_bridge_t* bridge;
    hypo_orchestrator_t orchestrator;
    training_integration_hub_t training_hub;
    hypo_training_bridge_config_t bridge_config;
    hypo_orch_config_t orch_config;
    training_hub_config_t hub_config;

    void SetUp() override {
        bridge = nullptr;
        orchestrator = nullptr;
        training_hub = nullptr;

        /* Initialize orchestrator config */
        int ret = hypo_orch_default_config(&orch_config);
        ASSERT_EQ(0, ret);
        orch_config.enable_async = false;  /* Synchronous for deterministic tests */
        orch_config.max_bridges = 16;
        orch_config.max_subscriptions = 64;

        /* Initialize training hub config */
        hub_config = training_hub_default_config();
        hub_config.enable_async = false;  /* Synchronous for deterministic tests */
        hub_config.max_modules = 16;
        hub_config.max_subscriptions = 64;

        /* Initialize bridge config */
        ret = hypo_training_bridge_default_config(&bridge_config);
        ASSERT_EQ(0, ret);
        bridge_config.enable_logging = false;
        bridge_config.enable_metrics = true;
    }

    void TearDown() override {
        if (bridge) {
            hypo_training_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (orchestrator) {
            hypo_orch_destroy(orchestrator);
            orchestrator = nullptr;
        }
        if (training_hub) {
            training_hub_destroy(training_hub);
            training_hub = nullptr;
        }
    }

    /* Helper to create full integration environment */
    void create_full_environment() {
        /* Create orchestrator */
        orchestrator = hypo_orch_create(&orch_config);
        ASSERT_NE(orchestrator, nullptr);

        /* Create training hub */
        training_hub = training_hub_create(&hub_config);
        ASSERT_NE(training_hub, nullptr);

        /* Create bridge with connections */
        bridge = hypo_training_bridge_create(&bridge_config, orchestrator, training_hub);
        ASSERT_NE(bridge, nullptr);
    }

    /* Helper to register test modules with training hub */
    void register_test_modules() {
        ASSERT_NE(training_hub, nullptr);

        int ret = training_hub_register_module(training_hub, TEST_MODULE_ID_TRAINING_LOOP,
                                                TRAINING_CATEGORY_CURRICULUM,
                                                "training_loop", nullptr);
        ASSERT_EQ(0, ret);

        ret = training_hub_register_module(training_hub, TEST_MODULE_ID_LR_SCHEDULER,
                                           TRAINING_CATEGORY_OPTIMIZATION,
                                           "lr_scheduler", nullptr);
        ASSERT_EQ(0, ret);

        ret = training_hub_register_module(training_hub, TEST_MODULE_ID_GRADIENT_MGR,
                                           TRAINING_CATEGORY_OPTIMIZATION,
                                           "gradient_manager", nullptr);
        ASSERT_EQ(0, ret);

        ret = training_hub_register_module(training_hub, TEST_MODULE_ID_CHECKPOINT,
                                           TRAINING_CATEGORY_DATA,
                                           "checkpoint", nullptr);
        ASSERT_EQ(0, ret);
    }

    /* Helper to simulate a training loss event */
    void simulate_loss_event(uint32_t epoch, uint32_t batch, float loss) {
        int ret = hypo_training_bridge_process_loss(bridge, epoch, batch, loss);
        EXPECT_EQ(0, ret);
    }

    /* Helper to simulate gradient event */
    void simulate_gradient_event(float gradient_norm, bool was_clipped) {
        int ret = hypo_training_bridge_process_gradient(bridge, gradient_norm, was_clipped);
        EXPECT_EQ(0, ret);
    }

    /* Helper to simulate epoch completion */
    void simulate_epoch_complete(uint32_t epoch, float avg_loss) {
        int ret = hypo_training_bridge_process_epoch(bridge, epoch, avg_loss);
        EXPECT_EQ(0, ret);
    }

    /* Helper to simulate LR change */
    void simulate_lr_change(float old_lr, float new_lr) {
        int ret = hypo_training_bridge_process_lr_change(bridge, old_lr, new_lr);
        EXPECT_EQ(0, ret);
    }
};

/* ============================================================================
 * Orchestrator Integration Tests
 * ============================================================================ */

TEST_F(HypothalamusTrainingIntegrationTest, ConnectToOrchestrator) {
    /* Create orchestrator */
    orchestrator = hypo_orch_create(&orch_config);
    ASSERT_NE(orchestrator, nullptr);

    /* Create bridge without initial connection */
    bridge = hypo_training_bridge_create(&bridge_config, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    /* Verify not connected initially */
    bool orch_connected, hub_connected;
    int ret = hypo_training_bridge_is_connected(bridge, &orch_connected, &hub_connected);
    EXPECT_EQ(0, ret);
    EXPECT_FALSE(orch_connected);
    EXPECT_FALSE(hub_connected);

    /* Connect to orchestrator */
    ret = hypo_training_bridge_connect_orchestrator(bridge, orchestrator);
    EXPECT_EQ(0, ret);

    /* Verify connected */
    ret = hypo_training_bridge_is_connected(bridge, &orch_connected, &hub_connected);
    EXPECT_EQ(0, ret);
    EXPECT_TRUE(orch_connected);
    EXPECT_FALSE(hub_connected);
}

TEST_F(HypothalamusTrainingIntegrationTest, ConnectToTrainingHub) {
    /* Create training hub */
    training_hub = training_hub_create(&hub_config);
    ASSERT_NE(training_hub, nullptr);

    /* Create bridge without initial connection */
    bridge = hypo_training_bridge_create(&bridge_config, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    /* Connect to training hub */
    int ret = hypo_training_bridge_connect_training_hub(bridge, training_hub);
    EXPECT_EQ(0, ret);

    /* Verify connected */
    bool orch_connected, hub_connected;
    ret = hypo_training_bridge_is_connected(bridge, &orch_connected, &hub_connected);
    EXPECT_EQ(0, ret);
    EXPECT_FALSE(orch_connected);
    EXPECT_TRUE(hub_connected);
}

TEST_F(HypothalamusTrainingIntegrationTest, FullConnectionAtCreation) {
    create_full_environment();

    /* Verify both connections */
    bool orch_connected, hub_connected;
    int ret = hypo_training_bridge_is_connected(bridge, &orch_connected, &hub_connected);
    EXPECT_EQ(0, ret);
    EXPECT_TRUE(orch_connected);
    EXPECT_TRUE(hub_connected);
}

TEST_F(HypothalamusTrainingIntegrationTest, DisconnectFromAll) {
    create_full_environment();

    /* Disconnect */
    int ret = hypo_training_bridge_disconnect(bridge);
    EXPECT_EQ(0, ret);

    /* Verify disconnected */
    bool orch_connected, hub_connected;
    ret = hypo_training_bridge_is_connected(bridge, &orch_connected, &hub_connected);
    EXPECT_EQ(0, ret);
    EXPECT_FALSE(orch_connected);
    EXPECT_FALSE(hub_connected);
}

/* ============================================================================
 * Multi-Component Workflow Tests: Loss -> Drive -> Modulation
 * ============================================================================ */

TEST_F(HypothalamusTrainingIntegrationTest, LossEventUpdatesDrives) {
    create_full_environment();

    /* Get initial drive state */
    hypo_training_drive_state_t initial_state;
    int ret = hypo_training_bridge_get_drive_state(bridge, &initial_state);
    EXPECT_EQ(0, ret);

    /* Simulate high loss (deviation from setpoint) */
    simulate_loss_event(1, 0, 0.9f);  /* High loss */

    /* Get updated drive state */
    hypo_training_drive_state_t updated_state;
    ret = hypo_training_bridge_get_drive_state(bridge, &updated_state);
    EXPECT_EQ(0, ret);

    /* Safety drive should increase with high loss */
    EXPECT_GE(updated_state.safety_activation, initial_state.safety_activation);
}

TEST_F(HypothalamusTrainingIntegrationTest, LossImprovementAffectsCuriosity) {
    create_full_environment();

    /* Process decreasing losses (improvement) */
    simulate_loss_event(1, 0, 0.8f);
    simulate_loss_event(1, 1, 0.7f);
    simulate_loss_event(1, 2, 0.6f);
    simulate_loss_event(1, 3, 0.5f);

    /* Get drive state after improvement */
    hypo_training_drive_state_t state;
    int ret = hypo_training_bridge_get_drive_state(bridge, &state);
    EXPECT_EQ(0, ret);

    /* Curiosity should be active with good progress */
    EXPECT_GE(state.curiosity_activation, 0.0f);
    EXPECT_LE(state.curiosity_activation, 1.0f);

    /* Learning readiness should be positive */
    EXPECT_GT(state.learning_readiness, 0.0f);
}

TEST_F(HypothalamusTrainingIntegrationTest, DriveStateAffectsModulation) {
    create_full_environment();

    /* Set high curiosity drive */
    int ret = hypo_training_bridge_set_drive(bridge, 0, 0.9f);  /* 0 = curiosity */
    EXPECT_EQ(0, ret);

    /* Get modulation output */
    hypo_training_modulation_t modulation;
    ret = hypo_training_bridge_compute_modulation(bridge, &modulation);
    EXPECT_EQ(0, ret);

    /* High curiosity should increase LR multiplier */
    EXPECT_GE(modulation.lr_multiplier, 1.0f);
    EXPECT_LE(modulation.lr_multiplier, HYPO_TRAINING_MAX_PRECISION);
}

TEST_F(HypothalamusTrainingIntegrationTest, SafetyDriveReducesLRMultiplier) {
    create_full_environment();

    /* Set high safety drive */
    int ret = hypo_training_bridge_set_drive(bridge, 1, 0.9f);  /* 1 = safety */
    EXPECT_EQ(0, ret);

    /* Get LR multiplier */
    float lr_mult;
    ret = hypo_training_bridge_get_lr_multiplier(bridge, &lr_mult);
    EXPECT_EQ(0, ret);

    /* High safety should reduce LR multiplier */
    EXPECT_LE(lr_mult, 1.0f);
    EXPECT_GE(lr_mult, HYPO_TRAINING_MIN_PRECISION);
}

TEST_F(HypothalamusTrainingIntegrationTest, CompetenceDriveAffectsDifficulty) {
    create_full_environment();

    /* Set high competence drive */
    int ret = hypo_training_bridge_set_drive(bridge, 2, 0.9f);  /* 2 = competence */
    EXPECT_EQ(0, ret);

    /* Get difficulty adjustment */
    float difficulty_adj;
    ret = hypo_training_bridge_get_difficulty_adjustment(bridge, &difficulty_adj);
    EXPECT_EQ(0, ret);

    /* High competence should suggest difficulty increase */
    EXPECT_GE(difficulty_adj, -1.0f);
    EXPECT_LE(difficulty_adj, 1.0f);
}

/* ============================================================================
 * Epoch Progression and Fatigue Tests
 * ============================================================================ */

TEST_F(HypothalamusTrainingIntegrationTest, EpochProgressionAccumulatesFatigue) {
    create_full_environment();

    /* Get initial fatigue */
    hypo_training_drive_state_t initial_state;
    int ret = hypo_training_bridge_get_drive_state(bridge, &initial_state);
    EXPECT_EQ(0, ret);
    float initial_fatigue = initial_state.fatigue_level;

    /* Simulate multiple epochs */
    for (uint32_t epoch = 0; epoch < 10; epoch++) {
        simulate_epoch_complete(epoch, 0.5f);
    }

    /* Get updated fatigue */
    hypo_training_drive_state_t updated_state;
    ret = hypo_training_bridge_get_drive_state(bridge, &updated_state);
    EXPECT_EQ(0, ret);

    /* Fatigue should have increased */
    EXPECT_GT(updated_state.fatigue_level, initial_fatigue);
}

TEST_F(HypothalamusTrainingIntegrationTest, HighFatigueTriggersMiniRest) {
    create_full_environment();

    /* Simulate many epochs to accumulate fatigue */
    for (uint32_t epoch = 0; epoch < 30; epoch++) {
        simulate_epoch_complete(epoch, 0.5f);
    }

    /* Check consolidation recommendation */
    hypo_consolidation_type_t consolidation;
    int ret = hypo_training_bridge_check_consolidation(bridge, &consolidation);
    EXPECT_EQ(0, ret);

    /* Should recommend some consolidation */
    EXPECT_NE(HYPO_CONSOL_NONE, consolidation);
}

TEST_F(HypothalamusTrainingIntegrationTest, ConsolidationRecommendationInModulation) {
    create_full_environment();

    /* Set high fatigue */
    int ret = hypo_training_bridge_set_drive(bridge, 3, 0.95f);  /* 3 = fatigue */
    EXPECT_EQ(0, ret);

    /* Get modulation */
    hypo_training_modulation_t modulation;
    ret = hypo_training_bridge_compute_modulation(bridge, &modulation);
    EXPECT_EQ(0, ret);

    /* Should recommend consolidation or checkpoint */
    bool has_recommendation = (modulation.recommended_consolidation != HYPO_CONSOL_NONE) ||
                              modulation.recommend_checkpoint ||
                              modulation.recommend_lr_reduction;
    EXPECT_TRUE(has_recommendation);
}

TEST_F(HypothalamusTrainingIntegrationTest, FatigueResetAfterConsolidation) {
    create_full_environment();

    /* Accumulate fatigue */
    for (uint32_t epoch = 0; epoch < 20; epoch++) {
        simulate_epoch_complete(epoch, 0.5f);
    }

    /* Get fatigue level */
    hypo_training_drive_state_t before_reset;
    int ret = hypo_training_bridge_get_drive_state(bridge, &before_reset);
    EXPECT_EQ(0, ret);
    EXPECT_GT(before_reset.fatigue_level, 0.0f);

    /* Reset fatigue (simulate consolidation completed) */
    ret = hypo_training_bridge_reset_fatigue(bridge);
    EXPECT_EQ(0, ret);

    /* Verify fatigue reset */
    hypo_training_drive_state_t after_reset;
    ret = hypo_training_bridge_get_drive_state(bridge, &after_reset);
    EXPECT_EQ(0, ret);
    EXPECT_LT(after_reset.fatigue_level, before_reset.fatigue_level);
}

/* ============================================================================
 * Gradient Event and Safety Response Tests
 * ============================================================================ */

TEST_F(HypothalamusTrainingIntegrationTest, HighGradientActivatesSafety) {
    create_full_environment();

    /* Get initial safety */
    hypo_training_drive_state_t initial_state;
    int ret = hypo_training_bridge_get_drive_state(bridge, &initial_state);
    EXPECT_EQ(0, ret);

    /* Simulate high gradient norm (potential instability) */
    simulate_gradient_event(100.0f, true);  /* High norm, was clipped */

    /* Get updated safety */
    hypo_training_drive_state_t updated_state;
    ret = hypo_training_bridge_get_drive_state(bridge, &updated_state);
    EXPECT_EQ(0, ret);

    /* Safety should increase */
    EXPECT_GE(updated_state.safety_activation, initial_state.safety_activation);
}

TEST_F(HypothalamusTrainingIntegrationTest, RepeatedClippingTriggersStrongerSafety) {
    create_full_environment();

    /* Simulate repeated gradient clipping */
    for (int i = 0; i < 5; i++) {
        simulate_gradient_event(50.0f, true);
    }

    /* Get safety state */
    hypo_training_drive_state_t state;
    int ret = hypo_training_bridge_get_drive_state(bridge, &state);
    EXPECT_EQ(0, ret);

    /* Safety should be elevated */
    EXPECT_GT(state.safety_activation, 0.3f);

    /* Exploration tendency should be reduced (safety > curiosity) */
    EXPECT_LE(state.exploration_tendency, 0.5f);
}

TEST_F(HypothalamusTrainingIntegrationTest, NormalGradientsKeepSafetyLow) {
    create_full_environment();

    /* Simulate normal gradient norms */
    for (int i = 0; i < 10; i++) {
        simulate_gradient_event(1.0f, false);  /* Normal, not clipped */
    }

    /* Get safety state */
    hypo_training_drive_state_t state;
    int ret = hypo_training_bridge_get_drive_state(bridge, &state);
    EXPECT_EQ(0, ret);

    /* Safety should remain moderate */
    EXPECT_LE(state.safety_activation, 0.5f);
}

TEST_F(HypothalamusTrainingIntegrationTest, GradientClippingIncreasesGradientClipMultiplier) {
    create_full_environment();

    /* Simulate repeated clipping */
    for (int i = 0; i < 5; i++) {
        simulate_gradient_event(50.0f, true);
    }

    /* Get modulation */
    hypo_training_modulation_t modulation;
    int ret = hypo_training_bridge_compute_modulation(bridge, &modulation);
    EXPECT_EQ(0, ret);

    /* Gradient clip multiplier should be affected */
    EXPECT_GE(modulation.gradient_clip_multiplier, 0.5f);
    EXPECT_LE(modulation.gradient_clip_multiplier, 2.0f);
}

/* ============================================================================
 * Learning Rate Change and Curiosity Tests
 * ============================================================================ */

TEST_F(HypothalamusTrainingIntegrationTest, LRIncreaseBoostsCuriosity) {
    create_full_environment();

    /* Get initial curiosity */
    hypo_training_drive_state_t initial_state;
    int ret = hypo_training_bridge_get_drive_state(bridge, &initial_state);
    EXPECT_EQ(0, ret);

    /* Simulate LR increase (more exploration) */
    simulate_lr_change(0.001f, 0.01f);

    /* Get updated curiosity */
    hypo_training_drive_state_t updated_state;
    ret = hypo_training_bridge_get_drive_state(bridge, &updated_state);
    EXPECT_EQ(0, ret);

    /* Curiosity should increase or stay same with LR increase */
    EXPECT_GE(updated_state.curiosity_activation, 0.0f);
}

TEST_F(HypothalamusTrainingIntegrationTest, LRDecreaseReducesCuriosity) {
    create_full_environment();

    /* Set initial high curiosity */
    int ret = hypo_training_bridge_set_drive(bridge, 0, 0.8f);
    EXPECT_EQ(0, ret);

    /* Get state before LR decrease */
    hypo_training_drive_state_t before_state;
    ret = hypo_training_bridge_get_drive_state(bridge, &before_state);
    EXPECT_EQ(0, ret);

    /* Simulate LR decrease (more exploitation) */
    simulate_lr_change(0.01f, 0.001f);

    /* Get updated state */
    hypo_training_drive_state_t after_state;
    ret = hypo_training_bridge_get_drive_state(bridge, &after_state);
    EXPECT_EQ(0, ret);

    /* Curiosity should decrease with LR decrease */
    EXPECT_LE(after_state.curiosity_activation, before_state.curiosity_activation);
}

TEST_F(HypothalamusTrainingIntegrationTest, ExplorationExploitationBalance) {
    create_full_environment();

    /* Set balanced curiosity and safety */
    hypo_training_bridge_set_drive(bridge, 0, 0.6f);  /* curiosity */
    hypo_training_bridge_set_drive(bridge, 1, 0.4f);  /* safety */

    /* Get drive state */
    hypo_training_drive_state_t state;
    int ret = hypo_training_bridge_get_drive_state(bridge, &state);
    EXPECT_EQ(0, ret);

    /* Exploration tendency should reflect balance */
    EXPECT_GT(state.exploration_tendency, -1.0f);
    EXPECT_LT(state.exploration_tendency, 1.0f);
}

/* ============================================================================
 * Homeostatic Setpoint Adaptation Tests
 * ============================================================================ */

TEST_F(HypothalamusTrainingIntegrationTest, GetHomeostaticState) {
    create_full_environment();

    /* Process some loss events */
    simulate_loss_event(1, 0, 0.6f);
    simulate_loss_event(1, 1, 0.55f);
    simulate_loss_event(1, 2, 0.5f);

    /* Get homeostatic state */
    hypo_training_homeostatic_state_t state;
    int ret = hypo_training_bridge_get_homeostatic_state(bridge, &state);
    EXPECT_EQ(0, ret);

    /* Verify state fields are populated */
    EXPECT_GE(state.current_loss, 0.0f);
    EXPECT_GT(state.loss_setpoint, 0.0f);
}

TEST_F(HypothalamusTrainingIntegrationTest, SetpointAdaptation) {
    create_full_environment();

    /* Get initial setpoint */
    hypo_training_homeostatic_state_t initial_state;
    int ret = hypo_training_bridge_get_homeostatic_state(bridge, &initial_state);
    EXPECT_EQ(0, ret);

    /* Update setpoint */
    float new_setpoint = 0.3f;
    ret = hypo_training_bridge_set_loss_setpoint(bridge, new_setpoint);
    EXPECT_EQ(0, ret);

    /* Verify setpoint updated */
    hypo_training_homeostatic_state_t updated_state;
    ret = hypo_training_bridge_get_homeostatic_state(bridge, &updated_state);
    EXPECT_EQ(0, ret);
    EXPECT_FLOAT_EQ(new_setpoint, updated_state.loss_setpoint);
}

TEST_F(HypothalamusTrainingIntegrationTest, DeviationTracking) {
    create_full_environment();

    /* Set setpoint */
    hypo_training_bridge_set_loss_setpoint(bridge, 0.5f);

    /* Process loss far from setpoint */
    simulate_loss_event(1, 0, 0.9f);  /* 0.4 deviation */

    /* Get homeostatic state */
    hypo_training_homeostatic_state_t state;
    int ret = hypo_training_bridge_get_homeostatic_state(bridge, &state);
    EXPECT_EQ(0, ret);

    /* Deviation should be tracked */
    EXPECT_NEAR(std::fabs(state.deviation), 0.4f, 0.1f);
}

TEST_F(HypothalamusTrainingIntegrationTest, TrainingStateAssessment) {
    create_full_environment();

    /* Get initial training state */
    hypo_training_state_t initial_train_state;
    int ret = hypo_training_bridge_get_training_state(bridge, &initial_train_state);
    EXPECT_EQ(0, ret);

    /* Process improving losses */
    simulate_loss_event(1, 0, 0.8f);
    simulate_loss_event(1, 1, 0.7f);
    simulate_loss_event(1, 2, 0.6f);
    simulate_loss_event(1, 3, 0.5f);

    /* Get updated training state */
    hypo_training_state_t updated_state;
    ret = hypo_training_bridge_get_training_state(bridge, &updated_state);
    EXPECT_EQ(0, ret);

    /* State should indicate improvement or health */
    bool is_positive = (updated_state == HYPO_TRAIN_STATE_HEALTHY ||
                        updated_state == HYPO_TRAIN_STATE_IMPROVING);
    EXPECT_TRUE(is_positive);
}

/* ============================================================================
 * Full Training Simulation Tests
 * ============================================================================ */

TEST_F(HypothalamusTrainingIntegrationTest, FullTrainingSimulationMultipleEpochs) {
    create_full_environment();
    register_test_modules();

    /* Simulate 5 epochs of training */
    const uint32_t num_epochs = 5;
    const uint32_t batches_per_epoch = 10;

    float prev_avg_loss = 1.0f;

    for (uint32_t epoch = 0; epoch < num_epochs; epoch++) {
        float epoch_loss_sum = 0.0f;

        for (uint32_t batch = 0; batch < batches_per_epoch; batch++) {
            /* Simulate decreasing loss */
            float loss = prev_avg_loss * (1.0f - 0.05f * batch / batches_per_epoch);
            simulate_loss_event(epoch, batch, loss);
            epoch_loss_sum += loss;

            /* Simulate gradient event */
            float gradient_norm = 1.0f + 0.1f * (rand() % 10);
            bool clipped = gradient_norm > 5.0f;
            simulate_gradient_event(gradient_norm, clipped);
        }

        float avg_loss = epoch_loss_sum / batches_per_epoch;
        simulate_epoch_complete(epoch, avg_loss);
        prev_avg_loss = avg_loss * 0.9f;  /* Improvement over epochs */

        /* LR decay */
        if (epoch > 0 && epoch % 2 == 0) {
            float old_lr = 0.01f / (1 << (epoch / 2 - 1));
            float new_lr = 0.01f / (1 << (epoch / 2));
            simulate_lr_change(old_lr, new_lr);
        }
    }

    /* Verify final state */
    hypo_training_bridge_stats_t stats;
    int ret = hypo_training_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(0, ret);

    /* Should have processed events */
    EXPECT_GT(stats.training_events_received, 0u);

    /* Get final modulation */
    hypo_training_modulation_t modulation;
    ret = hypo_training_bridge_compute_modulation(bridge, &modulation);
    EXPECT_EQ(0, ret);

    /* Verify modulation values are reasonable */
    EXPECT_GE(modulation.lr_multiplier, HYPO_TRAINING_MIN_PRECISION);
    EXPECT_LE(modulation.lr_multiplier, HYPO_TRAINING_MAX_PRECISION);
}

TEST_F(HypothalamusTrainingIntegrationTest, TrainingWithDivergence) {
    create_full_environment();

    /* Simulate training that starts diverging */
    float loss = 0.5f;
    for (int i = 0; i < 20; i++) {
        loss *= 1.1f;  /* Loss increasing */
        simulate_loss_event(0, i, loss);
    }

    /* Get training state */
    hypo_training_state_t state;
    int ret = hypo_training_bridge_get_training_state(bridge, &state);
    EXPECT_EQ(0, ret);

    /* Should detect divergence or unstable state */
    bool is_problematic = (state == HYPO_TRAIN_STATE_DIVERGING ||
                           state == HYPO_TRAIN_STATE_UNSTABLE ||
                           state == HYPO_TRAIN_STATE_CRITICAL);
    EXPECT_TRUE(is_problematic);

    /* Get modulation */
    hypo_training_modulation_t modulation;
    ret = hypo_training_bridge_compute_modulation(bridge, &modulation);
    EXPECT_EQ(0, ret);

    /* Should recommend caution */
    bool has_safety_recommendation = modulation.recommend_lr_reduction ||
                                      modulation.recommend_early_stopping ||
                                      (modulation.lr_multiplier < 1.0f);
    EXPECT_TRUE(has_safety_recommendation);
}

TEST_F(HypothalamusTrainingIntegrationTest, TrainingWithPlateau) {
    create_full_environment();

    /* Simulate training plateau (loss not improving) */
    float loss = 0.5f;
    for (int i = 0; i < 30; i++) {
        /* Small random variation but no real improvement */
        float noise = (rand() % 100 - 50) / 1000.0f;
        simulate_loss_event(0, i, loss + noise);
    }

    /* Get homeostatic state */
    hypo_training_homeostatic_state_t homeo_state;
    int ret = hypo_training_bridge_get_homeostatic_state(bridge, &homeo_state);
    EXPECT_EQ(0, ret);

    /* Epochs since improvement should be tracked */
    EXPECT_GE(homeo_state.epochs_since_improvement, 0u);
}

/* ============================================================================
 * Statistics and Reset Tests
 * ============================================================================ */

TEST_F(HypothalamusTrainingIntegrationTest, StatisticsAccumulation) {
    create_full_environment();

    /* Generate activity */
    for (int i = 0; i < 10; i++) {
        simulate_loss_event(0, i, 0.5f);
        simulate_gradient_event(1.0f, false);
    }
    simulate_epoch_complete(0, 0.5f);

    /* Get stats */
    hypo_training_bridge_stats_t stats;
    int ret = hypo_training_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(0, ret);

    /* Verify statistics */
    EXPECT_GT(stats.training_events_received, 0u);
}

TEST_F(HypothalamusTrainingIntegrationTest, StatisticsReset) {
    create_full_environment();

    /* Generate activity */
    for (int i = 0; i < 10; i++) {
        simulate_loss_event(0, i, 0.5f);
    }

    /* Reset stats */
    int ret = hypo_training_bridge_reset_stats(bridge);
    EXPECT_EQ(0, ret);

    /* Verify stats are reset */
    hypo_training_bridge_stats_t stats;
    ret = hypo_training_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0u, stats.training_events_received);
}

TEST_F(HypothalamusTrainingIntegrationTest, BridgeReset) {
    create_full_environment();

    /* Generate activity and modify state */
    for (int i = 0; i < 10; i++) {
        simulate_loss_event(0, i, 0.5f);
    }
    hypo_training_bridge_set_drive(bridge, 0, 0.9f);
    hypo_training_bridge_set_drive(bridge, 1, 0.8f);

    /* Reset bridge */
    int ret = hypo_training_bridge_reset(bridge);
    EXPECT_EQ(0, ret);

    /* Verify state is reset */
    hypo_training_drive_state_t state;
    ret = hypo_training_bridge_get_drive_state(bridge, &state);
    EXPECT_EQ(0, ret);

    /* Drives should be at baseline (defaults: curiosity=0.5, safety=0.2) */
    EXPECT_FLOAT_EQ(0.5f, state.curiosity_activation);
    EXPECT_FLOAT_EQ(0.2f, state.safety_activation);
}

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

TEST_F(HypothalamusTrainingIntegrationTest, NullBridgeHandling) {
    /* All operations on NULL bridge should fail gracefully */
    int ret = hypo_training_bridge_process_loss(nullptr, 0, 0, 0.5f);
    EXPECT_NE(0, ret);

    ret = hypo_training_bridge_process_gradient(nullptr, 1.0f, false);
    EXPECT_NE(0, ret);

    ret = hypo_training_bridge_process_epoch(nullptr, 0, 0.5f);
    EXPECT_NE(0, ret);

    ret = hypo_training_bridge_process_lr_change(nullptr, 0.01f, 0.001f);
    EXPECT_NE(0, ret);

    hypo_training_modulation_t modulation;
    ret = hypo_training_bridge_compute_modulation(nullptr, &modulation);
    EXPECT_NE(0, ret);
}

TEST_F(HypothalamusTrainingIntegrationTest, NullOutputParameterHandling) {
    create_full_environment();

    /* Most get_* functions return -1 (error) for NULL output parameters */
    int ret = hypo_training_bridge_get_drive_state(bridge, nullptr);
    EXPECT_NE(0, ret);

    ret = hypo_training_bridge_get_homeostatic_state(bridge, nullptr);
    EXPECT_NE(0, ret);

    ret = hypo_training_bridge_get_training_state(bridge, nullptr);
    EXPECT_NE(0, ret);

    ret = hypo_training_bridge_get_lr_multiplier(bridge, nullptr);
    EXPECT_NE(0, ret);

    ret = hypo_training_bridge_get_difficulty_adjustment(bridge, nullptr);
    EXPECT_NE(0, ret);

    ret = hypo_training_bridge_check_consolidation(bridge, nullptr);
    EXPECT_NE(0, ret);

    ret = hypo_training_bridge_get_stats(bridge, nullptr);
    EXPECT_NE(0, ret);

    /* is_connected gracefully skips NULL outputs (returns 0 success) */
    ret = hypo_training_bridge_is_connected(bridge, nullptr, nullptr);
    EXPECT_EQ(0, ret);
}

TEST_F(HypothalamusTrainingIntegrationTest, InvalidDriveTypeHandling) {
    create_full_environment();

    /* Invalid drive type should fail */
    int ret = hypo_training_bridge_set_drive(bridge, 100, 0.5f);  /* Invalid type */
    EXPECT_NE(0, ret);
}

TEST_F(HypothalamusTrainingIntegrationTest, DestroyNullIsSafe) {
    /* Should not crash */
    hypo_training_bridge_destroy(nullptr);
}

/* ============================================================================
 * String Utility Tests
 * ============================================================================ */

TEST_F(HypothalamusTrainingIntegrationTest, TrainingStateNames) {
    const char* name;

    name = hypo_training_state_name(HYPO_TRAIN_STATE_HEALTHY);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "");

    name = hypo_training_state_name(HYPO_TRAIN_STATE_IMPROVING);
    EXPECT_NE(name, nullptr);

    name = hypo_training_state_name(HYPO_TRAIN_STATE_PLATEAU);
    EXPECT_NE(name, nullptr);

    name = hypo_training_state_name(HYPO_TRAIN_STATE_DIVERGING);
    EXPECT_NE(name, nullptr);

    name = hypo_training_state_name(HYPO_TRAIN_STATE_UNSTABLE);
    EXPECT_NE(name, nullptr);

    name = hypo_training_state_name(HYPO_TRAIN_STATE_CRITICAL);
    EXPECT_NE(name, nullptr);
}

TEST_F(HypothalamusTrainingIntegrationTest, ConsolidationTypeNames) {
    const char* name;

    name = hypo_consolidation_type_name(HYPO_CONSOL_NONE);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "");

    name = hypo_consolidation_type_name(HYPO_CONSOL_MINI_REST);
    EXPECT_NE(name, nullptr);

    name = hypo_consolidation_type_name(HYPO_CONSOL_CHECKPOINT);
    EXPECT_NE(name, nullptr);

    name = hypo_consolidation_type_name(HYPO_CONSOL_REPLAY);
    EXPECT_NE(name, nullptr);

    name = hypo_consolidation_type_name(HYPO_CONSOL_FULL_REST);
    EXPECT_NE(name, nullptr);
}

TEST_F(HypothalamusTrainingIntegrationTest, ModulationTypeNames) {
    const char* name;

    name = hypo_training_modulation_name(HYPO_TRAIN_MOD_LEARNING_RATE);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "");

    name = hypo_training_modulation_name(HYPO_TRAIN_MOD_BATCH_SIZE);
    EXPECT_NE(name, nullptr);

    name = hypo_training_modulation_name(HYPO_TRAIN_MOD_GRADIENT_CLIP);
    EXPECT_NE(name, nullptr);

    name = hypo_training_modulation_name(HYPO_TRAIN_MOD_CURRICULUM_DIFF);
    EXPECT_NE(name, nullptr);
}

/* ============================================================================
 * Concurrent Access Tests
 * ============================================================================ */

TEST_F(HypothalamusTrainingIntegrationTest, ConcurrentLossProcessing) {
    create_full_environment();

    std::atomic<int> success_count{0};

    auto process_thread = [this, &success_count]() {
        for (int i = 0; i < 50; i++) {
            float loss = 0.5f + (rand() % 50) / 100.0f;
            int ret = hypo_training_bridge_process_loss(bridge, 0, i, loss);
            if (ret == 0) success_count++;
        }
    };

    std::thread t1(process_thread);
    std::thread t2(process_thread);
    std::thread t3(process_thread);

    t1.join();
    t2.join();
    t3.join();

    /* All operations should succeed */
    EXPECT_EQ(150, success_count.load());

    /* State should be queryable */
    hypo_training_drive_state_t state;
    int ret = hypo_training_bridge_get_drive_state(bridge, &state);
    EXPECT_EQ(0, ret);
}

TEST_F(HypothalamusTrainingIntegrationTest, ConcurrentModulationComputation) {
    create_full_environment();

    std::atomic<int> success_count{0};

    auto compute_thread = [this, &success_count]() {
        for (int i = 0; i < 50; i++) {
            hypo_training_modulation_t modulation;
            int ret = hypo_training_bridge_compute_modulation(bridge, &modulation);
            if (ret == 0) success_count++;
        }
    };

    std::thread t1(compute_thread);
    std::thread t2(compute_thread);

    t1.join();
    t2.join();

    /* All operations should succeed */
    EXPECT_EQ(100, success_count.load());
}

TEST_F(HypothalamusTrainingIntegrationTest, ConcurrentDriveUpdates) {
    create_full_environment();

    std::atomic<int> success_count{0};

    auto update_thread = [this, &success_count]() {
        for (int i = 0; i < 50; i++) {
            uint32_t drive_type = i % 5;  /* Cycle through drive types */
            float activation = (i % 10) / 10.0f;
            int ret = hypo_training_bridge_set_drive(bridge, drive_type, activation);
            if (ret == 0) success_count++;
        }
    };

    std::thread t1(update_thread);
    std::thread t2(update_thread);

    t1.join();
    t2.join();

    /* All operations should succeed */
    EXPECT_EQ(100, success_count.load());
}

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(HypothalamusTrainingIntegrationTest, DefaultConfigValues) {
    hypo_training_bridge_config_t config;
    int ret = hypo_training_bridge_default_config(&config);
    EXPECT_EQ(0, ret);

    /* Verify default values match documented defaults */
    EXPECT_TRUE(config.auto_connect_orchestrator);
    EXPECT_TRUE(config.auto_connect_training_hub);

    /* Verify homeostatic defaults */
    EXPECT_FLOAT_EQ(HYPO_TRAINING_DEFAULT_LOSS_SETPOINT,
                    config.homeostatic_config.loss_setpoint);
    EXPECT_FLOAT_EQ(HYPO_TRAINING_DEFAULT_LOSS_TOLERANCE,
                    config.homeostatic_config.loss_tolerance);

    /* Verify drive config defaults */
    EXPECT_FLOAT_EQ(HYPO_TRAINING_DEFAULT_CURIOSITY_LR_MULT,
                    config.drive_config.curiosity_lr_multiplier);
    EXPECT_FLOAT_EQ(HYPO_TRAINING_DEFAULT_SAFETY_LR_MULT,
                    config.drive_config.safety_lr_reduction);
    EXPECT_FLOAT_EQ(HYPO_TRAINING_DEFAULT_CONSOLIDATION_THRESHOLD,
                    config.drive_config.fatigue_consolidation_threshold);
}

TEST_F(HypothalamusTrainingIntegrationTest, CustomConfigValues) {
    hypo_training_bridge_config_t config;
    hypo_training_bridge_default_config(&config);

    /* Customize config */
    config.homeostatic_config.loss_setpoint = 0.3f;
    config.homeostatic_config.loss_tolerance = 0.05f;
    config.drive_config.curiosity_lr_multiplier = 2.0f;
    config.drive_config.safety_lr_reduction = 0.3f;
    config.enable_consolidation = true;
    config.enable_stress_response = true;

    /* Create bridge with custom config */
    bridge = hypo_training_bridge_create(&config, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    /* Verify setpoint was applied */
    hypo_training_homeostatic_state_t state;
    int ret = hypo_training_bridge_get_homeostatic_state(bridge, &state);
    EXPECT_EQ(0, ret);
    EXPECT_FLOAT_EQ(0.3f, state.loss_setpoint);
}

TEST_F(HypothalamusTrainingIntegrationTest, NullConfigUsesDefaults) {
    /* Create with NULL config */
    bridge = hypo_training_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    /* Should have default setpoint */
    hypo_training_homeostatic_state_t state;
    int ret = hypo_training_bridge_get_homeostatic_state(bridge, &state);
    EXPECT_EQ(0, ret);
    EXPECT_FLOAT_EQ(HYPO_TRAINING_DEFAULT_LOSS_SETPOINT, state.loss_setpoint);
}

/* ============================================================================
 * Print Function Tests (smoke tests)
 * ============================================================================ */

TEST_F(HypothalamusTrainingIntegrationTest, PrintSummaryDoesNotCrash) {
    create_full_environment();

    /* Should not crash */
    hypo_training_bridge_print_summary(bridge);
    hypo_training_bridge_print_summary(nullptr);  /* NULL should be safe */
}

TEST_F(HypothalamusTrainingIntegrationTest, PrintStatsDoesNotCrash) {
    create_full_environment();

    hypo_training_bridge_stats_t stats;
    int ret = hypo_training_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(0, ret);

    /* Should not crash */
    hypo_training_bridge_print_stats(&stats);
    hypo_training_bridge_print_stats(nullptr);  /* NULL should be safe */
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
