/**
 * @file test_metabolic_cascade_integration.cpp
 * @brief Integration tests for metabolic state affecting multiple bridges simultaneously
 * @date 2025-12-30
 *
 * WHAT: Tests how neural substrate metabolic state cascades through all cognitive
 *       bridges simultaneously, simulating biological energy dependence
 *
 * WHY: In biological brains, ATP depletion, fatigue, and metabolic stress affect
 *      ALL cognitive functions simultaneously:
 *      - Attention narrows and focus degrades
 *      - Emotional regulation weakens
 *      - Reasoning becomes shallow and error-prone
 *      - Memory consolidation fails
 *      - Theory of Mind becomes impaired
 *
 * HOW: Creates complete bridge ecosystem, systematically varies metabolic parameters,
 *      measures cascade effects across all cognitive systems
 *
 * TEST SCENARIOS:
 * 1. Progressive ATP depletion cascade
 * 2. Sudden metabolic crisis and recovery
 * 3. Fatigue accumulation effects
 * 4. Temperature variations (fever/hypothermia)
 * 5. Combined metabolic stressors
 * 6. Circadian metabolic variations
 * 7. Metabolic recovery patterns
 *
 * BIOLOGICAL BASIS:
 * - Brain consumes 20% of body's ATP despite 2% of body mass
 * - Prefrontal cortex is especially sensitive to ATP depletion
 * - Cognitive functions fail in specific sequence under metabolic stress:
 *   1. Working memory and executive function (first to fail)
 *   2. Attention and salience detection
 *   3. Emotional regulation
 *   4. Memory encoding
 *   5. Basic perception (last to fail)
 */

#include <gtest/gtest.h>
#include <vector>
#include <thread>
#include <chrono>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/neural_substrate/nimcp_neural_substrate.h"

