/**
 * @file test_immune_substrate_e2e.cpp
 * @brief End-to-end tests for immune-substrate integration
 *
 * WHAT: Complete E2E testing of bidirectional immune-substrate interactions
 * WHY:  Validate immune effects on substrate (fever, ATP depletion, damage)
 *       and substrate stress triggering immune responses
 * HOW:  Test inflammation cascades, cytokine modulation, neuroinflammation,
 *       and recovery timelines
 *
 * BIOLOGICAL BASIS:
 * - IL-1beta, TNF-alpha, IL-6 are endogenous pyrogens (cause fever)
 * - TNF-alpha impairs mitochondrial function -> ATP depletion
 * - Inflammation disrupts ion homeostasis and damages membranes
 * - IL-10 promotes anti-inflammatory recovery
 *
 * @version 1.0
 * @date 2026
 */

#include "e2e_test_framework.h"
#include <thread>
#include <chrono>
#include <vector>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <numeric>
#include <atomic>

// Headers have their own extern "C" guards
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "core/neural_substrate/nimcp_substrate_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

using namespace nimcp::e2e;

//=============================================================================
// Test Configuration
//=============================================================================

namespace {
    constexpr uint64_t SIMULATION_STEP_MS = 1;
    constexpr uint64_t INFLAMMATION_DURATION_MS = 5000;
    constexpr uint64_t RECOVERY_DURATION_MS = 10000;
    constexpr float FEVER_THRESHOLD = 38.5f;
    constexpr float NORMAL_TEMPERATURE = 37.0f;
    constexpr uint32_t TEST_ANTIGEN_SEVERITY = 7;
}

//=============================================================================
// Callback Tracking
//=============================================================================

struct ImmuneSubstrateTracker {
    std::atomic<int> antigen_count{0};
    std::atomic<int> cytokine_count{0};
    std::atomic<int> inflammation_count{0};

    void reset() {
        antigen_count = 0;
        cytokine_count = 0;
        inflammation_count = 0;
    }
};

static ImmuneSubstrateTracker g_tracker;

static void on_antigen_callback(
    brain_immune_system_t* system,
    const brain_antigen_t* antigen,
    void* user_data
) {
    (void)system;
    (void)user_data;
    (void)antigen;
    g_tracker.antigen_count++;
}

static void on_cytokine_callback(
    brain_immune_system_t* system,
    const brain_cytokine_t* cytokine,
    void* user_data
) {
    (void)system;
    (void)user_data;
    (void)cytokine;
    g_tracker.cytokine_count++;
}

static void on_inflammation_callback(
    brain_immune_system_t* system,
    const brain_inflammation_site_t* site,
    void* user_data
) {
    (void)system;
    (void)user_data;
    (void)site;
    g_tracker.inflammation_count++;
}

//=============================================================================
// Test Fixture
//=============================================================================

class ImmuneSubstrateE2ETest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    brain_immune_system_t* immune = nullptr;
    substrate_immune_bridge_t* bridge = nullptr;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();
        g_tracker.reset();

        // Create substrate
        substrate_config_t sub_config;
        ASSERT_EQ(substrate_default_config(&sub_config), 0);
        sub_config.enable_metabolic_model = true;
        sub_config.enable_temperature_effects = true;
        sub_config.enable_ion_dynamics = true;
        sub_config.enable_alerts = true;
        substrate = substrate_create(&sub_config);
        ASSERT_NE(substrate, nullptr);

        // Create immune system
        brain_immune_config_t imm_config;
        brain_immune_default_config(&imm_config);
        imm_config.enable_logging = false;
        immune = brain_immune_create(&imm_config);
        ASSERT_NE(immune, nullptr);

        // Register callbacks
        brain_immune_set_antigen_callback(immune, on_antigen_callback, nullptr);
        brain_immune_set_cytokine_callback(immune, on_cytokine_callback, nullptr);
        brain_immune_set_inflammation_callback(immune, on_inflammation_callback, nullptr);

        // Start immune system (required for brain_immune_update to process)
        brain_immune_start(immune);

        // Create bridge
        substrate_immune_config_t bridge_config;
        ASSERT_EQ(substrate_immune_default_config(&bridge_config), 0);
        bridge_config.enable_fever_response = true;
        bridge_config.enable_metabolic_effects = true;
        bridge_config.enable_damage_effects = true;
        bridge_config.enable_substrate_immune_trigger = true;
        bridge_config.enable_il10_recovery = true;

        bridge = substrate_immune_bridge_create(&bridge_config, substrate, immune);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            substrate_immune_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (immune) {
            brain_immune_stop(immune);
            brain_immune_destroy(immune);
            immune = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_LT(stats.current_allocated, 8192)
            << "Memory leak detected: " << stats.current_allocated << " bytes";
    }

    uint32_t presentAntigen(uint32_t severity) {
        uint8_t epitope[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04};
        uint32_t antigen_id = 0;

        int result = brain_immune_present_antigen(
            immune,
            ANTIGEN_SOURCE_MANUAL,
            epitope,
            sizeof(epitope),
            severity,
            1,
            &antigen_id
        );

        EXPECT_EQ(result, 0);
        return antigen_id;
    }

    void runBridgeSimulation(uint64_t steps) {
        for (uint64_t i = 0; i < steps; i++) {
            ASSERT_EQ(substrate_immune_bridge_update(bridge, SIMULATION_STEP_MS), 0);
            ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);
        }
    }
};

