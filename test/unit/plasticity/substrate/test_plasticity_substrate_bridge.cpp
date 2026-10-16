/**
 * @file test_plasticity_substrate_bridge.cpp
 * @brief Comprehensive unit tests for plasticity-substrate bridge
 *
 * WHAT: Test suite for plasticity-substrate integration bridge
 * WHY:  Verify substrate modulation of plasticity mechanisms (STDP, BCM, etc.)
 * HOW:  Use GoogleTest with biologically-inspired test cases
 *
 * KEY TESTS:
 * - Q10 temperature effects on STDP window (Q10=2.2)
 * - ATP gating of LTP (blocked at <0.3, full at >0.8)
 * - BCM threshold shift with metabolic stress (30-50% higher)
 * - Homeostatic target rate adjustment
 * - Eligibility trace decay modulation by ATP
 * - Dendritic NMDA conductance gating by membrane integrity
 */

#include <gtest/gtest.h>
#include <cmath>
#include <string.h>

// Headers have their own extern "C" guards
#include "plasticity/nimcp_plasticity_substrate_bridge.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "plasticity/stdp/nimcp_stdp.h"
#include "plasticity/bcm/nimcp_bcm.h"
#include "plasticity/homeostatic/nimcp_homeostatic.h"
#include "plasticity/eligibility/nimcp_eligibility_trace.h"
#include "plasticity/dendritic/nimcp_dendritic.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Constants
//=============================================================================

static const float EPSILON = 1e-6f;
static const float NORMAL_TEMP = 37.0f;
static const float NORMAL_ATP = 0.95f;

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * WHAT: Create test neural substrate with default healthy state
 * WHY:  Need substrate instance for bridge creation
 */
static neural_substrate_t* create_test_substrate()
{
    substrate_config_t config;
    substrate_default_config(&config);

    return substrate_create(&config);
}

/**
 * WHAT: Set substrate metabolic state
 * WHY:  Control substrate conditions for testing
 */
static void set_metabolic_state(neural_substrate_t* substrate,
                                float atp, float o2, float glucose)
{
    if (!substrate) return;

    substrate_set_atp(substrate, atp);
    substrate_set_oxygen(substrate, o2);
    substrate_set_glucose(substrate, glucose);
    substrate_update(substrate, 0);
}

/**
 * WHAT: Set substrate physical state
 * WHY:  Control temperature and membrane for testing
 */
static void set_physical_state(neural_substrate_t* substrate,
                               float temp, float membrane, float ion_balance)
{
    if (!substrate) return;

    substrate_set_temperature(substrate, temp);
    substrate_set_membrane_integrity(substrate, membrane);
    substrate_set_ion_balance(substrate, ion_balance);
    substrate_update(substrate, 0);
}

/**
 * WHAT: Check if values are approximately equal
 * WHY:  Floating point comparison with tolerance
 */
static bool float_equals(float a, float b, float epsilon = EPSILON)
{
    return fabs(a - b) < epsilon;
}

//=============================================================================
// Test Fixture
//=============================================================================

class PlasticitySubstrateBridgeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        substrate = create_test_substrate();
        ASSERT_NE(substrate, nullptr) << "Failed to create substrate";

        bridge = nullptr;
    }

    void TearDown() override
    {
        if (bridge) {
            plasticity_substrate_bridge_destroy(bridge);
            bridge = nullptr;
        }

        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }

    neural_substrate_t* substrate;
    plasticity_substrate_bridge_t* bridge;
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

/**
 * WHAT: Test default configuration
 * WHY:  Verify sensible defaults are provided
 */
TEST_F(PlasticitySubstrateBridgeTest, DefaultConfig)
{
    plasticity_substrate_config_t config;

    int result = plasticity_substrate_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_stdp_modulation);
    EXPECT_TRUE(config.enable_bcm_modulation);
    EXPECT_TRUE(config.enable_homeostatic_modulation);
    EXPECT_TRUE(config.enable_eligibility_modulation);
    EXPECT_TRUE(config.enable_dendritic_modulation);
    EXPECT_FALSE(config.enable_bio_async);

    EXPECT_FLOAT_EQ(config.atp_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(config.temperature_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(config.membrane_sensitivity, 1.0f);

    EXPECT_TRUE(config.enforce_atp_blocking);
    EXPECT_TRUE(config.use_q10_temperature);
    EXPECT_TRUE(config.compensate_homeostatic);
}

/**
 * WHAT: Test default config with NULL pointer
 * WHY:  Verify NULL safety
 */
TEST_F(PlasticitySubstrateBridgeTest, DefaultConfigNullPointer)
{
    int result = plasticity_substrate_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

/**
 * WHAT: Test bridge creation
 * WHY:  Verify successful initialization
 */
TEST_F(PlasticitySubstrateBridgeTest, CreateBridge)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);

    ASSERT_NE(bridge, nullptr);
    EXPECT_EQ(bridge->substrate, substrate);
    EXPECT_FALSE(bridge->base.bio_async_enabled);

    // Check neutral initial effects
    EXPECT_FLOAT_EQ(bridge->effects.global_learning_rate, 1.0f);
    EXPECT_FLOAT_EQ(bridge->effects.plasticity_capacity, 1.0f);
    EXPECT_FLOAT_EQ(bridge->effects.stdp.learning_rate_mod, 1.0f);
    EXPECT_FLOAT_EQ(bridge->effects.bcm.threshold_shift, 1.0f);
}