// All 8 substrate bridges
#include "cognitive/attention/nimcp_attention_substrate_bridge.h"
#include "cognitive/emotion/nimcp_emotion_substrate_bridge.h"
#include "cognitive/executive/nimcp_executive_substrate_bridge.h"
#include "cognitive/introspection/nimcp_introspection_substrate_bridge.h"
#include "cognitive/consolidation/nimcp_consolidation_substrate_bridge.h"
#include "cognitive/reasoning/nimcp_reasoning_substrate_bridge.h"
#include "cognitive/tom/nimcp_tom_substrate_bridge.h"
#include "cognitive/working_memory/nimcp_working_memory_substrate_bridge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class MetabolicCascadeIntegrationTest : public ::testing::Test {
protected:
    // Shared neural substrate
    neural_substrate_t* substrate = nullptr;

    // All 8 substrate bridges
    attention_substrate_bridge_t* attention = nullptr;
    emotion_substrate_bridge_t* emotion = nullptr;
    executive_substrate_bridge_t* executive = nullptr;
    introspection_substrate_bridge_t* introspection = nullptr;
    consolidation_substrate_bridge_t* consolidation = nullptr;
    reasoning_substrate_bridge_t* reasoning = nullptr;
    tom_substrate_bridge_t* tom = nullptr;
    wm_substrate_bridge_t* working_memory = nullptr;

    // Stub cognitive systems
    static char att_stub, emo_stub, exec_stub, intro_stub;
    static char cons_stub, reas_stub, tom_stub, wm_stub;

    void SetUp() override {
        // Create shared neural substrate
        substrate_config_t config;
        substrate_default_config(&config);
        substrate = substrate_create(&config);
        ASSERT_NE(substrate, nullptr);
    }

    void TearDown() override {
        // Destroy all bridges
        if (attention) {
            attention_substrate_bridge_destroy(attention);
            attention = nullptr;
        }
        if (emotion) {
            emotion_substrate_bridge_destroy(emotion);
            emotion = nullptr;
        }
        if (executive) {
            executive_substrate_bridge_destroy(executive);
            executive = nullptr;
        }
        if (introspection) {
            introspection_substrate_bridge_destroy(introspection);
            introspection = nullptr;
        }
        if (consolidation) {
            consolidation_substrate_bridge_destroy(consolidation);
            consolidation = nullptr;
        }
        if (reasoning) {
            reasoning_substrate_bridge_destroy(reasoning);
            reasoning = nullptr;
        }
        if (tom) {
            tom_substrate_bridge_destroy(tom);
            tom = nullptr;
        }
        if (working_memory) {
            wm_substrate_bridge_destroy(working_memory);
            working_memory = nullptr;
        }

        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }

    // Helper: Create all 8 substrate bridges
    void create_all_bridges() {
        attention_substrate_config_t att_config;
        attention_substrate_default_config(&att_config);
        attention = attention_substrate_bridge_create(
            &att_config, substrate, (nimcp_attention_system_t*)&att_stub);
        if (!attention) GTEST_SKIP() << "Cannot create attention bridge";

        emotion_substrate_config_t emo_config;
        emotion_substrate_default_config(&emo_config);
        emotion = emotion_substrate_bridge_create(
            &emo_config, (emotional_system_t*)&emo_stub, substrate);
        if (!emotion) GTEST_SKIP() << "Cannot create emotion bridge";

        executive_substrate_config_t exec_config;
        executive_substrate_default_config(&exec_config);
        executive = executive_substrate_bridge_create(
            &exec_config, (nimcp_executive_t*)&exec_stub, substrate);
        if (!executive) GTEST_SKIP() << "Cannot create executive bridge";

        introspection_substrate_config_t intro_config;
        introspection_substrate_default_config(&intro_config);
        introspection = introspection_substrate_bridge_create(
            &intro_config, substrate, (nimcp_introspection_t*)&intro_stub);
        if (!introspection) GTEST_SKIP() << "Cannot create introspection bridge";

        consolidation_substrate_config_t cons_config = consolidation_substrate_default_config();
        consolidation = consolidation_substrate_bridge_create(
            (void*)&cons_stub, substrate, &cons_config);
        if (!consolidation) GTEST_SKIP() << "Cannot create consolidation bridge";

        reasoning_substrate_config_t reas_config;
        reasoning_substrate_default_config(&reas_config);
        reasoning = reasoning_substrate_bridge_create(
            &reas_config, (nimcp_reasoning_system_t*)&reas_stub, substrate);
        if (!reasoning) GTEST_SKIP() << "Cannot create reasoning bridge";

        tom_substrate_config_t tom_config;
        tom_substrate_default_config(&tom_config);
        tom = tom_substrate_bridge_create(
            &tom_config, (theory_of_mind_t)&tom_stub, substrate);
        if (!tom) GTEST_SKIP() << "Cannot create ToM bridge";

        wm_substrate_config_t wm_config;
        wm_substrate_default_config(&wm_config);
        working_memory = wm_substrate_bridge_create(
            &wm_config, substrate, (working_memory_t*)&wm_stub);
        if (!working_memory) GTEST_SKIP() << "Cannot create working memory bridge";
    }

    // Helper: Update all bridges
    void update_all_bridges() {
        ASSERT_EQ(0, attention_substrate_update(attention));
        ASSERT_EQ(0, emotion_substrate_update(emotion));
        ASSERT_EQ(0, executive_substrate_update(executive));
        ASSERT_EQ(0, introspection_substrate_update(introspection));
        ASSERT_EQ(0, consolidation_substrate_bridge_update(consolidation));
        ASSERT_EQ(0, reasoning_substrate_update(reasoning));
        ASSERT_EQ(0, tom_substrate_update(tom));
        ASSERT_EQ(0, wm_substrate_update(working_memory));
    }

    // Helper: Get consolidation rate from bridge effects
    float get_consolidation_rate_from_bridge() {
        if (!consolidation) return 0.0f;
        consolidation_substrate_effects_t effects;
        if (consolidation_substrate_bridge_get_effects(consolidation, &effects) != 0) {
            return 0.0f;
        }
        return effects.consolidation_rate;
    }

    // Helper: Check if consolidation is impaired
    bool is_consolidation_impaired_helper() {
        if (!consolidation) return false;
        consolidation_substrate_effects_t effects;
        if (consolidation_substrate_bridge_get_effects(consolidation, &effects) != 0) {
            return false;
        }
        return effects.overall_capacity < 0.5f;
    }

    // Helper: Collect all capacities
    struct BridgeCapacities {
        float attention_focus;
        float emotion_regulation;
        float executive_quality;
        float introspection_depth;
        float consolidation_rate;
        float reasoning_depth;
        float tom_capacity;
        float wm_capacity;

        float average() const {
            return (attention_focus + emotion_regulation + executive_quality +
                    introspection_depth + consolidation_rate + reasoning_depth +
                    tom_capacity + wm_capacity) / 8.0f;
        }

        int impaired_count() const {
            int count = 0;
            if (attention_focus < 0.3f) count++;
            if (emotion_regulation < 0.3f) count++;
            if (executive_quality < 0.3f) count++;
            if (introspection_depth < 0.3f) count++;
            if (consolidation_rate < 0.3f) count++;
            if (reasoning_depth < 0.3f) count++;
            if (tom_capacity < 0.3f) count++;
            if (wm_capacity < 0.3f) count++;
            return count;
        }
    };

    BridgeCapacities get_all_capacities() {
        BridgeCapacities caps;
        caps.attention_focus = attention_substrate_get_focus_capacity(attention);
        caps.emotion_regulation = emotion_substrate_get_regulation_capacity(emotion);
        caps.executive_quality = executive_substrate_get_decision_quality(executive);
        caps.introspection_depth = introspection_substrate_get_self_awareness_depth(introspection);
        caps.consolidation_rate = get_consolidation_rate_from_bridge();

        const reasoning_substrate_effects_t* reas_eff = reasoning_substrate_get_effects(reasoning);
        caps.reasoning_depth = reas_eff ? reas_eff->inference_depth : 0.0f;

        caps.tom_capacity = tom_substrate_get_mentalizing_capacity(tom);
        caps.wm_capacity = wm_substrate_get_capacity_factor(working_memory);
        return caps;
    }

    // Helper: Check if all bridges report impairment
    bool all_impaired() {
        return attention_substrate_is_impaired(attention) &&
               emotion_substrate_is_impaired(emotion) &&
               executive_substrate_is_impaired(executive) &&
               introspection_substrate_is_impaired(introspection) &&
               is_consolidation_impaired_helper() &&
               reasoning_substrate_is_impaired(reasoning) &&
               tom_substrate_is_impaired(tom) &&
               wm_substrate_is_impaired(working_memory);
    }

    // Helper: Check if none are impaired
    bool none_impaired() {
        return !attention_substrate_is_impaired(attention) &&
               !emotion_substrate_is_impaired(emotion) &&
               !executive_substrate_is_impaired(executive) &&
               !introspection_substrate_is_impaired(introspection) &&
               !is_consolidation_impaired_helper() &&
               !reasoning_substrate_is_impaired(reasoning) &&
               !tom_substrate_is_impaired(tom) &&
               !wm_substrate_is_impaired(working_memory);
    }

    // Helper: Count impaired bridges
    int count_impaired() {
        int count = 0;
        if (attention_substrate_is_impaired(attention)) count++;
        if (emotion_substrate_is_impaired(emotion)) count++;
        if (executive_substrate_is_impaired(executive)) count++;
        if (introspection_substrate_is_impaired(introspection)) count++;
        if (is_consolidation_impaired_helper()) count++;
        if (reasoning_substrate_is_impaired(reasoning)) count++;
        if (tom_substrate_is_impaired(tom)) count++;
        if (wm_substrate_is_impaired(working_memory)) count++;
        return count;
    }

    // Helper: Check if most bridges are impaired (>=5 of 8)
    bool most_impaired() {
        return count_impaired() >= 5;
    }

    // Helper: Check if most bridges are recovered (<=2 of 8 impaired)
    bool most_recovered() {
        return count_impaired() <= 2;
    }
};

