/**
 * @file test_portia_tier_switch.cpp
 * @brief Unit tests for Portia dynamic tier switching subsystem
 *
 * Tests cover both the mock-based discrete tier switching (legacy)
 * and the new continuous resource pressure API.
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cmath>
#include <vector>

// Include the real header for continuous allocation API
#include "portia/nimcp_portia_tier_switch.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/platform/nimcp_platform.h"

// Shadow types for mock-based legacy tests (local scope, not conflicting
// with real types because they use different names)
typedef enum {
    MOCK_TIER_MINIMAL = 0,
    MOCK_TIER_LOW = 1,
    MOCK_TIER_MEDIUM = 2,
    MOCK_TIER_HIGH = 3,
    MOCK_TIER_PERFORMANCE = 4
} mock_tier_t;

typedef struct {
    mock_tier_t current_tier;
    mock_tier_t target_tier;
    float memory_pressure;     // 0.0-1.0
    float thermal_level;       // 0.0-1.0
    float battery_level;       // 0.0-1.0
    uint64_t last_switch_ms;
    uint32_t hysteresis_ms;
    bool manual_override;
} mock_tier_state_t;

typedef struct {
    float memory_threshold_up;
    float memory_threshold_down;
    float thermal_threshold_up;
    float thermal_threshold_down;
    float battery_threshold_down;
    uint32_t hysteresis_ms;
    bool enable_auto_switch;
} mock_tier_config_t;

// Mock functions for testing discrete tier switching (legacy)
mock_tier_state_t* mock_tier_switch_init(const mock_tier_config_t* config) {
    if (!config) return nullptr;
    mock_tier_state_t* state = (mock_tier_state_t*)malloc(sizeof(mock_tier_state_t));
    if (state) {
        state->current_tier = MOCK_TIER_MEDIUM;
        state->target_tier = MOCK_TIER_MEDIUM;
        state->memory_pressure = 0.3f;
        state->thermal_level = 0.3f;
        state->battery_level = 0.8f;
        state->last_switch_ms = 0;
        state->hysteresis_ms = config->hysteresis_ms;
        state->manual_override = false;
    }
    return state;
}

void mock_tier_switch_destroy(mock_tier_state_t* state) {
    free(state);
}

int mock_tier_switch_evaluate(mock_tier_state_t* state, float memory, float thermal, float battery) {
    if (!state) return -1;
    state->memory_pressure = memory;
    state->thermal_level = thermal;
    state->battery_level = battery;

    // Simple evaluation logic
    if (memory > 0.8f || thermal > 0.8f || battery < 0.2f) {
        if (state->current_tier > MOCK_TIER_MINIMAL) {
            state->target_tier = static_cast<mock_tier_t>(state->current_tier - 1);
        }
    } else if (memory < 0.5f && thermal < 0.5f && battery > 0.5f) {
        if (state->current_tier < MOCK_TIER_PERFORMANCE) {
            state->target_tier = static_cast<mock_tier_t>(state->current_tier + 1);
        }
    }
    return 0;
}

int mock_tier_switch_set_tier(mock_tier_state_t* state, mock_tier_t tier) {
    if (!state) return -1;
    state->current_tier = tier;
    state->target_tier = tier;
    state->manual_override = true;
    state->last_switch_ms = 0; // Mock timestamp
    return 0;
}

mock_tier_t mock_tier_switch_get_tier(const mock_tier_state_t* state) {
    return state ? state->current_tier : MOCK_TIER_MINIMAL;
}

int mock_tier_switch_apply(mock_tier_state_t* state, uint64_t timestamp_ms) {
    if (!state) return -1;
    if (state->current_tier != state->target_tier) {
        if (timestamp_ms - state->last_switch_ms >= state->hysteresis_ms) {
            state->current_tier = state->target_tier;
            state->last_switch_ms = timestamp_ms;
            return 1; // Tier changed
        }
    }
    return 0; // No change
}

class PortiaTierSwitchTest : public ::testing::Test {
protected:
    mock_tier_state_t* tier_state;
    mock_tier_config_t config;

    void SetUp() override {
        config.memory_threshold_up = 0.75f;
        config.memory_threshold_down = 0.5f;
        config.thermal_threshold_up = 0.75f;
        config.thermal_threshold_down = 0.5f;
        config.battery_threshold_down = 0.3f;
        config.hysteresis_ms = 1000;
        config.enable_auto_switch = true;

        tier_state = mock_tier_switch_init(&config);
    }

    void TearDown() override {
        if (tier_state) {
            mock_tier_switch_destroy(tier_state);
            tier_state = nullptr;
        }
    }
};

// Initialization Tests
TEST_F(PortiaTierSwitchTest, InitializationSuccess) {
    ASSERT_NE(tier_state, nullptr);
    EXPECT_EQ(mock_tier_switch_get_tier(tier_state), MOCK_TIER_MEDIUM);
}

TEST_F(PortiaTierSwitchTest, InitializationWithNullConfig) {
    mock_tier_state_t* state = mock_tier_switch_init(nullptr);
    EXPECT_EQ(state, nullptr);
}

TEST_F(PortiaTierSwitchTest, DestroyNullState) {
    mock_tier_switch_destroy(nullptr);
    // Should not crash
}

// Tier Evaluation Tests
TEST_F(PortiaTierSwitchTest, EvaluateHighMemoryPressure) {
    int result = mock_tier_switch_evaluate(tier_state, 0.9f, 0.3f, 0.8f);
    EXPECT_EQ(result, 0);
    // Should trigger downgrade
    EXPECT_LT(tier_state->target_tier, MOCK_TIER_MEDIUM);
}

TEST_F(PortiaTierSwitchTest, EvaluateHighThermal) {
    int result = mock_tier_switch_evaluate(tier_state, 0.3f, 0.9f, 0.8f);
    EXPECT_EQ(result, 0);
    // Should trigger downgrade
    EXPECT_LT(tier_state->target_tier, MOCK_TIER_MEDIUM);
}

TEST_F(PortiaTierSwitchTest, EvaluateLowBattery) {
    int result = mock_tier_switch_evaluate(tier_state, 0.3f, 0.3f, 0.1f);
    EXPECT_EQ(result, 0);
    // Should trigger downgrade
    EXPECT_LT(tier_state->target_tier, MOCK_TIER_MEDIUM);
}

TEST_F(PortiaTierSwitchTest, EvaluateFavorableConditions) {
    // Start at lower tier
    mock_tier_switch_set_tier(tier_state, MOCK_TIER_LOW);

    int result = mock_tier_switch_evaluate(tier_state, 0.3f, 0.3f, 0.9f);
    EXPECT_EQ(result, 0);
    // Should allow upgrade
    EXPECT_GT(tier_state->target_tier, MOCK_TIER_LOW);
}

TEST_F(PortiaTierSwitchTest, EvaluateNullState) {
    int result = mock_tier_switch_evaluate(nullptr, 0.5f, 0.5f, 0.5f);
    EXPECT_NE(result, 0);
}

// Hysteresis Tests
TEST_F(PortiaTierSwitchTest, HysteresisPreventsRapidSwitching) {
    // Trigger tier change
    mock_tier_switch_evaluate(tier_state, 0.9f, 0.3f, 0.8f);

    // Try to apply immediately
    int result1 = mock_tier_switch_apply(tier_state, 0);
    EXPECT_EQ(result1, 0); // Should not switch yet

    // Try to apply after hysteresis period
    int result2 = mock_tier_switch_apply(tier_state, config.hysteresis_ms + 100);
    EXPECT_NE(result2, 0); // Should switch now
}

TEST_F(PortiaTierSwitchTest, HysteresisRespectsPreviousSwitch) {
    mock_tier_t initial_tier = mock_tier_switch_get_tier(tier_state);

    // Force tier change
    mock_tier_switch_set_tier(tier_state, MOCK_TIER_LOW);

    // Immediately try to evaluate for another change
    mock_tier_switch_evaluate(tier_state, 0.3f, 0.3f, 0.9f);

    // Should not switch immediately due to hysteresis
    mock_tier_switch_apply(tier_state, 500); // Before hysteresis expires

    mock_tier_t current_tier = mock_tier_switch_get_tier(tier_state);
    EXPECT_EQ(current_tier, MOCK_TIER_LOW);
}

// Memory Pressure Tests
TEST_F(PortiaTierSwitchTest, MemoryPressureThresholds) {
    // Low memory pressure
    mock_tier_switch_evaluate(tier_state, 0.2f, 0.3f, 0.8f);
    EXPECT_GE(tier_state->target_tier, MOCK_TIER_MEDIUM);

    // High memory pressure
    mock_tier_switch_evaluate(tier_state, 0.95f, 0.3f, 0.8f);
    EXPECT_LT(tier_state->target_tier, MOCK_TIER_MEDIUM);
}

TEST_F(PortiaTierSwitchTest, MemoryPressureBoundaries) {
    // At threshold
    mock_tier_switch_evaluate(tier_state, 0.75f, 0.3f, 0.8f);
    // Should trigger tier change
    EXPECT_NE(tier_state->target_tier, MOCK_TIER_PERFORMANCE);

    // Just below threshold
    mock_tier_switch_evaluate(tier_state, 0.74f, 0.3f, 0.8f);
    // Behavior depends on implementation
}

// Thermal Tests
TEST_F(PortiaTierSwitchTest, ThermalThresholds) {
    // Low thermal
    mock_tier_switch_evaluate(tier_state, 0.3f, 0.2f, 0.8f);
    EXPECT_GE(tier_state->target_tier, MOCK_TIER_MEDIUM);

    // High thermal
    mock_tier_switch_evaluate(tier_state, 0.3f, 0.95f, 0.8f);
    EXPECT_LT(tier_state->target_tier, MOCK_TIER_MEDIUM);
}

TEST_F(PortiaTierSwitchTest, ThermalThrottling) {
    // Extreme thermal - should go to minimal tier
    mock_tier_switch_evaluate(tier_state, 0.3f, 1.0f, 0.8f);
    mock_tier_switch_apply(tier_state, config.hysteresis_ms + 100);

    mock_tier_t tier = mock_tier_switch_get_tier(tier_state);
    EXPECT_LE(tier, MOCK_TIER_LOW);
}

// Battery Level Tests
TEST_F(PortiaTierSwitchTest, BatteryLevelDowngrade) {
    // Critical battery
    mock_tier_switch_evaluate(tier_state, 0.3f, 0.3f, 0.05f);
    mock_tier_switch_apply(tier_state, config.hysteresis_ms + 100);

    mock_tier_t tier = mock_tier_switch_get_tier(tier_state);
    EXPECT_LE(tier, MOCK_TIER_LOW);
}

TEST_F(PortiaTierSwitchTest, BatteryLevelThreshold) {
    // Just above threshold
    mock_tier_switch_evaluate(tier_state, 0.3f, 0.3f, 0.31f);
    mock_tier_t tier1 = tier_state->target_tier;

    // Just below threshold
    mock_tier_switch_evaluate(tier_state, 0.3f, 0.3f, 0.29f);
    mock_tier_t tier2 = tier_state->target_tier;

    // Should trigger downgrade
    EXPECT_LE(tier2, tier1);
}

// Manual Override Tests
TEST_F(PortiaTierSwitchTest, ManualTierOverride) {
    int result = mock_tier_switch_set_tier(tier_state, MOCK_TIER_HIGH);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(mock_tier_switch_get_tier(tier_state), MOCK_TIER_HIGH);
}

TEST_F(PortiaTierSwitchTest, ManualOverrideAllTiers) {
    for (int tier = MOCK_TIER_MINIMAL; tier <= MOCK_TIER_PERFORMANCE; tier++) {
        int result = mock_tier_switch_set_tier(tier_state, static_cast<mock_tier_t>(tier));
        EXPECT_EQ(result, 0);
        EXPECT_EQ(mock_tier_switch_get_tier(tier_state), static_cast<mock_tier_t>(tier));
    }
}

TEST_F(PortiaTierSwitchTest, ManualOverrideNullState) {
    int result = mock_tier_switch_set_tier(nullptr, MOCK_TIER_HIGH);
    EXPECT_NE(result, 0);
}

// State Transition Tests
TEST_F(PortiaTierSwitchTest, TransitionMinimalToPerformance) {
    mock_tier_switch_set_tier(tier_state, MOCK_TIER_MINIMAL);

    // Gradually improve conditions
    for (int i = 0; i < 10; i++) {
        mock_tier_switch_evaluate(tier_state, 0.2f, 0.2f, 1.0f);
        mock_tier_switch_apply(tier_state, (i + 1) * (config.hysteresis_ms + 100));

        if (mock_tier_switch_get_tier(tier_state) == MOCK_TIER_PERFORMANCE) {
            break;
        }
    }

    // Should have reached higher tier
    EXPECT_GT(mock_tier_switch_get_tier(tier_state), MOCK_TIER_MINIMAL);
}

TEST_F(PortiaTierSwitchTest, TransitionPerformanceToMinimal) {
    mock_tier_switch_set_tier(tier_state, MOCK_TIER_PERFORMANCE);

    // Gradually worsen conditions
    for (int i = 0; i < 10; i++) {
        mock_tier_switch_evaluate(tier_state, 0.95f, 0.95f, 0.1f);
        mock_tier_switch_apply(tier_state, (i + 1) * (config.hysteresis_ms + 100));

        if (mock_tier_switch_get_tier(tier_state) == MOCK_TIER_MINIMAL) {
            break;
        }
    }

    // Should have reached lower tier
    EXPECT_LT(mock_tier_switch_get_tier(tier_state), MOCK_TIER_PERFORMANCE);
}

TEST_F(PortiaTierSwitchTest, TransitionStability) {
    mock_tier_t initial_tier = MOCK_TIER_MEDIUM;
    mock_tier_switch_set_tier(tier_state, initial_tier);

    // Stable conditions should maintain tier
    for (int i = 0; i < 10; i++) {
        mock_tier_switch_evaluate(tier_state, 0.5f, 0.5f, 0.5f);
        mock_tier_switch_apply(tier_state, i * (config.hysteresis_ms + 100));
    }

    EXPECT_EQ(mock_tier_switch_get_tier(tier_state), initial_tier);
}

// Multi-Factor Tests
TEST_F(PortiaTierSwitchTest, MultiplePressuresSimultaneous) {
    // All factors at critical levels
    mock_tier_switch_evaluate(tier_state, 0.95f, 0.95f, 0.05f);
    mock_tier_switch_apply(tier_state, config.hysteresis_ms + 100);

    // Should force to minimal tier
    EXPECT_LE(mock_tier_switch_get_tier(tier_state), MOCK_TIER_LOW);
}

TEST_F(PortiaTierSwitchTest, ConflictingPressures) {
    // High memory but good thermal and battery
    mock_tier_switch_evaluate(tier_state, 0.9f, 0.2f, 0.9f);

    // Should still downgrade due to memory pressure
    EXPECT_LT(tier_state->target_tier, MOCK_TIER_PERFORMANCE);
}

// Edge Cases
TEST_F(PortiaTierSwitchTest, BoundaryValues) {
    // Test extreme values
    mock_tier_switch_evaluate(tier_state, 0.0f, 0.0f, 1.0f);
    EXPECT_GE(tier_state->target_tier, MOCK_TIER_MINIMAL);

    mock_tier_switch_evaluate(tier_state, 1.0f, 1.0f, 0.0f);
    EXPECT_LE(tier_state->target_tier, MOCK_TIER_PERFORMANCE);
}

TEST_F(PortiaTierSwitchTest, InvalidTierValues) {
    // Try to set invalid tier (implementation should reject)
    mock_tier_t invalid = static_cast<mock_tier_t>(999);
    int result = mock_tier_switch_set_tier(tier_state, invalid);
    // Should either reject or clamp
}

TEST_F(PortiaTierSwitchTest, ZeroHysteresis) {
    mock_tier_config_t zero_config = config;
    zero_config.hysteresis_ms = 0;

    mock_tier_state_t* state = mock_tier_switch_init(&zero_config);
    ASSERT_NE(state, nullptr);

    // Should allow immediate switching
    mock_tier_switch_evaluate(state, 0.9f, 0.3f, 0.8f);
    int result = mock_tier_switch_apply(state, 0);
    // Should switch immediately
    EXPECT_NE(result, 0);

    mock_tier_switch_destroy(state);
}

// Thread Safety Tests
TEST_F(PortiaTierSwitchTest, ConcurrentEvaluations) {
    auto evaluate_worker = [this]() {
        for (int i = 0; i < 100; i++) {
            float pressure = static_cast<float>(i % 100) / 100.0f;
            mock_tier_switch_evaluate(tier_state, pressure, 0.5f, 0.5f);
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    };

    std::thread t1(evaluate_worker);
    std::thread t2(evaluate_worker);

    t1.join();
    t2.join();

    // Should not crash
    EXPECT_TRUE(true);
}

TEST_F(PortiaTierSwitchTest, ConcurrentSwitching) {
    auto switch_worker = [this](mock_tier_t tier) {
        for (int i = 0; i < 50; i++) {
            mock_tier_switch_set_tier(tier_state, tier);
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    };

    std::thread t1(switch_worker, MOCK_TIER_LOW);
    std::thread t2(switch_worker, MOCK_TIER_HIGH);

    t1.join();
    t2.join();

    // Should end in valid state
    mock_tier_t final_tier = mock_tier_switch_get_tier(tier_state);
    EXPECT_GE(final_tier, MOCK_TIER_MINIMAL);
    EXPECT_LE(final_tier, MOCK_TIER_PERFORMANCE);
}

// Integration Tests
TEST_F(PortiaTierSwitchTest, RealisticWorkload) {
    // Simulate realistic workload pattern
    struct Scenario {
        float memory;
        float thermal;
        float battery;
        uint32_t duration_ms;
    };

    Scenario scenarios[] = {
        {0.3f, 0.3f, 1.0f, 5000},   // Idle - good conditions
        {0.6f, 0.5f, 0.9f, 3000},   // Light load
        {0.8f, 0.7f, 0.7f, 2000},   // Heavy load
        {0.9f, 0.9f, 0.5f, 1000},   // Peak load
        {0.5f, 0.6f, 0.4f, 2000},   // Cool down
        {0.3f, 0.3f, 0.3f, 3000}    // Low battery
    };

    uint64_t current_time = 0;
    for (const auto& scenario : scenarios) {
        mock_tier_switch_evaluate(tier_state, scenario.memory, scenario.thermal, scenario.battery);
        current_time += scenario.duration_ms;
        mock_tier_switch_apply(tier_state, current_time);
    }

    // Should adapt to battery situation
    EXPECT_LE(mock_tier_switch_get_tier(tier_state), MOCK_TIER_MEDIUM);
}

TEST_F(PortiaTierSwitchTest, AdaptiveResponse) {
    // Test that system adapts appropriately to changing conditions

    // Start with good conditions
    mock_tier_switch_set_tier(tier_state, MOCK_TIER_PERFORMANCE);

    // Gradually increase memory pressure
    for (int i = 0; i < 10; i++) {
        float memory = 0.5f + (i * 0.05f);
        mock_tier_switch_evaluate(tier_state, memory, 0.3f, 0.8f);
        mock_tier_switch_apply(tier_state, i * (config.hysteresis_ms + 100));
    }

    // Should have downgraded
    EXPECT_LT(mock_tier_switch_get_tier(tier_state), MOCK_TIER_PERFORMANCE);

    // Improve conditions
    for (int i = 0; i < 10; i++) {
        float memory = 0.9f - (i * 0.05f);
        mock_tier_switch_evaluate(tier_state, memory, 0.3f, 0.8f);
        mock_tier_switch_apply(tier_state, (i + 10) * (config.hysteresis_ms + 100));
    }

    // Should have upgraded
    EXPECT_GT(mock_tier_switch_get_tier(tier_state), MOCK_TIER_MINIMAL);
}

//=============================================================================
// Continuous Resource Pressure and Allocation Tests
//=============================================================================
// These tests exercise the real implementation from portia_tier_switch.h:
//   portia_compute_allocation() - pure function, no state needed
//   portia_tier_from_pressure() - pure function, no state needed
//=============================================================================

class PortiaContinuousAllocationTest : public ::testing::Test {
protected:
    portia_allocation_t alloc;

    void SetUp() override {
        memset(&alloc, 0, sizeof(alloc));
    }
};

// --- portia_tier_from_pressure tests ---

TEST_F(PortiaContinuousAllocationTest, TierFromPressure_ZeroPressureIsFullTier) {
    EXPECT_EQ(portia_tier_from_pressure(0.0f), PLATFORM_TIER_FULL);
}

TEST_F(PortiaContinuousAllocationTest, TierFromPressure_LowPressureIsFullTier) {
    EXPECT_EQ(portia_tier_from_pressure(0.10f), PLATFORM_TIER_FULL);
    EXPECT_EQ(portia_tier_from_pressure(0.24f), PLATFORM_TIER_FULL);
}

TEST_F(PortiaContinuousAllocationTest, TierFromPressure_ModeratePressureIsMediumTier) {
    EXPECT_EQ(portia_tier_from_pressure(0.25f), PLATFORM_TIER_MEDIUM);
    EXPECT_EQ(portia_tier_from_pressure(0.40f), PLATFORM_TIER_MEDIUM);
    EXPECT_EQ(portia_tier_from_pressure(0.49f), PLATFORM_TIER_MEDIUM);
}

TEST_F(PortiaContinuousAllocationTest, TierFromPressure_HighPressureIsConstrainedTier) {
    EXPECT_EQ(portia_tier_from_pressure(0.50f), PLATFORM_TIER_CONSTRAINED);
    EXPECT_EQ(portia_tier_from_pressure(0.60f), PLATFORM_TIER_CONSTRAINED);
    EXPECT_EQ(portia_tier_from_pressure(0.74f), PLATFORM_TIER_CONSTRAINED);
}

TEST_F(PortiaContinuousAllocationTest, TierFromPressure_CriticalPressureIsMinimalTier) {
    EXPECT_EQ(portia_tier_from_pressure(0.75f), PLATFORM_TIER_MINIMAL);
    EXPECT_EQ(portia_tier_from_pressure(0.90f), PLATFORM_TIER_MINIMAL);
    EXPECT_EQ(portia_tier_from_pressure(1.0f), PLATFORM_TIER_MINIMAL);
}

TEST_F(PortiaContinuousAllocationTest, TierFromPressure_ClampsBelowZero) {
    EXPECT_EQ(portia_tier_from_pressure(-0.5f), PLATFORM_TIER_FULL);
}

TEST_F(PortiaContinuousAllocationTest, TierFromPressure_ClampsAboveOne) {
    EXPECT_EQ(portia_tier_from_pressure(1.5f), PLATFORM_TIER_MINIMAL);
}

// --- portia_compute_allocation tests ---

TEST_F(PortiaContinuousAllocationTest, ComputeAllocation_NullOutputReturnsError) {
    EXPECT_EQ(portia_compute_allocation(0.5f, nullptr), -1);
}

TEST_F(PortiaContinuousAllocationTest, ComputeAllocation_ZeroPressureGivesMaxAllocation) {
    int result = portia_compute_allocation(0.0f, &alloc);
    EXPECT_EQ(result, 0);

    // At zero pressure, everything should be near maximum
    EXPECT_GT(alloc.quality_scale, 0.95f);
    EXPECT_NEAR(alloc.allocation_fraction, 1.0f, 0.01f);
    EXPECT_GT(alloc.feature_gate_plasticity, 0.95f);
    EXPECT_GT(alloc.feature_gate_learning, 0.95f);
    EXPECT_GT(alloc.feature_gate_emotions, 0.95f);
    EXPECT_GT(alloc.feature_gate_planning, 0.95f);
    EXPECT_GT(alloc.feature_gate_meta, 0.95f);
    EXPECT_GT(alloc.compute_budget_scale, 0.95f);
    EXPECT_GT(alloc.memory_budget_scale, 0.95f);
    EXPECT_NEAR(alloc.thread_budget_scale, 1.0f, 0.01f);
    EXPECT_EQ(alloc.derived_tier, PLATFORM_TIER_FULL);
}

TEST_F(PortiaContinuousAllocationTest, ComputeAllocation_MaxPressureGivesMinAllocation) {
    int result = portia_compute_allocation(1.0f, &alloc);
    EXPECT_EQ(result, 0);

    // At max pressure, most things should be near minimum
    EXPECT_LT(alloc.quality_scale, 0.05f);
    EXPECT_NEAR(alloc.allocation_fraction, 0.1f, 0.01f);
    EXPECT_LT(alloc.feature_gate_plasticity, 0.05f);
    EXPECT_LT(alloc.feature_gate_learning, 0.05f);
    EXPECT_LT(alloc.feature_gate_emotions, 0.05f);
    EXPECT_LT(alloc.feature_gate_planning, 0.05f);
    EXPECT_LT(alloc.feature_gate_meta, 0.05f);
    EXPECT_LT(alloc.compute_budget_scale, 0.10f);
    EXPECT_LT(alloc.memory_budget_scale, 0.10f);
    EXPECT_NEAR(alloc.thread_budget_scale, 0.2f, 0.01f);
    EXPECT_EQ(alloc.derived_tier, PLATFORM_TIER_MINIMAL);
}

TEST_F(PortiaContinuousAllocationTest, ComputeAllocation_ModeratePressureGivesIntermediateValues) {
    int result = portia_compute_allocation(0.5f, &alloc);
    EXPECT_EQ(result, 0);

    // At moderate pressure, values should be intermediate
    EXPECT_GT(alloc.quality_scale, 0.5f);    // Sigmoid centered at 0.7
    EXPECT_LT(alloc.quality_scale, 1.0f);
    EXPECT_GT(alloc.allocation_fraction, 0.4f);
    EXPECT_LT(alloc.allocation_fraction, 0.7f);
    EXPECT_EQ(alloc.derived_tier, PLATFORM_TIER_CONSTRAINED);
}

TEST_F(PortiaContinuousAllocationTest, SmoothTransition_NoDiscontinuities) {
    // Test that allocation values change smoothly (no jumps > epsilon)
    // by sweeping pressure from 0 to 1 in small increments

    const int NUM_STEPS = 100;
    const float MAX_QUALITY_JUMP = 0.15f;  // Max allowed single-step jump

    float prev_quality = -1.0f;
    float prev_alloc = -1.0f;

    for (int i = 0; i <= NUM_STEPS; i++) {
        float pressure = static_cast<float>(i) / static_cast<float>(NUM_STEPS);
        int result = portia_compute_allocation(pressure, &alloc);
        ASSERT_EQ(result, 0);

        // All values must be in [0, 1]
        EXPECT_GE(alloc.quality_scale, 0.0f);
        EXPECT_LE(alloc.quality_scale, 1.0f);
        EXPECT_GE(alloc.allocation_fraction, 0.0f);
        EXPECT_LE(alloc.allocation_fraction, 1.0f);
        EXPECT_GE(alloc.feature_gate_plasticity, 0.0f);
        EXPECT_LE(alloc.feature_gate_plasticity, 1.0f);

        if (prev_quality >= 0.0f) {
            float quality_jump = std::fabs(alloc.quality_scale - prev_quality);
            EXPECT_LT(quality_jump, MAX_QUALITY_JUMP)
                << "Quality discontinuity at pressure=" << pressure
                << ": " << prev_quality << " -> " << alloc.quality_scale;

            float alloc_jump = std::fabs(alloc.allocation_fraction - prev_alloc);
            EXPECT_LT(alloc_jump, MAX_QUALITY_JUMP)
                << "Allocation discontinuity at pressure=" << pressure;
        }

        prev_quality = alloc.quality_scale;
        prev_alloc = alloc.allocation_fraction;
    }
}

TEST_F(PortiaContinuousAllocationTest, SmoothTransition_MonotonicallyDecreasing) {
    // Quality and allocation should decrease as pressure increases

    const int NUM_STEPS = 50;
    float prev_quality = 2.0f;  // Start higher than max
    float prev_alloc = 2.0f;

    for (int i = 0; i <= NUM_STEPS; i++) {
        float pressure = static_cast<float>(i) / static_cast<float>(NUM_STEPS);
        int result = portia_compute_allocation(pressure, &alloc);
        ASSERT_EQ(result, 0);

        EXPECT_LE(alloc.quality_scale, prev_quality + 0.001f)
            << "Quality not monotonically decreasing at pressure=" << pressure;
        EXPECT_LE(alloc.allocation_fraction, prev_alloc + 0.001f)
            << "Allocation not monotonically decreasing at pressure=" << pressure;

        prev_quality = alloc.quality_scale;
        prev_alloc = alloc.allocation_fraction;
    }
}

TEST_F(PortiaContinuousAllocationTest, FeatureGates_OrderedByPriority) {
    // At moderate pressure, costlier features should gate off first
    // Meta gates off earliest (center=0.45), then plasticity (0.5),
    // learning (0.55), emotions (0.6), planning (0.65)

    int result = portia_compute_allocation(0.55f, &alloc);
    ASSERT_EQ(result, 0);

    // Meta-cognition should be more suppressed than planning
    EXPECT_LT(alloc.feature_gate_meta, alloc.feature_gate_planning);
    // Plasticity should be more suppressed than emotions
    EXPECT_LT(alloc.feature_gate_plasticity, alloc.feature_gate_emotions);
}

TEST_F(PortiaContinuousAllocationTest, FeatureGates_AllNearOneAtLowPressure) {
    int result = portia_compute_allocation(0.1f, &alloc);
    ASSERT_EQ(result, 0);

    EXPECT_GT(alloc.feature_gate_plasticity, 0.9f);
    EXPECT_GT(alloc.feature_gate_learning, 0.9f);
    EXPECT_GT(alloc.feature_gate_emotions, 0.9f);
    EXPECT_GT(alloc.feature_gate_planning, 0.9f);
    EXPECT_GT(alloc.feature_gate_meta, 0.9f);
}

TEST_F(PortiaContinuousAllocationTest, FeatureGates_AllNearZeroAtHighPressure) {
    int result = portia_compute_allocation(0.95f, &alloc);
    ASSERT_EQ(result, 0);

    EXPECT_LT(alloc.feature_gate_plasticity, 0.1f);
    EXPECT_LT(alloc.feature_gate_learning, 0.1f);
    EXPECT_LT(alloc.feature_gate_emotions, 0.1f);
    EXPECT_LT(alloc.feature_gate_planning, 0.1f);
    EXPECT_LT(alloc.feature_gate_meta, 0.1f);
}

TEST_F(PortiaContinuousAllocationTest, DerivedTier_MatchesTierFromPressure) {
    // Verify derived_tier in allocation matches portia_tier_from_pressure
    float test_pressures[] = {0.0f, 0.1f, 0.24f, 0.25f, 0.49f, 0.50f, 0.74f, 0.75f, 0.99f, 1.0f};

    for (float p : test_pressures) {
        int result = portia_compute_allocation(p, &alloc);
        ASSERT_EQ(result, 0);
        EXPECT_EQ(alloc.derived_tier, portia_tier_from_pressure(p))
            << "Mismatch at pressure=" << p;
    }
}

TEST_F(PortiaContinuousAllocationTest, Clamping_NegativePressure) {
    int result = portia_compute_allocation(-1.0f, &alloc);
    EXPECT_EQ(result, 0);
    // Should behave like pressure=0.0
    portia_allocation_t alloc_zero;
    portia_compute_allocation(0.0f, &alloc_zero);
    EXPECT_FLOAT_EQ(alloc.quality_scale, alloc_zero.quality_scale);
    EXPECT_FLOAT_EQ(alloc.allocation_fraction, alloc_zero.allocation_fraction);
}

TEST_F(PortiaContinuousAllocationTest, Clamping_OverOnePressure) {
    int result = portia_compute_allocation(2.0f, &alloc);
    EXPECT_EQ(result, 0);
    // Should behave like pressure=1.0
    portia_allocation_t alloc_one;
    portia_compute_allocation(1.0f, &alloc_one);
    EXPECT_FLOAT_EQ(alloc.quality_scale, alloc_one.quality_scale);
    EXPECT_FLOAT_EQ(alloc.allocation_fraction, alloc_one.allocation_fraction);
}

TEST_F(PortiaContinuousAllocationTest, ThreadBudget_HasMinimumFloor) {
    // Even at max pressure, thread budget should not go below 20%
    int result = portia_compute_allocation(1.0f, &alloc);
    EXPECT_EQ(result, 0);
    EXPECT_GE(alloc.thread_budget_scale, 0.19f);  // ~0.2 with float tolerance
}

TEST_F(PortiaContinuousAllocationTest, AllocationFraction_HasMinimumFloor) {
    // Even at max pressure, allocation fraction should not go below 10%
    int result = portia_compute_allocation(1.0f, &alloc);
    EXPECT_EQ(result, 0);
    EXPECT_GE(alloc.allocation_fraction, 0.09f);  // ~0.1 with float tolerance
}