/**
 * WHAT: Test bridge creation with custom config
 * WHY:  Verify configuration is applied
 */
TEST_F(PlasticitySubstrateBridgeTest, CreateBridgeWithConfig)
{
    plasticity_substrate_config_t config;
    plasticity_substrate_default_config(&config);
    config.atp_sensitivity = 2.0f;
    config.enable_stdp_modulation = false;

    bridge = plasticity_substrate_bridge_create(&config, substrate);

    ASSERT_NE(bridge, nullptr);
    EXPECT_FLOAT_EQ(bridge->config.atp_sensitivity, 2.0f);
    EXPECT_FALSE(bridge->config.enable_stdp_modulation);
}

/**
 * WHAT: Test bridge creation with NULL substrate
 * WHY:  Verify substrate is required
 */
TEST_F(PlasticitySubstrateBridgeTest, CreateBridgeNullSubstrate)
{
    bridge = plasticity_substrate_bridge_create(nullptr, nullptr);
    EXPECT_EQ(bridge, nullptr);
}

/**
 * WHAT: Test bridge destruction
 * WHY:  Verify clean shutdown
 */
TEST_F(PlasticitySubstrateBridgeTest, DestroyBridge)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    plasticity_substrate_bridge_destroy(bridge);
    bridge = nullptr; // Don't double-destroy in TearDown

    // No crash = success
}

/**
 * WHAT: Test bridge destruction with NULL
 * WHY:  Verify NULL-safe destruction
 */
TEST_F(PlasticitySubstrateBridgeTest, DestroyBridgeNullSafe)
{
    plasticity_substrate_bridge_destroy(nullptr);
    // No crash = success
}

/**
 * WHAT: Test connecting plasticity contexts
 * WHY:  Verify context storage
 */
TEST_F(PlasticitySubstrateBridgeTest, ConnectContexts)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    void* stdp_ctx = (void*)0x1234;
    void* bcm_ctx = (void*)0x5678;

    int result = plasticity_substrate_connect_contexts(
        bridge, stdp_ctx, bcm_ctx, nullptr, nullptr, nullptr
    );

    EXPECT_EQ(result, 0);
    EXPECT_EQ(bridge->stdp_context, stdp_ctx);
    EXPECT_EQ(bridge->bcm_context, bcm_ctx);
}

/**
 * WHAT: Test connecting contexts with NULL bridge
 * WHY:  Verify NULL safety
 */
TEST_F(PlasticitySubstrateBridgeTest, ConnectContextsNullBridge)
{
    int result = plasticity_substrate_connect_contexts(
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
    );
    EXPECT_EQ(result, -1);
}

//=============================================================================
// Bio-async Integration Tests
//=============================================================================

/**
 * WHAT: Test bio-async connection
 * WHY:  Verify bio-async integration works
 */
TEST_F(PlasticitySubstrateBridgeTest, BioAsyncConnect)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    int result = plasticity_substrate_connect_bio_async(bridge);

    // May succeed or fail depending on bio-async availability
    // Either way, should not crash
    if (result == 0) {
        EXPECT_TRUE(bridge->base.bio_async_enabled);
    }
}

/**
 * WHAT: Test bio-async connection with NULL bridge
 * WHY:  Verify NULL safety
 */
TEST_F(PlasticitySubstrateBridgeTest, BioAsyncConnectNullBridge)
{
    int result = plasticity_substrate_connect_bio_async(nullptr);
    EXPECT_EQ(result, -1);
}

/**
 * WHAT: Test bio-async disconnection
 * WHY:  Verify clean disconnect
 */
TEST_F(PlasticitySubstrateBridgeTest, BioAsyncDisconnect)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    plasticity_substrate_connect_bio_async(bridge);

    int result = plasticity_substrate_disconnect_bio_async(bridge);

    // Should succeed if was connected, or fail if not
    if (bridge->base.bio_async_enabled) {
        EXPECT_EQ(result, 0);
        EXPECT_FALSE(plasticity_substrate_is_bio_async_connected(bridge));
    }
}