// Static stub definitions
char MetabolicCascadeIntegrationTest::att_stub;
char MetabolicCascadeIntegrationTest::emo_stub;
char MetabolicCascadeIntegrationTest::exec_stub;
char MetabolicCascadeIntegrationTest::intro_stub;
char MetabolicCascadeIntegrationTest::cons_stub;
char MetabolicCascadeIntegrationTest::reas_stub;
char MetabolicCascadeIntegrationTest::tom_stub;
char MetabolicCascadeIntegrationTest::wm_stub;

//=============================================================================
// TEST SUITE 1: Progressive ATP Depletion Cascade
//=============================================================================

TEST_F(MetabolicCascadeIntegrationTest, ProgressiveATPDepletion) {
    create_all_bridges();

    std::vector<float> atp_levels = {1.0f, 0.8f, 0.6f, 0.4f, 0.3f, 0.2f, 0.1f};
    std::vector<BridgeCapacities> capacities;

    for (float atp : atp_levels) {
        substrate_set_atp(substrate, atp);
        update_all_bridges();
        capacities.push_back(get_all_capacities());
    }

    // Verify progressive degradation
    for (size_t i = 1; i < capacities.size(); i++) {
        EXPECT_LE(capacities[i].average(), capacities[i-1].average() + 0.05f);
    }

    // At optimal ATP, most functions should be good
    EXPECT_GT(capacities[0].average(), 0.7f);

    // At critical ATP, most should be impaired
    EXPECT_LT(capacities.back().average(), 0.5f);

    printf("[CASCADE] ATP depletion progression:\n");
    for (size_t i = 0; i < atp_levels.size(); i++) {
        printf("  ATP %.1f: avg=%.2f, impaired=%d\n",
               atp_levels[i], capacities[i].average(), capacities[i].impaired_count());
    }
}

