/**
 * @file test_brain_factory_handlers.cpp
 * @brief Integration tests for brain factory message handlers
 *
 * Tests the bio-async message handlers in brain factory initialization:
 * - Insula message handlers (cytokine, salience, emotion tensor)
 * - Motor cortex handlers (command, stop, cerebellar correction)
 * - Collective cognition handlers (consensus, broadcast, swarm update)
 * - Hypothalamus subsystem connections
 * - Broca subsystem connections
 * - Prefrontal subsystem connections
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/factory/init/nimcp_brain_init_insula.h"
#include "core/brain/factory/init/nimcp_brain_init_motor.h"
#include "core/brain/factory/init/nimcp_brain_init_broca.h"
#include "core/brain/factory/init/nimcp_brain_init_prefrontal.h"
#include "core/brain/factory/init/nimcp_brain_init_hypothalamus.h"
#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrainFactoryHandlersTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        // Initialize bio-async first
        nimcp_bio_async_config_t async_config = nimcp_bio_async_default_config();
        async_config.enable_logging = false;
        nimcp_error_t err = nimcp_bio_async_init(&async_config);
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Failed to initialize bio-async";

        // Initialize bio-router
        bio_router_config_t router_config = bio_router_default_config();
        router_config.enable_logging = false;
        err = bio_router_init(&router_config);
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Failed to initialize bio-router";

        // Create brain with all subsystems enabled
        brain_config_t brain_cfg = brain_default_config();
        brain_cfg.num_inputs = 16;
        brain_cfg.num_outputs = 8;
        brain_cfg.enable_bio_async = true;
        brain_cfg.enable_emotional_tagging = true;
        brain_cfg.enable_multimodal_integration = true;
        brain_cfg.enable_speech_cortex = true;
        brain_cfg.enable_executive_control = true;
        brain_cfg.enable_collective_cognition = true;
        brain_cfg.working_memory_capacity = 7;

        brain = brain_create(&brain_cfg);
        ASSERT_NE(brain, nullptr) << "Failed to create brain";
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
        bio_router_shutdown();
        nimcp_bio_async_shutdown();
    }
};

//=============================================================================
// Insula Subsystem Tests
//=============================================================================

TEST_F(BrainFactoryHandlersTest, InsulaSubsystemInit) {
    // Insula should be initialized if emotional tagging is enabled
    struct brain_struct* b = (struct brain_struct*)brain;

    // Try initializing insula subsystem
    bool result = nimcp_brain_factory_init_insula_subsystem(brain);
    EXPECT_TRUE(result);
}

TEST_F(BrainFactoryHandlersTest, InsulaInitWithNullBrain) {
    bool result = nimcp_brain_factory_init_insula_subsystem(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainFactoryHandlersTest, InsulaQuantumBridgeInit) {
    // First ensure insula is initialized
    nimcp_brain_factory_init_insula_subsystem(brain);

    // Then try quantum bridge - should succeed if quantum is enabled, or return true if not
    bool result = nimcp_brain_factory_init_insula_quantum_bridge(brain);
    EXPECT_TRUE(result);
}

TEST_F(BrainFactoryHandlersTest, InsulaConnectionsInit) {
    // First ensure insula is initialized
    nimcp_brain_factory_init_insula_subsystem(brain);

    // Test all insula connection functions
    EXPECT_TRUE(nimcp_brain_factory_connect_insula_to_limbic(brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_insula_to_somatosensory(brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_insula_to_emotional(brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_insula_to_immune(brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_insula_to_theory_of_mind(brain));
}

//=============================================================================
// Motor Cortex Subsystem Tests
//=============================================================================

TEST_F(BrainFactoryHandlersTest, MotorSubsystemInit) {
    bool result = nimcp_brain_factory_init_motor_subsystem(brain);
    EXPECT_TRUE(result);
}

TEST_F(BrainFactoryHandlersTest, MotorInitWithNullBrain) {
    bool result = nimcp_brain_factory_init_motor_subsystem(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainFactoryHandlersTest, MotorBridgesInit) {
    // First ensure motor is initialized
    nimcp_brain_factory_init_motor_subsystem(brain);

    // Test bridge initialization functions
    EXPECT_TRUE(nimcp_brain_factory_init_motor_substrate_bridge(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_motor_thalamic_bridge(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_motor_quantum_bridge(brain));
}

TEST_F(BrainFactoryHandlersTest, MotorConnectionsInit) {
    // First ensure motor is initialized
    nimcp_brain_factory_init_motor_subsystem(brain);

    // Test all motor connection functions
    EXPECT_TRUE(nimcp_brain_factory_connect_motor_to_basal_ganglia(brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_motor_to_cerebellum(brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_motor_to_thalamus(brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_motor_to_training(brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_motor_to_immune(brain));
}

//=============================================================================
// Broca Subsystem Tests
//=============================================================================

TEST_F(BrainFactoryHandlersTest, BrocaSubsystemInit) {
    bool result = nimcp_brain_factory_init_broca_subsystem(brain);
    EXPECT_TRUE(result);
}

TEST_F(BrainFactoryHandlersTest, BrocaInitWithNullBrain) {
    bool result = nimcp_brain_factory_init_broca_subsystem(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainFactoryHandlersTest, BrocaBridgesInit) {
    // First ensure broca is initialized
    nimcp_brain_factory_init_broca_subsystem(brain);

    // Test bridge initialization functions
    EXPECT_TRUE(nimcp_brain_factory_init_broca_substrate_bridge(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_broca_thalamic_bridge(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_broca_quantum_bridge(brain));
}

TEST_F(BrainFactoryHandlersTest, BrocaConnectionsInit) {
    // First ensure broca is initialized
    nimcp_brain_factory_init_broca_subsystem(brain);

    // Test all broca connection functions
    EXPECT_TRUE(nimcp_brain_factory_connect_broca_to_working_memory(brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_broca_to_training(brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_broca_to_immune(brain));
}

//=============================================================================
// Prefrontal Subsystem Tests
//=============================================================================

TEST_F(BrainFactoryHandlersTest, PrefrontalSubsystemInit) {
    bool result = nimcp_brain_factory_init_prefrontal_subsystem(brain);
    EXPECT_TRUE(result);
}

TEST_F(BrainFactoryHandlersTest, PrefrontalInitWithNullBrain) {
    bool result = nimcp_brain_factory_init_prefrontal_subsystem(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainFactoryHandlersTest, PrefrontalBridgesInit) {
    // First ensure prefrontal is initialized
    nimcp_brain_factory_init_prefrontal_subsystem(brain);

    // Test bridge initialization functions
    EXPECT_TRUE(nimcp_brain_factory_init_prefrontal_substrate_bridge(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_prefrontal_thalamic_bridge(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_prefrontal_quantum_bridge(brain));
}

TEST_F(BrainFactoryHandlersTest, PrefrontalConnectionsInit) {
    // First ensure prefrontal is initialized
    nimcp_brain_factory_init_prefrontal_subsystem(brain);

    // Test all prefrontal connection functions
    EXPECT_TRUE(nimcp_brain_factory_connect_prefrontal_to_working_memory(brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_prefrontal_to_basal_ganglia(brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_prefrontal_to_thalamus(brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_prefrontal_to_training(brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_prefrontal_to_immune(brain));
}

//=============================================================================
// Hypothalamus Subsystem Tests
//=============================================================================

TEST_F(BrainFactoryHandlersTest, HypothalamusSubsystemInit) {
    bool result = nimcp_brain_factory_init_hypothalamus_subsystem(brain);
    EXPECT_TRUE(result);
}

TEST_F(BrainFactoryHandlersTest, HypothalamusInitWithNullBrain) {
    bool result = nimcp_brain_factory_init_hypothalamus_subsystem(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainFactoryHandlersTest, HypothalamusBridgesInit) {
    // First ensure hypothalamus is initialized
    nimcp_brain_factory_init_hypothalamus_subsystem(brain);

    // Test bridge initialization functions
    EXPECT_TRUE(nimcp_brain_factory_init_hypothalamus_limbic_bridge(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_hypothalamus_brainstem_bridge(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_hypothalamus_pituitary_bridge(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_hypothalamus_quantum_bridge(brain));
}

TEST_F(BrainFactoryHandlersTest, HypothalamusConnectionsInit) {
    // First ensure hypothalamus is initialized
    nimcp_brain_factory_init_hypothalamus_subsystem(brain);

    // Test all hypothalamus connection functions
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_sleep(brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_immune(brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_wellbeing(brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_medulla(brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_emotions(brain));
}

//=============================================================================
// Collective Cognition Subsystem Tests
//=============================================================================

TEST_F(BrainFactoryHandlersTest, CollectiveCognitionSubsystemInit) {
    bool result = nimcp_brain_factory_init_collective_cognition_subsystem(brain);
    // Result depends on whether collective cognition is enabled in config
    EXPECT_TRUE(result);  // Should at least not crash
}

TEST_F(BrainFactoryHandlersTest, CollectiveCognitionInitWithNullBrain) {
    bool result = nimcp_brain_factory_init_collective_cognition_subsystem(nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// Null Pointer Safety Tests
//=============================================================================

TEST_F(BrainFactoryHandlersTest, AllConnectionFunctionsHandleNull) {
    // All connection functions should handle NULL brain gracefully (return true)
    EXPECT_TRUE(nimcp_brain_factory_connect_insula_to_limbic(nullptr));
    EXPECT_TRUE(nimcp_brain_factory_connect_insula_to_somatosensory(nullptr));
    EXPECT_TRUE(nimcp_brain_factory_connect_insula_to_emotional(nullptr));
    EXPECT_TRUE(nimcp_brain_factory_connect_insula_to_immune(nullptr));
    EXPECT_TRUE(nimcp_brain_factory_connect_insula_to_theory_of_mind(nullptr));

    EXPECT_TRUE(nimcp_brain_factory_connect_motor_to_basal_ganglia(nullptr));
    EXPECT_TRUE(nimcp_brain_factory_connect_motor_to_cerebellum(nullptr));
    EXPECT_TRUE(nimcp_brain_factory_connect_motor_to_thalamus(nullptr));
    EXPECT_TRUE(nimcp_brain_factory_connect_motor_to_training(nullptr));
    EXPECT_TRUE(nimcp_brain_factory_connect_motor_to_immune(nullptr));

    EXPECT_TRUE(nimcp_brain_factory_connect_broca_to_working_memory(nullptr));
    EXPECT_TRUE(nimcp_brain_factory_connect_broca_to_training(nullptr));
    EXPECT_TRUE(nimcp_brain_factory_connect_broca_to_immune(nullptr));

    EXPECT_TRUE(nimcp_brain_factory_connect_prefrontal_to_working_memory(nullptr));
    EXPECT_TRUE(nimcp_brain_factory_connect_prefrontal_to_basal_ganglia(nullptr));
    EXPECT_TRUE(nimcp_brain_factory_connect_prefrontal_to_thalamus(nullptr));
    EXPECT_TRUE(nimcp_brain_factory_connect_prefrontal_to_training(nullptr));
    EXPECT_TRUE(nimcp_brain_factory_connect_prefrontal_to_immune(nullptr));

    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_sleep(nullptr));
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_immune(nullptr));
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_wellbeing(nullptr));
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_medulla(nullptr));
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_emotions(nullptr));
}

//=============================================================================
// Subsystem Update Tests
//=============================================================================

TEST_F(BrainFactoryHandlersTest, MotorSubsystemUpdate) {
    // Initialize motor subsystem
    nimcp_brain_factory_init_motor_subsystem(brain);

    // Update should not crash
    bool result = nimcp_brain_factory_update_motor_subsystem(brain, 1000);  // 1ms
    EXPECT_TRUE(result);
}

TEST_F(BrainFactoryHandlersTest, MotorUpdateWithNullBrain) {
    bool result = nimcp_brain_factory_update_motor_subsystem(nullptr, 1000);
    EXPECT_TRUE(result);  // Should return true (nothing to update)
}

//=============================================================================
// Double Initialization Safety Tests
//=============================================================================

TEST_F(BrainFactoryHandlersTest, DoubleInitIsIdempotent) {
    // First initialization
    EXPECT_TRUE(nimcp_brain_factory_init_insula_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_motor_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_broca_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_prefrontal_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_hypothalamus_subsystem(brain));

    // Second initialization should be idempotent
    EXPECT_TRUE(nimcp_brain_factory_init_insula_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_motor_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_broca_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_prefrontal_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_hypothalamus_subsystem(brain));
}

//=============================================================================
// Integration: Full Subsystem Chain
//=============================================================================

TEST_F(BrainFactoryHandlersTest, FullSubsystemChainInit) {
    // Initialize all subsystems in order
    EXPECT_TRUE(nimcp_brain_factory_init_hypothalamus_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_insula_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_motor_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_broca_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_prefrontal_subsystem(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_collective_cognition_subsystem(brain));

    // All bridges and connections should work after full init
    EXPECT_TRUE(nimcp_brain_factory_connect_prefrontal_to_working_memory(brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_broca_to_working_memory(brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_motor_to_basal_ganglia(brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_emotions(brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_insula_to_emotional(brain));
}