/**
 * WHAT: Test bio-async connection status check
 * WHY:  Verify status query works
 */
TEST_F(PlasticitySubstrateBridgeTest, BioAsyncIsConnected)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    EXPECT_FALSE(plasticity_substrate_is_bio_async_connected(bridge));

    if (plasticity_substrate_connect_bio_async(bridge) == 0) {
        EXPECT_TRUE(plasticity_substrate_is_bio_async_connected(bridge));
    }
}

/**
 * WHAT: Test bio-async status with NULL bridge
 * WHY:  Verify NULL returns false
 */
TEST_F(PlasticitySubstrateBridgeTest, BioAsyncIsConnectedNullBridge)
{
    EXPECT_FALSE(plasticity_substrate_is_bio_async_connected(nullptr));
}

//=============================================================================
// STDP Update Tests - Temperature Effects (Q10 = 2.2)
//=============================================================================

/**
 * WHAT: Test STDP window modulation at normal temperature
 * WHY:  Verify Q10 scaling with T = 37°C (baseline)
 * BIOLOGICAL: Q10 = 2.2 means rate doubles every 10°C
 */
TEST_F(PlasticitySubstrateBridgeTest, StdpNormalTemperature)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    set_physical_state(substrate, NORMAL_TEMP, 0.95f, 0.95f);

    int result = plasticity_substrate_update_stdp(bridge);

    EXPECT_EQ(result, 0);
    EXPECT_NEAR(bridge->effects.stdp.temperature_factor, 1.0f, 0.01f);
    EXPECT_NEAR(bridge->effects.stdp.tau_plus_mod, 1.0f, 0.01f);
    EXPECT_NEAR(bridge->effects.stdp.tau_minus_mod, 1.0f, 0.01f);
}

/**
 * WHAT: Test STDP window at elevated temperature
 * WHY:  Verify hyperthermia expands STDP window
 * BIOLOGICAL: 40°C → Q10^((40-37)/10) = 2.2^0.3 ≈ 1.25x window
 */
TEST_F(PlasticitySubstrateBridgeTest, StdpElevatedTemperature)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    set_physical_state(substrate, 40.0f, 0.95f, 0.95f);

    int result = plasticity_substrate_update_stdp(bridge);

    EXPECT_EQ(result, 0);
    // Q10^((40-37)/10) = 2.2^0.3 ≈ 1.246
    EXPECT_GT(bridge->effects.stdp.temperature_factor, 1.0f);
    EXPECT_LT(bridge->effects.stdp.temperature_factor, 1.5f);
}

/**
 * WHAT: Test STDP window at reduced temperature
 * WHY:  Verify hypothermia contracts STDP window
 * BIOLOGICAL: 32°C → Q10^((32-37)/10) = 2.2^-0.5 ≈ 0.67x window
 */
TEST_F(PlasticitySubstrateBridgeTest, StdpReducedTemperature)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    set_physical_state(substrate, 32.0f, 0.95f, 0.95f);

    int result = plasticity_substrate_update_stdp(bridge);

    EXPECT_EQ(result, 0);
    // Q10^((32-37)/10) = 2.2^-0.5 ≈ 0.674
    EXPECT_LT(bridge->effects.stdp.temperature_factor, 1.0f);
    EXPECT_GT(bridge->effects.stdp.temperature_factor, 0.5f);
}

//=============================================================================
// STDP Update Tests - ATP Gating
//=============================================================================

/**
 * WHAT: Test STDP with full ATP (>0.8)
 * WHY:  Verify full plasticity at high energy
 * BIOLOGICAL: ATP > 0.8 → no limitation
 */
TEST_F(PlasticitySubstrateBridgeTest, StdpFullATP)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    set_metabolic_state(substrate, 0.9f, 0.95f, 0.90f);

    int result = plasticity_substrate_update_stdp(bridge);

    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(bridge->effects.stdp.atp_gating, 1.0f);
    EXPECT_FLOAT_EQ(bridge->effects.stdp.learning_rate_mod, 1.0f);
}

/**
 * WHAT: Test STDP with reduced ATP (0.5-0.8)
 * WHY:  Verify reduced plasticity at moderate energy
 * BIOLOGICAL: Linear scaling in reduced range
 */
TEST_F(PlasticitySubstrateBridgeTest, StdpReducedATP)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    set_metabolic_state(substrate, 0.65f, 0.95f, 0.90f);

    int result = plasticity_substrate_update_stdp(bridge);

    EXPECT_EQ(result, 0);
    // ATP = 0.65 in [0.5, 0.8] → factor = 0.5 + 0.5 * (0.65-0.5)/(0.8-0.5)
    //                                    = 0.5 + 0.5 * 0.5 = 0.75
    EXPECT_GT(bridge->effects.stdp.atp_gating, 0.5f);
    EXPECT_LT(bridge->effects.stdp.atp_gating, 1.0f);
}