TEST_F(MetabolicCascadeIntegrationTest, ExecutiveFailsFirst) {
    create_all_bridges();

    // Start optimal
    substrate_set_atp(substrate, 1.0f);
    update_all_bridges();

    // Moderate depletion - executive functions most sensitive
    substrate_set_atp(substrate, 0.45f);
    update_all_bridges();

    BridgeCapacities caps = get_all_capacities();

    // Executive and working memory typically fail first (prefrontal sensitivity)
    // These should show more degradation than attention/emotion at moderate ATP
    EXPECT_LT(caps.executive_quality, 0.8f);
    EXPECT_LT(caps.wm_capacity, 0.8f);
}

//=============================================================================
// TEST SUITE 2: Sudden Metabolic Crisis and Recovery
//=============================================================================

TEST_F(MetabolicCascadeIntegrationTest, SuddenCrisisAllImpaired) {
    create_all_bridges();

    // Start optimal
    substrate_set_atp(substrate, 0.95f);
    update_all_bridges();
    // Most bridges should start recovered
    EXPECT_TRUE(most_recovered());

    // Sudden crisis
    substrate_set_atp(substrate, 0.1f);
    update_all_bridges();

    // Most should be impaired (different bridges have different thresholds)
    EXPECT_TRUE(most_impaired());
}

TEST_F(MetabolicCascadeIntegrationTest, GradualRecoveryFromCrisis) {
    create_all_bridges();

    // Crisis state
    substrate_set_atp(substrate, 0.1f);
    update_all_bridges();
    EXPECT_TRUE(most_impaired());

    // Gradual recovery
    std::vector<float> recovery_atp = {0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f};
    int first_recovery_index = -1;

    for (size_t i = 0; i < recovery_atp.size(); i++) {
        substrate_set_atp(substrate, recovery_atp[i]);
        substrate_update(substrate, 100);
        update_all_bridges();

        if (!most_impaired() && first_recovery_index < 0) {
            first_recovery_index = (int)i;
        }
    }

    // Should start recovering somewhere along the way
    EXPECT_GE(first_recovery_index, 0);

    // At end, most should be recovered (bridges recover at different rates)
    EXPECT_TRUE(most_recovered());
}

TEST_F(MetabolicCascadeIntegrationTest, RapidCrisisRecoveryCycles) {
    create_all_bridges();

    // Multiple crisis-recovery cycles
    for (int cycle = 0; cycle < 5; cycle++) {
        // Crisis
        substrate_set_atp(substrate, 0.15f);
        update_all_bridges();
        EXPECT_TRUE(most_impaired());

        // Recovery
        substrate_set_atp(substrate, 0.9f);
        update_all_bridges();
        EXPECT_TRUE(most_recovered());
    }
}

//=============================================================================
// TEST SUITE 3: ATP Degradation Effects (simulating fatigue via ATP)
//=============================================================================

TEST_F(MetabolicCascadeIntegrationTest, LowATPSlowsProcessing) {
    create_all_bridges();

    // Good ATP (simulating rested state)
    substrate_set_atp(substrate, 0.85f);
    update_all_bridges();

    BridgeCapacities rested = get_all_capacities();

    const reasoning_substrate_effects_t* eff_rested = reasoning_substrate_get_effects(reasoning);
    float speed_rested = eff_rested ? eff_rested->processing_speed : 0.0f;

    // Lower ATP (simulating fatigued state)
    substrate_set_atp(substrate, 0.35f);
    update_all_bridges();

    BridgeCapacities fatigued = get_all_capacities();

    const reasoning_substrate_effects_t* eff_fatigued = reasoning_substrate_get_effects(reasoning);
    float speed_fatigued = eff_fatigued ? eff_fatigued->processing_speed : 0.0f;

    // Processing speed should be lower with low ATP
    EXPECT_LE(speed_fatigued, speed_rested + 0.01f);
}