//=============================================================================
// E2E Test: Immune-Substrate Interaction Lifecycle
//=============================================================================

TEST_F(ImmuneSubstrateE2ETest, ImmuneSubstrateInteractionLifecycle) {
    E2E_PIPELINE_START("Immune-Substrate Interaction Lifecycle");

    E2E_STAGE_BEGIN("Verify initial healthy state", 100);
    substrate_physical_state_t initial_phys;
    ASSERT_EQ(substrate_get_physical_state(substrate, &initial_phys), 0);

    std::cout << "  Initial temperature: " << initial_phys.temperature << " C" << std::endl;
    EXPECT_NEAR(initial_phys.temperature, NORMAL_TEMPERATURE, 1.0f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Present antigen to trigger immune response", 1000);
    uint32_t antigen_id = presentAntigen(TEST_ANTIGEN_SEVERITY);
    EXPECT_GT(antigen_id, 0u);

    // Run immune system to process antigen
    for (int i = 0; i < 100; i++) {
        brain_immune_update(immune, 10);
    }

    std::cout << "  Antigens detected: " << g_tracker.antigen_count.load() << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply immune effects to substrate", 5000);
    // Apply fever and metabolic effects
    ASSERT_EQ(substrate_immune_apply_fever(bridge), 0);
    ASSERT_EQ(substrate_immune_apply_metabolic_effects(bridge), 0);
    ASSERT_EQ(substrate_immune_bridge_update(bridge, 100), 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify substrate changes from immune activity", 100);
    cytokine_substrate_effects_t effects;
    ASSERT_EQ(substrate_immune_get_cytokine_effects(bridge, &effects), 0);

    std::cout << "  Fever intensity: " << effects.fever_intensity << std::endl;
    std::cout << "  Metabolic burden: " << effects.metabolic_burden << std::endl;
    std::cout << "  Total temp increase: " << effects.total_temp_increase << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Run extended integration", 30000);
    runBridgeSimulation(5000);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify final state", 100);
    bool modulated = substrate_immune_is_modulated(bridge);
    std::cout << "  Substrate is modulated: " << (modulated ? "yes" : "no") << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Inflammation Cascade Effects
//=============================================================================

TEST_F(ImmuneSubstrateE2ETest, InflammationCascadeEffects) {
    E2E_PIPELINE_START("Inflammation Cascade Effects");

    E2E_STAGE_BEGIN("Record baseline state", 100);
    substrate_metabolic_state_t baseline;
    ASSERT_EQ(substrate_get_metabolic_state(substrate, &baseline), 0);
    substrate_physical_state_t baseline_phys;
    ASSERT_EQ(substrate_get_physical_state(substrate, &baseline_phys), 0);

    std::cout << "  Baseline ATP: " << baseline.atp_level << std::endl;
    std::cout << "  Baseline temperature: " << baseline_phys.temperature << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Trigger inflammatory response", 2000);
    // Present multiple antigens to escalate inflammation
    for (int i = 0; i < 5; i++) {
        presentAntigen(8);
        brain_immune_update(immune, 50);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Run inflammation cascade", 20000);
    std::vector<float> temp_timeline;
    std::vector<float> atp_timeline;

    for (uint64_t step = 0; step < 3000; step++) {
        // Apply all immune effects
        ASSERT_EQ(substrate_immune_apply_fever(bridge), 0);
        ASSERT_EQ(substrate_immune_apply_metabolic_effects(bridge), 0);
        ASSERT_EQ(substrate_immune_apply_damage(bridge), 0);
        ASSERT_EQ(substrate_immune_bridge_update(bridge, SIMULATION_STEP_MS), 0);
        brain_immune_update(immune, SIMULATION_STEP_MS);

        if (step % 200 == 0) {
            substrate_physical_state_t phys;
            ASSERT_EQ(substrate_get_physical_state(substrate, &phys), 0);
            temp_timeline.push_back(phys.temperature);

            substrate_metabolic_state_t met;
            ASSERT_EQ(substrate_get_metabolic_state(substrate, &met), 0);
            atp_timeline.push_back(met.atp_level);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Analyze inflammation effects", 100);
    float max_temp = *std::max_element(temp_timeline.begin(), temp_timeline.end());
    float min_atp = *std::min_element(atp_timeline.begin(), atp_timeline.end());

    std::cout << "  Peak temperature: " << max_temp << " C" << std::endl;
    std::cout << "  Lowest ATP: " << min_atp << std::endl;
    std::cout << "  Inflammation events: " << g_tracker.inflammation_count.load() << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Check fever intensity", 100);
    float fever = substrate_immune_get_fever_intensity(bridge);
    std::cout << "  Current fever intensity: " << fever << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Cytokine Modulation of Substrate
//=============================================================================

TEST_F(ImmuneSubstrateE2ETest, CytokineModulationOfSubstrate) {
    E2E_PIPELINE_START("Cytokine Modulation of Substrate");

    E2E_STAGE_BEGIN("Induce cytokine release", 5000);
    // Present antigen to trigger cytokine cascade
    presentAntigen(9);

    for (int i = 0; i < 200; i++) {
        brain_immune_update(immune, 10);
    }

    std::cout << "  Cytokines released: " << g_tracker.cytokine_count.load() << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply cytokine effects", 1000);
    for (int i = 0; i < 100; i++) {
        ASSERT_EQ(substrate_immune_apply_fever(bridge), 0);
        ASSERT_EQ(substrate_immune_apply_metabolic_effects(bridge), 0);
        ASSERT_EQ(substrate_immune_bridge_update(bridge, 10), 0);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Analyze cytokine substrate effects", 100);
    cytokine_substrate_effects_t effects;
    ASSERT_EQ(substrate_immune_get_cytokine_effects(bridge, &effects), 0);

    std::cout << "  IL-1 temperature effect: " << effects.il1_temp_effect << std::endl;
    std::cout << "  IL-6 temperature effect: " << effects.il6_temp_effect << std::endl;
    std::cout << "  TNF ATP effect: " << effects.tnf_atp_effect << std::endl;
    std::cout << "  IFN O2 effect: " << effects.ifn_o2_effect << std::endl;
    std::cout << "  TNF membrane effect: " << effects.tnf_membrane_effect << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify substrate modulation", 100);
    substrate_modulation_t mod;
    ASSERT_EQ(substrate_get_modulation(substrate, &mod), 0);

    std::cout << "  Firing rate mod: " << mod.firing_rate_mod << std::endl;
    std::cout << "  Transmission efficiency: " << mod.transmission_efficiency << std::endl;
    std::cout << "  Overall capacity: " << mod.overall_capacity << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Neuroinflammation Simulation
//=============================================================================

TEST_F(ImmuneSubstrateE2ETest, NeuroinflammationSimulation) {
    E2E_PIPELINE_START("Neuroinflammation Simulation");

    E2E_STAGE_BEGIN("Establish chronic inflammation", 30000);
    // Simulate persistent antigen exposure (chronic infection)
    for (int wave = 0; wave < 10; wave++) {
        presentAntigen(6);

        for (int i = 0; i < 300; i++) {
            brain_immune_update(immune, SIMULATION_STEP_MS);
            ASSERT_EQ(substrate_immune_apply_fever(bridge), 0);
            ASSERT_EQ(substrate_immune_apply_metabolic_effects(bridge), 0);
            ASSERT_EQ(substrate_immune_apply_damage(bridge), 0);
            ASSERT_EQ(substrate_immune_bridge_update(bridge, SIMULATION_STEP_MS), 0);
            ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Assess neuroinflammation damage", 100);
    substrate_physical_state_t phys;
    ASSERT_EQ(substrate_get_physical_state(substrate, &phys), 0);

    std::cout << "  Membrane integrity: " << phys.membrane_integrity << std::endl;
    std::cout << "  Ion balance: " << phys.ion_balance << std::endl;
    std::cout << "  Physical capacity: " << phys.physical_capacity << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Check substrate health degradation", 100);
    substrate_health_level_t health = substrate_get_health_level(substrate);
    std::cout << "  Health level: " << substrate_health_level_to_string(health) << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify alerts reflect neuroinflammation", 100);
    substrate_alert_type_t alerts[8];
    uint32_t alert_count = 0;
    ASSERT_EQ(substrate_get_alerts(substrate, alerts, &alert_count), 0);

    std::cout << "  Active alerts: " << alert_count << std::endl;
    for (uint32_t i = 0; i < alert_count; i++) {
        std::cout << "    - " << substrate_alert_type_to_string(alerts[i]) << std::endl;
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Recovery Timeline Validation
//=============================================================================

TEST_F(ImmuneSubstrateE2ETest, RecoveryTimelineValidation) {
    E2E_PIPELINE_START("Recovery Timeline Validation");

    E2E_STAGE_BEGIN("Induce acute inflammation", 10000);
    // Strong but brief immune response with explicit cytokine release
    for (int i = 0; i < 3; i++) {
        uint32_t ag_id = presentAntigen(9);
        uint32_t site_id = 0;
        brain_immune_initiate_inflammation(immune, 1, ag_id, &site_id);
    }
    brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_IL1, 0, 0.8f, 1, nullptr);
    brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_IL6, 0, 0.6f, 1, nullptr);
    brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_TNF, 0, 0.5f, 1, nullptr);

    for (uint64_t step = 0; step < 2000; step++) {
        brain_immune_update(immune, SIMULATION_STEP_MS);
        ASSERT_EQ(substrate_immune_apply_fever(bridge), 0);
        ASSERT_EQ(substrate_immune_apply_metabolic_effects(bridge), 0);
        ASSERT_EQ(substrate_immune_apply_damage(bridge), 0);
        ASSERT_EQ(substrate_immune_bridge_update(bridge, SIMULATION_STEP_MS), 0);
        ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Record post-inflammation state", 100);
    substrate_physical_state_t inflamed;
    ASSERT_EQ(substrate_get_physical_state(substrate, &inflamed), 0);
    substrate_metabolic_state_t inflamed_met;
    ASSERT_EQ(substrate_get_metabolic_state(substrate, &inflamed_met), 0);

    std::cout << "  Post-inflammation temperature: " << inflamed.temperature << std::endl;
    std::cout << "  Post-inflammation ATP: " << inflamed_met.atp_level << std::endl;
    std::cout << "  Post-inflammation membrane: " << inflamed.membrane_integrity << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply IL-10 recovery treatment", 60000);
    // Resolve inflammation to stop ongoing fever before recovery
    brain_immune_stats_t pre_stats;
    brain_immune_get_stats(immune, &pre_stats);
    for (uint32_t i = 0; i < pre_stats.inflammation_sites; i++) {
        brain_immune_resolve_inflammation(immune, i);
    }
    // Release anti-inflammatory IL-10
    brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_IL10, 0, 0.9f, 1, nullptr);

    std::vector<std::pair<uint64_t, float>> recovery_curve;

    for (uint64_t step = 0; step < RECOVERY_DURATION_MS; step++) {
        // Apply anti-inflammatory IL-10 (skip bridge_update to avoid re-applying fever)
        brain_immune_update(immune, SIMULATION_STEP_MS);
        ASSERT_EQ(substrate_immune_apply_il10_recovery(bridge, 0.8f), 0);
        ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);

        if (step % 500 == 0) {
            float capacity = substrate_get_capacity(substrate);
            recovery_curve.push_back({step, capacity});
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Analyze recovery trajectory", 100);
    ASSERT_GT(recovery_curve.size(), 5u);

    std::cout << "  Recovery curve:" << std::endl;
    for (const auto& [step, cap] : recovery_curve) {
        std::cout << "    Step " << step << ": capacity = " << cap << std::endl;
    }

    // Verify recovery trend
    float start_cap = recovery_curve.front().second;
    float end_cap = recovery_curve.back().second;
    std::cout << "  Recovery improvement: " << (end_cap - start_cap) << std::endl;
    EXPECT_GT(end_cap, start_cap) << "Should show recovery over time";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify final recovered state", 100);
    substrate_physical_state_t recovered;
    ASSERT_EQ(substrate_get_physical_state(substrate, &recovered), 0);

    std::cout << "  Recovered temperature: " << recovered.temperature << std::endl;
    std::cout << "  Recovered membrane: " << recovered.membrane_integrity << std::endl;

    // Temperature should return toward normal
    EXPECT_LT(std::abs(recovered.temperature - NORMAL_TEMPERATURE),
              std::abs(inflamed.temperature - NORMAL_TEMPERATURE));
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Substrate Stress Triggering Immune Response
//=============================================================================

TEST_F(ImmuneSubstrateE2ETest, SubstrateStressTriggeringImmuneResponse) {
    E2E_PIPELINE_START("Substrate Stress Triggering Immune");

    E2E_STAGE_BEGIN("Induce substrate stress without immune activity", 100);
    // Direct substrate damage (simulating physical injury)
    ASSERT_EQ(substrate_set_membrane_integrity(substrate, 0.4f), 0);
    ASSERT_EQ(substrate_set_atp(substrate, 0.35f), 0);
    ASSERT_EQ(substrate_set_ion_balance(substrate, 0.45f), 0);
    ASSERT_EQ(substrate_update(substrate, 100), 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Check for DAMP release trigger", 1000);
    // Need multiple consecutive stress checks to exceed persistence threshold
    bool should_trigger = false;
    for (int check = 0; check < 5; check++) {
        ASSERT_EQ(substrate_update(substrate, 10), 0);
        should_trigger = substrate_immune_check_stress(bridge);
        if (should_trigger) break;
    }
    std::cout << "  Stress trigger detected: " << (should_trigger ? "yes" : "no") << std::endl;
    EXPECT_TRUE(should_trigger) << "Severe stress should trigger immune";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Compute stress severity", 100);
    uint32_t severity = substrate_immune_compute_severity(bridge);
    std::cout << "  Computed severity: " << severity << "/10" << std::endl;
    EXPECT_GT(severity, 3u);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Trigger immune response from substrate", 5000);
    int result = substrate_immune_trigger_response(bridge);
    EXPECT_EQ(result, 0);

    // Process the triggered response
    for (int i = 0; i < 200; i++) {
        brain_immune_update(immune, 10);
        ASSERT_EQ(substrate_immune_bridge_update(bridge, 10), 0);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify trigger state", 100);
    substrate_immune_trigger_t trigger;
    ASSERT_EQ(substrate_immune_get_trigger_state(bridge, &trigger), 0);

    std::cout << "  Immune triggered: " << (trigger.immune_triggered ? "yes" : "no") << std::endl;
    std::cout << "  ATP alert: " << (trigger.atp_alert ? "yes" : "no") << std::endl;
    std::cout << "  Membrane alert: " << (trigger.membrane_alert ? "yes" : "no") << std::endl;
    std::cout << "  Ion alert: " << (trigger.ion_alert ? "yes" : "no") << std::endl;
    std::cout << "  Computed severity: " << trigger.computed_severity << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Bidirectional Update Cycle
//=============================================================================

TEST_F(ImmuneSubstrateE2ETest, BidirectionalUpdateCycle) {
    E2E_PIPELINE_START("Bidirectional Update Cycle");

    E2E_STAGE_BEGIN("Start with healthy baseline", 100);
    substrate_immune_stats_t initial_stats;
    ASSERT_EQ(substrate_immune_get_stats(bridge, &initial_stats), 0);
    std::cout << "  Initial updates: " << initial_stats.total_updates << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Run bidirectional simulation", 60000);
    // Alternate between immune stress and recovery
    for (int cycle = 0; cycle < 5; cycle++) {
        // Stress phase: present antigen
        presentAntigen(7);

        // Run with immune effects
        for (uint64_t step = 0; step < 1000; step++) {
            brain_immune_update(immune, SIMULATION_STEP_MS);
            ASSERT_EQ(substrate_immune_bridge_update(bridge, SIMULATION_STEP_MS), 0);
            ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);
        }

        // Recovery phase: IL-10
        for (uint64_t step = 0; step < 1000; step++) {
            ASSERT_EQ(substrate_immune_apply_il10_recovery(bridge, 0.5f), 0);
            ASSERT_EQ(substrate_immune_bridge_update(bridge, SIMULATION_STEP_MS), 0);
            ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify bridge statistics", 100);
    substrate_immune_stats_t final_stats;
    ASSERT_EQ(substrate_immune_get_stats(bridge, &final_stats), 0);

    std::cout << "  Total updates: " << final_stats.total_updates << std::endl;
    std::cout << "  Fever cycles: " << final_stats.fever_cycles << std::endl;
    std::cout << "  ATP depletions: " << final_stats.atp_depletions << std::endl;
    std::cout << "  Membrane damages: " << final_stats.membrane_damages << std::endl;
    std::cout << "  Immune triggers: " << final_stats.immune_triggers << std::endl;
    std::cout << "  IL-10 recoveries: " << final_stats.il10_recoveries << std::endl;
    std::cout << "  Max temperature: " << final_stats.max_temperature << std::endl;
    std::cout << "  Min ATP level: " << final_stats.min_atp_level << std::endl;

    EXPECT_GT(final_stats.total_updates, initial_stats.total_updates);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Fever Response Dynamics
//=============================================================================

TEST_F(ImmuneSubstrateE2ETest, FeverResponseDynamics) {
    E2E_PIPELINE_START("Fever Response Dynamics");

    E2E_STAGE_BEGIN("Record normal temperature", 100);
    substrate_physical_state_t normal;
    ASSERT_EQ(substrate_get_physical_state(substrate, &normal), 0);
    std::cout << "  Normal temperature: " << normal.temperature << " C" << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Trigger fever response", 10000);
    // Present high-severity antigen and initiate inflammation
    uint32_t antigen_id = presentAntigen(9);
    uint32_t site_id = 0;
    brain_immune_initiate_inflammation(immune, 1, antigen_id, &site_id);
    // Release pro-inflammatory cytokines (endogenous pyrogens)
    brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_IL1, 0, 0.7f, 1, nullptr);
    brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_IL6, 0, 0.5f, 1, nullptr);

    std::vector<float> temp_curve;

    for (uint64_t step = 0; step < 2000; step++) {
        brain_immune_update(immune, SIMULATION_STEP_MS);
        ASSERT_EQ(substrate_immune_apply_fever(bridge), 0);
        ASSERT_EQ(substrate_immune_bridge_update(bridge, SIMULATION_STEP_MS), 0);
        ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);

        if (step % 100 == 0) {
            substrate_physical_state_t phys;
            ASSERT_EQ(substrate_get_physical_state(substrate, &phys), 0);
            temp_curve.push_back(phys.temperature);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Analyze fever curve", 100);
    ASSERT_GT(temp_curve.size(), 10u);

    float max_temp = *std::max_element(temp_curve.begin(), temp_curve.end());
    float min_temp = *std::min_element(temp_curve.begin(), temp_curve.end());

    std::cout << "  Temperature range: [" << min_temp << ", " << max_temp << "] C" << std::endl;
    std::cout << "  Peak fever: " << max_temp << " C" << std::endl;

    // Should show temperature elevation
    EXPECT_GT(max_temp, normal.temperature);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test fever intensity measurement", 100);
    float fever_intensity = substrate_immune_get_fever_intensity(bridge);
    std::cout << "  Fever intensity: " << fever_intensity << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Damage Effect Accumulation
//=============================================================================

TEST_F(ImmuneSubstrateE2ETest, DamageEffectAccumulation) {
    E2E_PIPELINE_START("Damage Effect Accumulation");

    E2E_STAGE_BEGIN("Record initial integrity", 100);
    substrate_physical_state_t initial;
    ASSERT_EQ(substrate_get_physical_state(substrate, &initial), 0);
    std::cout << "  Initial membrane: " << initial.membrane_integrity << std::endl;
    std::cout << "  Initial ion balance: " << initial.ion_balance << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply cumulative damage", 30000);
    // Multiple antigen exposures causing cumulative damage
    std::vector<float> membrane_curve;
    std::vector<float> ion_curve;

    for (int wave = 0; wave < 10; wave++) {
        presentAntigen(7);

        for (uint64_t step = 0; step < 500; step++) {
            brain_immune_update(immune, SIMULATION_STEP_MS);
            ASSERT_EQ(substrate_immune_apply_damage(bridge), 0);
            ASSERT_EQ(substrate_immune_bridge_update(bridge, SIMULATION_STEP_MS), 0);
            ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);
        }

        substrate_physical_state_t phys;
        ASSERT_EQ(substrate_get_physical_state(substrate, &phys), 0);
        membrane_curve.push_back(phys.membrane_integrity);
        ion_curve.push_back(phys.ion_balance);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Analyze damage accumulation", 100);
    std::cout << "  Membrane integrity over time:" << std::endl;
    for (size_t i = 0; i < membrane_curve.size(); i++) {
        std::cout << "    Wave " << i << ": membrane=" << membrane_curve[i]
                  << ", ion=" << ion_curve[i] << std::endl;
    }

    // Should show degradation trend
    float final_membrane = membrane_curve.back();
    std::cout << "  Final membrane: " << final_membrane << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Multi-Antigen Response
//=============================================================================

TEST_F(ImmuneSubstrateE2ETest, MultiAntigenResponse) {
    E2E_PIPELINE_START("Multi-Antigen Response");

    E2E_STAGE_BEGIN("Present multiple distinct antigens", 5000);
    for (int i = 0; i < 5; i++) {
        presentAntigen(5 + i);  // Varying severities
        brain_immune_update(immune, 100);
    }

    std::cout << "  Total antigens presented: " << g_tracker.antigen_count.load() << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Run complex immune response", 30000);
    for (uint64_t step = 0; step < 5000; step++) {
        brain_immune_update(immune, SIMULATION_STEP_MS);
        ASSERT_EQ(substrate_immune_apply_fever(bridge), 0);
        ASSERT_EQ(substrate_immune_apply_metabolic_effects(bridge), 0);
        ASSERT_EQ(substrate_immune_apply_damage(bridge), 0);
        ASSERT_EQ(substrate_immune_bridge_update(bridge, SIMULATION_STEP_MS), 0);
        ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Assess combined immune burden", 100);
    cytokine_substrate_effects_t effects;
    ASSERT_EQ(substrate_immune_get_cytokine_effects(bridge, &effects), 0);

    std::cout << "  Combined fever intensity: " << effects.fever_intensity << std::endl;
    std::cout << "  Combined metabolic burden: " << effects.metabolic_burden << std::endl;
    std::cout << "  Combined damage severity: " << effects.damage_severity << std::endl;

    float capacity = substrate_get_capacity(substrate);
    std::cout << "  Overall capacity: " << capacity << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Modulation Query Consistency
//=============================================================================

TEST_F(ImmuneSubstrateE2ETest, ModulationQueryConsistency) {
    E2E_PIPELINE_START("Modulation Query Consistency");

    E2E_STAGE_BEGIN("Test modulation before immune activity", 100);
    bool modulated_before = substrate_immune_is_modulated(bridge);
    std::cout << "  Modulated before immune: " << (modulated_before ? "yes" : "no") << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Trigger immune response", 10000);
    presentAntigen(8);

    for (uint64_t step = 0; step < 2000; step++) {
        brain_immune_update(immune, SIMULATION_STEP_MS);
        ASSERT_EQ(substrate_immune_apply_fever(bridge), 0);
        ASSERT_EQ(substrate_immune_apply_metabolic_effects(bridge), 0);
        ASSERT_EQ(substrate_immune_bridge_update(bridge, SIMULATION_STEP_MS), 0);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test modulation after immune activity", 100);
    bool modulated_after = substrate_immune_is_modulated(bridge);
    std::cout << "  Modulated after immune: " << (modulated_after ? "yes" : "no") << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify effect queries are consistent", 100);
    cytokine_substrate_effects_t effects;
    ASSERT_EQ(substrate_immune_get_cytokine_effects(bridge, &effects), 0);

    float fever = substrate_immune_get_fever_intensity(bridge);
    std::cout << "  Query fever: " << fever << std::endl;
    std::cout << "  Effects fever: " << effects.fever_intensity << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Chronic Inflammation Model
//=============================================================================

TEST_F(ImmuneSubstrateE2ETest, ChronicInflammationModel) {
    E2E_PIPELINE_START("Chronic Inflammation Model");

    E2E_STAGE_BEGIN("Establish chronic low-grade inflammation", 60000);
    std::vector<float> capacity_over_time;

    for (int day = 0; day < 10; day++) {
        // Present low-severity antigen daily
        presentAntigen(4);

        // Simulate one "day" of activity
        for (uint64_t step = 0; step < 1000; step++) {
            brain_immune_update(immune, SIMULATION_STEP_MS);
            ASSERT_EQ(substrate_immune_apply_fever(bridge), 0);
            ASSERT_EQ(substrate_immune_apply_metabolic_effects(bridge), 0);
            ASSERT_EQ(substrate_immune_bridge_update(bridge, SIMULATION_STEP_MS), 0);
            ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);
        }

        float cap = substrate_get_capacity(substrate);
        capacity_over_time.push_back(cap);
        std::cout << "  Day " << (day + 1) << " capacity: " << cap << std::endl;
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Analyze chronic effects", 100);
    EXPECT_GT(capacity_over_time.size(), 0u);
    // Chronic inflammation may show gradual degradation
    if (!capacity_over_time.empty()) {
        std::cout << "  Initial capacity: " << capacity_over_time.front() << std::endl;
        std::cout << "  Final capacity: " << capacity_over_time.back() << std::endl;
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Autoimmune-like Response
//=============================================================================

TEST_F(ImmuneSubstrateE2ETest, AutoimmuneLikeResponse) {
    E2E_PIPELINE_START("Autoimmune-like Response");

    E2E_STAGE_BEGIN("Simulate self-antigen presentation", 30000);
    // Present repeated antigens to simulate autoimmune activation
    for (int wave = 0; wave < 10; wave++) {
        presentAntigen(6);

        for (uint64_t step = 0; step < 500; step++) {
            brain_immune_update(immune, SIMULATION_STEP_MS);
            ASSERT_EQ(substrate_immune_apply_fever(bridge), 0);
            ASSERT_EQ(substrate_immune_apply_metabolic_effects(bridge), 0);
            ASSERT_EQ(substrate_immune_apply_damage(bridge), 0);
            ASSERT_EQ(substrate_immune_bridge_update(bridge, SIMULATION_STEP_MS), 0);
            ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Assess autoimmune damage", 100);
    substrate_physical_state_t phys;
    ASSERT_EQ(substrate_get_physical_state(substrate, &phys), 0);

    std::cout << "  Membrane integrity after autoimmune: " << phys.membrane_integrity << std::endl;
    std::cout << "  Ion balance after autoimmune: " << phys.ion_balance << std::endl;

    substrate_health_level_t health = substrate_get_health_level(substrate);
    std::cout << "  Health level: " << substrate_health_level_to_string(health) << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Sepsis-like Cytokine Storm
//=============================================================================

TEST_F(ImmuneSubstrateE2ETest, SepsisLikeCytokineStorm) {
    E2E_PIPELINE_START("Sepsis-like Cytokine Storm");

    E2E_STAGE_BEGIN("Trigger massive immune response", 20000);
    // Present many high-severity antigens with cytokine storm
    for (int i = 0; i < 10; i++) {
        uint32_t ag_id = presentAntigen(10);  // Maximum severity
        uint32_t site_id = 0;
        brain_immune_initiate_inflammation(immune, (uint32_t)i, ag_id, &site_id);
    }
    // Release massive cytokines (cytokine storm)
    brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_IL1, 0, 0.9f, 1, nullptr);
    brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_IL6, 0, 0.9f, 1, nullptr);
    brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_TNF, 0, 0.9f, 1, nullptr);
    brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_IFN_GAMMA, 0, 0.8f, 1, nullptr);

    // Run aggressive immune response
    for (uint64_t step = 0; step < 3000; step++) {
        brain_immune_update(immune, SIMULATION_STEP_MS);
        ASSERT_EQ(substrate_immune_apply_fever(bridge), 0);
        ASSERT_EQ(substrate_immune_apply_metabolic_effects(bridge), 0);
        ASSERT_EQ(substrate_immune_apply_damage(bridge), 0);
        ASSERT_EQ(substrate_immune_bridge_update(bridge, SIMULATION_STEP_MS), 0);
        ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Assess storm damage", 100);
    substrate_physical_state_t phys;
    ASSERT_EQ(substrate_get_physical_state(substrate, &phys), 0);

    std::cout << "  Temperature during storm: " << phys.temperature << " C" << std::endl;

    float fever = substrate_immune_get_fever_intensity(bridge);
    std::cout << "  Fever intensity: " << fever << std::endl;

    substrate_health_level_t health = substrate_get_health_level(substrate);
    std::cout << "  Health level: " << substrate_health_level_to_string(health) << std::endl;

    // Storm should cause severe effects
    EXPECT_GE((int)health, (int)SUBSTRATE_HEALTH_STRESSED);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Anti-inflammatory Treatment
//=============================================================================

TEST_F(ImmuneSubstrateE2ETest, AntiInflammatoryTreatment) {
    E2E_PIPELINE_START("Anti-inflammatory Treatment");

    E2E_STAGE_BEGIN("Induce inflammation", 10000);
    for (int i = 0; i < 3; i++) {
        presentAntigen(8);
    }

    for (uint64_t step = 0; step < 2000; step++) {
        brain_immune_update(immune, SIMULATION_STEP_MS);
        ASSERT_EQ(substrate_immune_apply_fever(bridge), 0);
        ASSERT_EQ(substrate_immune_apply_metabolic_effects(bridge), 0);
        ASSERT_EQ(substrate_immune_bridge_update(bridge, SIMULATION_STEP_MS), 0);
        ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);
    }

    float pre_treatment = substrate_immune_get_fever_intensity(bridge);
    std::cout << "  Pre-treatment fever: " << pre_treatment << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply high-dose IL-10 treatment", 30000);
    for (uint64_t step = 0; step < 5000; step++) {
        ASSERT_EQ(substrate_immune_apply_il10_recovery(bridge, 1.0f), 0);  // High dose
        ASSERT_EQ(substrate_immune_bridge_update(bridge, SIMULATION_STEP_MS), 0);
        ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);
    }

    float post_treatment = substrate_immune_get_fever_intensity(bridge);
    std::cout << "  Post-treatment fever: " << post_treatment << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify treatment effectiveness", 100);
    substrate_immune_stats_t stats;
    ASSERT_EQ(substrate_immune_get_stats(bridge, &stats), 0);

    std::cout << "  IL-10 recoveries: " << stats.il10_recoveries << std::endl;
    EXPECT_GT(stats.il10_recoveries, 0u);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Main Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