/**
 * WHAT: Test STDP with low ATP (0.3-0.5)
 * WHY:  Verify severely reduced plasticity
 * BIOLOGICAL: Critical energy state
 */
TEST_F(PlasticitySubstrateBridgeTest, StdpLowATP)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    set_metabolic_state(substrate, 0.4f, 0.95f, 0.90f);

    int result = plasticity_substrate_update_stdp(bridge);

    EXPECT_EQ(result, 0);
    // ATP = 0.4 in [0.3, 0.5] → factor = 0.1 + 0.4 * (0.4-0.3)/(0.5-0.3)
    //                                   = 0.1 + 0.4 * 0.5 = 0.3
    EXPECT_GT(bridge->effects.stdp.atp_gating, 0.1f);
    EXPECT_LT(bridge->effects.stdp.atp_gating, 0.5f);
}

/**
 * WHAT: Test STDP with blocked ATP (<0.3)
 * WHY:  Verify LTP is blocked at critical energy
 * BIOLOGICAL: ATP < 0.3 → LTP blocked, LTD may persist
 */
TEST_F(PlasticitySubstrateBridgeTest, StdpBlockedATP)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    set_metabolic_state(substrate, 0.2f, 0.95f, 0.90f);

    int result = plasticity_substrate_update_stdp(bridge);

    EXPECT_EQ(result, 0);
    // With enforce_atp_blocking = true, should be 0.0
    EXPECT_FLOAT_EQ(bridge->effects.stdp.atp_gating, 0.0f);
    EXPECT_GT(bridge->stats.atp_limited_events, 0u);
}

/**
 * WHAT: Test STDP with ATP blocking disabled
 * WHY:  Verify minimal LR instead of complete block
 */
TEST_F(PlasticitySubstrateBridgeTest, StdpBlockedATPNoEnforce)
{
    plasticity_substrate_config_t config;
    plasticity_substrate_default_config(&config);
    config.enforce_atp_blocking = false;

    bridge = plasticity_substrate_bridge_create(&config, substrate);
    ASSERT_NE(bridge, nullptr);

    set_metabolic_state(substrate, 0.2f, 0.95f, 0.90f);

    int result = plasticity_substrate_update_stdp(bridge);

    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(bridge->effects.stdp.atp_gating, 0.1f);
}

//=============================================================================
// BCM Update Tests - Threshold Shift
//=============================================================================

/**
 * WHAT: Test BCM threshold at optimal metabolic state
 * WHY:  Verify normal threshold at full energy
 * BIOLOGICAL: Optimal → θ multiplier = 1.0
 */
TEST_F(PlasticitySubstrateBridgeTest, BcmOptimalMetabolic)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    set_metabolic_state(substrate, 0.95f, 0.95f, 0.90f);

    int result = plasticity_substrate_update_bcm(bridge);

    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(bridge->effects.bcm.threshold_shift, 1.0f);
    EXPECT_FLOAT_EQ(bridge->effects.bcm.metabolic_bias, 0.0f);
}

/**
 * WHAT: Test BCM threshold with low ATP
 * WHY:  Verify stress shifts threshold toward LTD
 * BIOLOGICAL: Low ATP → higher threshold → bias LTD (30% increase)
 */
TEST_F(PlasticitySubstrateBridgeTest, BcmLowATPStress)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    set_metabolic_state(substrate, 0.4f, 0.95f, 0.90f);

    int result = plasticity_substrate_update_bcm(bridge);

    EXPECT_EQ(result, 0);
    // ATP = 0.4 < 0.5 → threshold increases
    EXPECT_GT(bridge->effects.bcm.threshold_shift, 1.0f);
    EXPECT_LE(bridge->effects.bcm.threshold_shift, 1.5f);
}

/**
 * WHAT: Test BCM threshold with hypoxia
 * WHY:  Verify O2 depletion shifts threshold (50% increase)
 * BIOLOGICAL: Hypoxia → impaired Ca2+ signaling → bias LTD
 */
TEST_F(PlasticitySubstrateBridgeTest, BcmHypoxiaStress)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    set_metabolic_state(substrate, 0.9f, 0.5f, 0.90f);

    int result = plasticity_substrate_update_bcm(bridge);

    EXPECT_EQ(result, 0);
    // O2 = 0.5 < 0.7 → threshold increases
    EXPECT_GT(bridge->effects.bcm.threshold_shift, 1.0f);  // Some increase expected
    EXPECT_LE(bridge->effects.bcm.threshold_shift, 1.5f);
}

/**
 * WHAT: Test BCM metabolic bias
 * WHY:  Verify low metabolic capacity biases toward LTD
 * BIOLOGICAL: Capacity < 0.7 → negative bias
 */