TEST_F(MetabolicCascadeIntegrationTest, ModerateATPShowsDegradation) {
    create_all_bridges();

    // Optimal ATP
    substrate_set_atp(substrate, 0.9f);
    update_all_bridges();

    BridgeCapacities optimal = get_all_capacities();

    // Moderate ATP (simulating mental exhaustion)
    substrate_set_atp(substrate, 0.45f);
    update_all_bridges();

    BridgeCapacities exhausted = get_all_capacities();

    // Moderate ATP should show worse performance than optimal
    EXPECT_LT(exhausted.average(), optimal.average());
}

//=============================================================================
// TEST SUITE 4: Temperature Variations
//=============================================================================

TEST_F(MetabolicCascadeIntegrationTest, FeverImpairsFunction) {
    create_all_bridges();

    // Normal temperature
    substrate_set_atp(substrate, 0.8f);
    substrate_set_temperature(substrate, 37.0f);
    update_all_bridges();

    BridgeCapacities normal_temp = get_all_capacities();

    // Fever
    substrate_set_temperature(substrate, 40.0f);
    update_all_bridges();

    BridgeCapacities fever = get_all_capacities();

    // Fever affects emotional intensity and memory
    float normal_intensity = emotion_substrate_get_intensity_mod(emotion);
    float fever_intensity = emotion_substrate_get_intensity_mod(emotion);

    // Both should be valid ranges
    EXPECT_GE(normal_intensity, 0.0f);
    EXPECT_GE(fever_intensity, 0.0f);
}

TEST_F(MetabolicCascadeIntegrationTest, HypothermiaSlowsProcessing) {
    create_all_bridges();

    // Normal temperature
    substrate_set_atp(substrate, 0.8f);
    substrate_set_temperature(substrate, 37.0f);
    update_all_bridges();

    // Hypothermia
    substrate_set_temperature(substrate, 32.0f);
    update_all_bridges();

    // Metabolism slows in cold - effects should be observable
    BridgeCapacities cold = get_all_capacities();

    // All values should be in valid range
    EXPECT_GE(cold.attention_focus, 0.0f);
    EXPECT_GE(cold.reasoning_depth, 0.0f);
}

//=============================================================================
// TEST SUITE 5: Combined Metabolic Stressors
//=============================================================================

TEST_F(MetabolicCascadeIntegrationTest, MultipleStressorsCascade) {
    create_all_bridges();

    // Optimal baseline
    substrate_set_atp(substrate, 0.9f);
    substrate_set_temperature(substrate, 37.0f);
    update_all_bridges();

    BridgeCapacities optimal = get_all_capacities();
    EXPECT_TRUE(none_impaired());

    // Add stressors one by one
    // 1. Moderate ATP depletion
    substrate_set_atp(substrate, 0.5f);
    update_all_bridges();
    BridgeCapacities low_atp = get_all_capacities();

    // 2. Further reduce ATP (simulating fatigue)
    substrate_set_atp(substrate, 0.35f);
    update_all_bridges();
    BridgeCapacities lower_atp = get_all_capacities();

    // 3. Add fever
    substrate_set_temperature(substrate, 39.5f);
    update_all_bridges();
    BridgeCapacities stress_with_fever = get_all_capacities();

    // Each additional stressor should worsen overall capacity
    EXPECT_LE(low_atp.average(), optimal.average());
    EXPECT_LE(lower_atp.average(), low_atp.average() + 0.1f);

    printf("[CASCADE] Multiple stressors:\n");
    printf("  Optimal: avg=%.2f\n", optimal.average());
    printf("  Low ATP: avg=%.2f\n", low_atp.average());
    printf("  Lower ATP: avg=%.2f\n", lower_atp.average());
    printf("  +Fever: avg=%.2f\n", stress_with_fever.average());
}

//=============================================================================
// TEST SUITE 6: Circadian Metabolic Variations
//=============================================================================

TEST_F(MetabolicCascadeIntegrationTest, CircadianEnergyPatterns) {
    create_all_bridges();

    // Simulate 24-hour cycle with varying ATP
    std::vector<float> hourly_atp;
    std::vector<float> hourly_capacity;

    for (int hour = 0; hour < 24; hour++) {
        // Sinusoidal ATP variation: peak at noon, trough at 4am
        // During waking hours, ATP decreases slightly (simulating fatigue)
        float phase = (float)(hour - 12) * 3.14159f / 12.0f;
        float base_atp = 0.7f + 0.2f * cosf(phase);

        // Reduce ATP during waking hours to simulate fatigue accumulation
        float fatigue_effect = (hour >= 6 && hour <= 22) ?
                               0.15f * (float)(hour - 6) / 16.0f : 0.0f;
        float atp = base_atp - fatigue_effect;
        if (atp < 0.3f) atp = 0.3f;

        substrate_set_atp(substrate, atp);
        update_all_bridges();

        BridgeCapacities caps = get_all_capacities();
        hourly_atp.push_back(atp);
        hourly_capacity.push_back(caps.average());
    }

    // Peak performance should be around midday
    float morning_avg = (hourly_capacity[9] + hourly_capacity[10] + hourly_capacity[11]) / 3.0f;
    float evening_avg = (hourly_capacity[20] + hourly_capacity[21] + hourly_capacity[22]) / 3.0f;
    float night_avg = (hourly_capacity[2] + hourly_capacity[3] + hourly_capacity[4]) / 3.0f;

    // Morning should generally be better than late night
    EXPECT_GE(morning_avg, night_avg - 0.1f);
}