TEST_F(PlasticitySubstrateBridgeTest, BcmMetabolicBias)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    // Set low ATP and O2 to reduce metabolic capacity
    set_metabolic_state(substrate, 0.4f, 0.5f, 0.5f);

    int result = plasticity_substrate_update_bcm(bridge);

    EXPECT_EQ(result, 0);
    // Low capacity → negative bias (favor LTD)
    EXPECT_LT(bridge->effects.bcm.metabolic_bias, 0.0f);
}

//=============================================================================
// Homeostatic Update Tests
//=============================================================================

/**
 * WHAT: Test homeostatic adjustment at optimal health
 * WHY:  Verify normal target rate at full health
 */
TEST_F(PlasticitySubstrateBridgeTest, HomeostasisOptimalHealth)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    // Substrate starts in optimal state
    int result = plasticity_substrate_update_homeostatic(bridge);

    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(bridge->effects.homeostatic.target_rate_adjustment, 1.0f);
    EXPECT_FLOAT_EQ(bridge->effects.homeostatic.recovery_boost, 0.0f);
}

/**
 * WHAT: Test homeostatic adjustment with degraded substrate
 * WHY:  Verify lower target rate for compromised neurons
 * BIOLOGICAL: Degraded substrate can't sustain high firing rates
 */
TEST_F(PlasticitySubstrateBridgeTest, HomeostasisDegradedSubstrate)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    // Degrade substrate to critical state
    set_metabolic_state(substrate, 0.3f, 0.4f, 0.3f);
    set_physical_state(substrate, 37.0f, 0.6f, 0.5f);

    int result = plasticity_substrate_update_homeostatic(bridge);

    EXPECT_EQ(result, 0);
    // Critical health → 0.7 target rate, 0.3 recovery boost
    EXPECT_LT(bridge->effects.homeostatic.target_rate_adjustment, 1.0f);
    EXPECT_GT(bridge->effects.homeostatic.recovery_boost, 0.0f);
}

/**
 * WHAT: Test homeostatic compensation
 * WHY:  Verify enhanced scaling for degraded substrate
 * BIOLOGICAL: Poor health → faster homeostatic compensation
 */
TEST_F(PlasticitySubstrateBridgeTest, HomeostasisCompensation)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    set_metabolic_state(substrate, 0.5f, 0.6f, 0.5f);

    int result = plasticity_substrate_update_homeostatic(bridge);

    EXPECT_EQ(result, 0);
    // Low capacity → enhanced scaling rate
    EXPECT_GT(bridge->effects.homeostatic.scaling_rate_mod, 1.0f);
}

//=============================================================================
// Eligibility Trace Update Tests
//=============================================================================

/**
 * WHAT: Test eligibility trace decay at normal ATP
 * WHY:  Verify normal trace persistence at high energy
 */
TEST_F(PlasticitySubstrateBridgeTest, EligibilityNormalATP)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    set_metabolic_state(substrate, 0.9f, 0.95f, 0.90f);

    int result = plasticity_substrate_update_eligibility(bridge);

    EXPECT_EQ(result, 0);
    EXPECT_NEAR(bridge->effects.eligibility.decay_lambda_mod, 1.0f, 0.1f);
    EXPECT_FLOAT_EQ(bridge->effects.eligibility.consolidation_gate, 1.0f);
}

/**
 * WHAT: Test eligibility trace decay at low ATP
 * WHY:  Verify faster decay when energy is low
 * BIOLOGICAL: Trace maintenance requires ATP, deficit → faster decay
 */
TEST_F(PlasticitySubstrateBridgeTest, EligibilityLowATP)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    set_metabolic_state(substrate, 0.4f, 0.95f, 0.90f);

    int result = plasticity_substrate_update_eligibility(bridge);

    EXPECT_EQ(result, 0);
    // Low ATP → reduced decay lambda (faster decay)
    EXPECT_LT(bridge->effects.eligibility.decay_lambda_mod, 1.0f);
    EXPECT_GT(bridge->effects.eligibility.decay_lambda_mod, 0.8f);
}

/**
 * WHAT: Test eligibility consolidation gating
 * WHY:  Verify consolidation is ATP-dependent
 * BIOLOGICAL: Protein synthesis requires ATP
 */
TEST_F(PlasticitySubstrateBridgeTest, EligibilityConsolidationGate)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    set_metabolic_state(substrate, 0.3f, 0.95f, 0.90f);

    int result = plasticity_substrate_update_eligibility(bridge);

    EXPECT_EQ(result, 0);
    // ATP < 0.5 → severely reduced consolidation
    EXPECT_LT(bridge->effects.eligibility.consolidation_gate, 0.5f);
    EXPECT_LT(bridge->effects.eligibility.protein_synthesis_rate, 0.5f);
}

//=============================================================================
// Dendritic Update Tests - Membrane Integrity
//=============================================================================

/**
 * WHAT: Test dendritic NMDA at healthy membrane
 * WHY:  Verify full NMDA function with intact membrane
 */
TEST_F(PlasticitySubstrateBridgeTest, DendriticHealthyMembrane)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    set_physical_state(substrate, 37.0f, 0.95f, 0.95f);

    int result = plasticity_substrate_update_dendritic(bridge);

    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(bridge->effects.dendritic.nmda_conductance_mod, 1.0f);
    EXPECT_FLOAT_EQ(bridge->effects.dendritic.membrane_factor, 1.0f);
}

/**
 * WHAT: Test dendritic NMDA with degraded membrane
 * WHY:  Verify reduced NMDA with membrane damage
 * BIOLOGICAL: Damaged membrane → leaky conductance → impaired NMDA
 */
TEST_F(PlasticitySubstrateBridgeTest, DendriticDegradedMembrane)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    set_physical_state(substrate, 37.0f, 0.7f, 0.95f);

    int result = plasticity_substrate_update_dendritic(bridge);

    EXPECT_EQ(result, 0);
    // Membrane = 0.7 in [0.6, 0.9] → reduced NMDA
    EXPECT_GT(bridge->effects.dendritic.nmda_conductance_mod, 0.5f);
    EXPECT_LT(bridge->effects.dendritic.nmda_conductance_mod, 1.0f);
}

/**
 * WHAT: Test dendritic NMDA with critical membrane damage
 * WHY:  Verify minimal NMDA at critical threshold
 * BIOLOGICAL: Membrane < 0.6 → severe impairment
 */
TEST_F(PlasticitySubstrateBridgeTest, DendriticCriticalMembrane)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    set_physical_state(substrate, 37.0f, 0.5f, 0.95f);

    int result = plasticity_substrate_update_dendritic(bridge);

    EXPECT_EQ(result, 0);
    // Membrane < 0.6 → minimal NMDA
    EXPECT_LT(bridge->effects.dendritic.nmda_conductance_mod, 0.5f);
    EXPECT_GT(bridge->stats.membrane_blocks, 0u);
}

/**
 * WHAT: Test dendritic spike threshold with ion imbalance
 * WHY:  Verify raised threshold with ionic dysregulation
 * BIOLOGICAL: Na+/K+ gradient required for dendritic spikes
 */
TEST_F(PlasticitySubstrateBridgeTest, DendriticIonImbalance)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    set_physical_state(substrate, 37.0f, 0.95f, 0.6f);

    int result = plasticity_substrate_update_dendritic(bridge);

    EXPECT_EQ(result, 0);
    // Ion balance = 0.6 → threshold shift
    EXPECT_GT(bridge->effects.dendritic.spike_threshold_shift, 0.0f);
}

//=============================================================================
// Combined Update Tests
//=============================================================================

/**
 * WHAT: Test update all mechanisms
 * WHY:  Verify combined update works
 */
TEST_F(PlasticitySubstrateBridgeTest, UpdateAll)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    int result = plasticity_substrate_update_all(bridge);

    EXPECT_EQ(result, 0);
    EXPECT_GT(bridge->stats.total_updates, 0u);
}

/**
 * WHAT: Test update all with NULL bridge
 * WHY:  Verify NULL safety
 */
TEST_F(PlasticitySubstrateBridgeTest, UpdateAllNullBridge)
{
    int result = plasticity_substrate_update_all(nullptr);
    EXPECT_EQ(result, -1);
}

/**
 * WHAT: Test global learning rate computation
 * WHY:  Verify combined LR from STDP and BCM
 */
TEST_F(PlasticitySubstrateBridgeTest, GlobalLearningRate)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    set_metabolic_state(substrate, 0.6f, 0.95f, 0.90f);

    plasticity_substrate_update_all(bridge);

    float lr_mod = plasticity_substrate_get_learning_rate_mod(bridge);
    EXPECT_GT(lr_mod, 0.1f);
    EXPECT_LE(lr_mod, 1.0f);
}

/**
 * WHAT: Test plasticity capacity computation
 * WHY:  Verify capacity reflects substrate health
 */
TEST_F(PlasticitySubstrateBridgeTest, PlasticityCapacity)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    plasticity_substrate_update_all(bridge);

    float capacity = plasticity_substrate_get_capacity(bridge);
    EXPECT_GE(capacity, 0.0f);
    EXPECT_LE(capacity, 1.0f);
}

//=============================================================================
// Query API Tests
//=============================================================================

/**
 * WHAT: Test get learning rate modulation
 * WHY:  Verify query returns correct value
 */