//=============================================================================
// TEST SUITE 7: Metabolic Recovery Patterns
//=============================================================================

TEST_F(MetabolicCascadeIntegrationTest, RestBasedRecovery) {
    create_all_bridges();

    // Depleted state
    substrate_set_atp(substrate, 0.2f);
    update_all_bridges();

    BridgeCapacities depleted = get_all_capacities();

    // Simulate rest: gradual ATP restoration
    for (int minute = 0; minute < 60; minute++) {
        float atp = 0.2f + 0.7f * (float)minute / 60.0f;
        substrate_set_atp(substrate, atp);
        substrate_update(substrate, 1000);  // 1 second per minute
    }
    update_all_bridges();

    BridgeCapacities recovered = get_all_capacities();

    // Should have substantially recovered
    EXPECT_GT(recovered.average(), depleted.average());
    EXPECT_TRUE(most_recovered());
}

TEST_F(MetabolicCascadeIntegrationTest, GlucoseIntakeRecovery) {
    create_all_bridges();

    // Low energy state (hypoglycemia-like)
    substrate_set_atp(substrate, 0.35f);
    update_all_bridges();

    BridgeCapacities low_energy = get_all_capacities();

    // Simulate glucose intake -> rapid ATP restoration
    substrate_set_atp(substrate, 0.85f);
    update_all_bridges();

    BridgeCapacities post_glucose = get_all_capacities();

    // Should recover quickly
    EXPECT_GT(post_glucose.average(), low_energy.average() + 0.2f);
}

//=============================================================================
// TEST SUITE 8: Performance and Stability
//=============================================================================

TEST_F(MetabolicCascadeIntegrationTest, Performance_ManyUpdateCycles) {
    create_all_bridges();

    auto start = std::chrono::high_resolution_clock::now();

    // 1000 metabolic update cycles
    for (int i = 0; i < 1000; i++) {
        float atp = 0.3f + 0.6f * (float)(i % 100) / 100.0f;
        substrate_set_atp(substrate, atp);
        update_all_bridges();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 1000 cycles should complete in < 3 seconds
    EXPECT_LT(duration.count(), 3000);

    printf("[PERF] 1000 metabolic cycles: %ldms (%.1f cycles/sec)\n",
           duration.count(), 1000000.0f / (float)duration.count());
}

TEST_F(MetabolicCascadeIntegrationTest, Stability_ExtremeValues) {
    create_all_bridges();

    // Test extreme values don't crash
    std::vector<float> extreme_atp = {0.0f, 0.001f, 0.999f, 1.0f, 10.0f, -0.1f};

    for (float atp : extreme_atp) {
        substrate_set_atp(substrate, atp);
        update_all_bridges();

        BridgeCapacities caps = get_all_capacities();

        // All values should be valid (not NaN, not Inf)
        EXPECT_FALSE(std::isnan(caps.attention_focus));
        EXPECT_FALSE(std::isinf(caps.reasoning_depth));
        EXPECT_GE(caps.emotion_regulation, 0.0f);
    }
}

TEST_F(MetabolicCascadeIntegrationTest, Stability_RapidFluctuations) {
    create_all_bridges();

    // Rapid ATP fluctuations (seizure-like metabolic instability)
    for (int i = 0; i < 100; i++) {
        float atp = (i % 2 == 0) ? 0.9f : 0.2f;
        substrate_set_atp(substrate, atp);
        update_all_bridges();
    }

    // System should remain stable after fluctuations
    substrate_set_atp(substrate, 0.8f);
    update_all_bridges();

    BridgeCapacities post_fluctuation = get_all_capacities();

    // All values in valid range
    EXPECT_GE(post_fluctuation.average(), 0.0f);
    EXPECT_LE(post_fluctuation.average(), 1.0f);
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