TEST_F(PlasticitySubstrateBridgeTest, GetLearningRateMod)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    bridge->effects.global_learning_rate = 0.75f;

    float lr_mod = plasticity_substrate_get_learning_rate_mod(bridge);
    EXPECT_FLOAT_EQ(lr_mod, 0.75f);
}

/**
 * WHAT: Test get STDP window modulation
 * WHY:  Verify average of tau_plus and tau_minus
 */
TEST_F(PlasticitySubstrateBridgeTest, GetStdpWindowMod)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    bridge->effects.stdp.tau_plus_mod = 1.2f;
    bridge->effects.stdp.tau_minus_mod = 1.4f;

    float window_mod = plasticity_substrate_get_stdp_window_mod(bridge);
    EXPECT_FLOAT_EQ(window_mod, 1.3f);
}

/**
 * WHAT: Test get BCM threshold shift
 * WHY:  Verify threshold multiplier query
 */
TEST_F(PlasticitySubstrateBridgeTest, GetBcmThresholdShift)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    bridge->effects.bcm.threshold_shift = 1.3f;

    float shift = plasticity_substrate_get_bcm_threshold_shift(bridge);
    EXPECT_FLOAT_EQ(shift, 1.3f);
}

/**
 * WHAT: Test get homeostatic adjustment
 * WHY:  Verify target rate adjustment query
 */
TEST_F(PlasticitySubstrateBridgeTest, GetHomeostaticAdjustment)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    bridge->effects.homeostatic.target_rate_adjustment = 0.85f;

    float adj = plasticity_substrate_get_homeostatic_adjustment(bridge);
    EXPECT_FLOAT_EQ(adj, 0.85f);
}

/**
 * WHAT: Test get eligibility decay modulation
 * WHY:  Verify decay lambda query
 */
TEST_F(PlasticitySubstrateBridgeTest, GetEligibilityDecayMod)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    bridge->effects.eligibility.decay_lambda_mod = 0.9f;

    float decay = plasticity_substrate_get_eligibility_decay_mod(bridge);
    EXPECT_FLOAT_EQ(decay, 0.9f);
}

/**
 * WHAT: Test get NMDA conductance modulation
 * WHY:  Verify NMDA gating query
 */
TEST_F(PlasticitySubstrateBridgeTest, GetNmdaConductanceMod)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    bridge->effects.dendritic.nmda_conductance_mod = 0.7f;

    float nmda = plasticity_substrate_get_nmda_conductance_mod(bridge);
    EXPECT_FLOAT_EQ(nmda, 0.7f);
}

/**
 * WHAT: Test get complete effects structure
 * WHY:  Verify all effects can be retrieved at once
 */
TEST_F(PlasticitySubstrateBridgeTest, GetEffects)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    plasticity_substrate_update_all(bridge);

    plasticity_substrate_effects_t effects;
    int result = plasticity_substrate_get_effects(bridge, &effects);

    EXPECT_EQ(result, 0);
    EXPECT_GE(effects.global_learning_rate, 0.0f);
    EXPECT_LE(effects.global_learning_rate, 1.5f);
}

/**
 * WHAT: Test get effects with NULL
 * WHY:  Verify NULL safety
 */
TEST_F(PlasticitySubstrateBridgeTest, GetEffectsNullPointers)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    plasticity_substrate_effects_t effects;

    EXPECT_EQ(plasticity_substrate_get_effects(nullptr, &effects), -1);
    EXPECT_EQ(plasticity_substrate_get_effects(bridge, nullptr), -1);
}

/**
 * WHAT: Test is plasticity limited
 * WHY:  Verify limitation detection
 */
TEST_F(PlasticitySubstrateBridgeTest, IsLimited)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    // Start not limited
    EXPECT_FALSE(plasticity_substrate_is_limited(bridge));

    // Reduce ATP to limit plasticity
    set_metabolic_state(substrate, 0.3f, 0.95f, 0.90f);
    plasticity_substrate_update_all(bridge);

    EXPECT_TRUE(plasticity_substrate_is_limited(bridge));
}

/**
 * WHAT: Test get statistics
 * WHY:  Verify stats retrieval
 */
TEST_F(PlasticitySubstrateBridgeTest, GetStats)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    plasticity_substrate_update_all(bridge);

    plasticity_substrate_stats_t stats;
    int result = plasticity_substrate_get_stats(bridge, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.total_updates, 1u);
}

/**
 * WHAT: Test get stats with NULL
 * WHY:  Verify NULL safety
 */
TEST_F(PlasticitySubstrateBridgeTest, GetStatsNullPointers)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    plasticity_substrate_stats_t stats;

    EXPECT_EQ(plasticity_substrate_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(plasticity_substrate_get_stats(bridge, nullptr), -1);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

/**
 * WHAT: Test extreme temperature (very hot)
 * WHY:  Verify bounds checking at high temp
 */
TEST_F(PlasticitySubstrateBridgeTest, ExtremeHighTemperature)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    set_physical_state(substrate, 50.0f, 0.95f, 0.95f);

    int result = plasticity_substrate_update_stdp(bridge);

    EXPECT_EQ(result, 0);
    // Should be clamped to max (2.0)
    EXPECT_LE(bridge->effects.stdp.temperature_factor, 2.0f);
}

/**
 * WHAT: Test extreme temperature (very cold)
 * WHY:  Verify bounds checking at low temp
 */
TEST_F(PlasticitySubstrateBridgeTest, ExtremeLowTemperature)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    set_physical_state(substrate, 20.0f, 0.95f, 0.95f);

    int result = plasticity_substrate_update_stdp(bridge);

    EXPECT_EQ(result, 0);
    // Should be clamped to min (0.5)
    EXPECT_GE(bridge->effects.stdp.temperature_factor, 0.5f);
}

/**
 * WHAT: Test zero ATP
 * WHY:  Verify handling of complete energy depletion
 */
TEST_F(PlasticitySubstrateBridgeTest, ZeroATP)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    set_metabolic_state(substrate, 0.0f, 0.95f, 0.90f);

    plasticity_substrate_update_all(bridge);

    EXPECT_FLOAT_EQ(bridge->effects.stdp.atp_gating, 0.0f);
    EXPECT_TRUE(plasticity_substrate_is_limited(bridge));
}

/**
 * WHAT: Test zero membrane integrity
 * WHY:  Verify handling of complete membrane failure
 */
TEST_F(PlasticitySubstrateBridgeTest, ZeroMembraneIntegrity)
{
    bridge = plasticity_substrate_bridge_create(nullptr, substrate);
    ASSERT_NE(bridge, nullptr);

    set_physical_state(substrate, 37.0f, 0.0f, 0.95f);

    plasticity_substrate_update_dendritic(bridge);

    EXPECT_LT(bridge->effects.dendritic.nmda_conductance_mod, 0.5f);
}

/**
 * WHAT: Test disabled modulation mechanisms
 * WHY:  Verify selective disabling of features
 */
TEST_F(PlasticitySubstrateBridgeTest, DisabledModulation)
{
    plasticity_substrate_config_t config;
    plasticity_substrate_default_config(&config);
    config.enable_stdp_modulation = false;
    config.enable_bcm_modulation = false;

    bridge = plasticity_substrate_bridge_create(&config, substrate);
    ASSERT_NE(bridge, nullptr);

    // Updates should return -1 when disabled
    EXPECT_EQ(plasticity_substrate_update_stdp(bridge), -1);
    EXPECT_EQ(plasticity_substrate_update_bcm(bridge), -1);
}

/**
 * WHAT: Test sensitivity multipliers
 * WHY:  Verify sensitivity scaling works
 */
TEST_F(PlasticitySubstrateBridgeTest, SensitivityMultipliers)
{
    plasticity_substrate_config_t config;
    plasticity_substrate_default_config(&config);
    config.atp_sensitivity = 2.0f;

    bridge = plasticity_substrate_bridge_create(&config, substrate);
    ASSERT_NE(bridge, nullptr);

    set_metabolic_state(substrate, 0.6f, 0.95f, 0.90f);
    plasticity_substrate_update_stdp(bridge);

    // Higher sensitivity → stronger modulation
    EXPECT_LT(bridge->effects.stdp.atp_gating, 0.9f);
}

/**
 * WHAT: Test query functions with NULL bridge
 * WHY:  Verify all query functions are NULL-safe
 */
TEST_F(PlasticitySubstrateBridgeTest, QueryFunctionsNullSafe)
{
    EXPECT_FLOAT_EQ(plasticity_substrate_get_learning_rate_mod(nullptr), 1.0f);
    EXPECT_FLOAT_EQ(plasticity_substrate_get_stdp_window_mod(nullptr), 1.0f);
    EXPECT_FLOAT_EQ(plasticity_substrate_get_bcm_threshold_shift(nullptr), 1.0f);
    EXPECT_FLOAT_EQ(plasticity_substrate_get_homeostatic_adjustment(nullptr), 1.0f);
    EXPECT_FLOAT_EQ(plasticity_substrate_get_eligibility_decay_mod(nullptr), 1.0f);
    EXPECT_FLOAT_EQ(plasticity_substrate_get_nmda_conductance_mod(nullptr), 1.0f);
    EXPECT_FLOAT_EQ(plasticity_substrate_get_capacity(nullptr), 1.0f);
    EXPECT_FALSE(plasticity_substrate_is_limited(nullptr));
}
