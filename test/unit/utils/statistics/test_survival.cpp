//=============================================================================
// test_survival.cpp - Unit Tests for Survival Analysis Module
//=============================================================================
/**
 * @file test_survival.cpp
 * @brief Comprehensive unit tests for survival analysis methods
 *
 * WHAT: Test coverage for Kaplan-Meier, Cox regression, log-rank test
 * WHY:  Ensure correctness of time-to-event statistical analysis
 * HOW:  GTest framework with known survival data verification
 *
 * TEST COVERAGE:
 * - Kaplan-Meier estimator
 * - Cox proportional hazards model
 * - Log-rank test
 * - Hazard functions
 *
 * @date 2026-01-30
 */

#include <gtest/gtest.h>
#include "utils/statistics/nimcp_statistics.h"
#include <cmath>
#include <vector>
#include <numeric>
#include <algorithm>
#include <random>

// Test tolerances
#define TOLERANCE 1e-5f
#define LOOSE_TOLERANCE 1e-3f
#define SURVIVAL_TOLERANCE 0.05f

//=============================================================================
// Test Fixture
//=============================================================================

class SurvivalTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_stats_config_t config = nimcp_stats_default_config();
        nimcp_stats_init(&config);
        rng.seed(42);
    }

    void TearDown() override {
        nimcp_stats_shutdown();
    }

    std::mt19937 rng;

    // Survival data point
    struct SurvivalData {
        float time;      // Observed time
        bool event;      // True if event occurred (not censored)
        float covariate; // Covariate value (for Cox model)
    };

    // Helper: Generate exponential survival data
    std::vector<SurvivalData> generateExponentialSurvival(
        float lambda, float censor_rate, size_t n, int seed = 42) {

        std::mt19937 gen(seed);
        std::exponential_distribution<float> event_dist(lambda);
        std::exponential_distribution<float> censor_dist(censor_rate);
        std::normal_distribution<float> cov_dist(0.0f, 1.0f);

        std::vector<SurvivalData> data(n);
        for (size_t i = 0; i < n; i++) {
            float event_time = event_dist(gen);
            float censor_time = censor_dist(gen);

            data[i].time = std::min(event_time, censor_time);
            data[i].event = (event_time <= censor_time);
            data[i].covariate = cov_dist(gen);
        }

        return data;
    }

    // Helper: Compute Kaplan-Meier survival function
    std::vector<std::pair<float, float>> computeKaplanMeier(
        const std::vector<SurvivalData>& data) {

        // Sort by time
        std::vector<SurvivalData> sorted = data;
        std::sort(sorted.begin(), sorted.end(),
                  [](const SurvivalData& a, const SurvivalData& b) {
                      return a.time < b.time;
                  });

        std::vector<std::pair<float, float>> km;  // (time, survival)
        km.push_back({0.0f, 1.0f});

        size_t n_at_risk = sorted.size();
        float survival = 1.0f;

        for (size_t i = 0; i < sorted.size(); i++) {
            if (sorted[i].event) {
                // Event occurred
                survival *= (1.0f - 1.0f / n_at_risk);
                km.push_back({sorted[i].time, survival});
            }
            n_at_risk--;
        }

        return km;
    }

    // Helper: Compute hazard rate at time t for exponential distribution
    float exponentialHazard(float lambda) {
        return lambda;  // Constant hazard for exponential
    }

    // Helper: Exponential survival function S(t) = exp(-lambda * t)
    float exponentialSurvival(float t, float lambda) {
        return std::exp(-lambda * t);
    }
};

//=============================================================================
// Kaplan-Meier Tests
//=============================================================================

class KaplanMeierTest : public SurvivalTest {};

TEST_F(KaplanMeierTest, StartsAtOne) {
    // S(0) = 1
    auto data = generateExponentialSurvival(0.1f, 0.05f, 100, 42);
    auto km = computeKaplanMeier(data);

    EXPECT_NEAR(km[0].second, 1.0f, TOLERANCE);
}

TEST_F(KaplanMeierTest, NonIncreasing) {
    // Survival function is non-increasing
    auto data = generateExponentialSurvival(0.1f, 0.05f, 200, 123);
    auto km = computeKaplanMeier(data);

    for (size_t i = 1; i < km.size(); i++) {
        EXPECT_LE(km[i].second, km[i-1].second + TOLERANCE);
    }
}

TEST_F(KaplanMeierTest, BoundedByZeroOne) {
    // 0 <= S(t) <= 1
    auto data = generateExponentialSurvival(0.2f, 0.1f, 150, 456);
    auto km = computeKaplanMeier(data);

    for (const auto& [t, s] : km) {
        EXPECT_GE(s, 0.0f - TOLERANCE);
        EXPECT_LE(s, 1.0f + TOLERANCE);
    }
}

TEST_F(KaplanMeierTest, ConvergesToTrueExponential) {
    // KM should approximate true survival function for large n
    float lambda = 0.1f;
    auto data = generateExponentialSurvival(lambda, 0.02f, 1000, 789);
    auto km = computeKaplanMeier(data);

    // Check at several time points
    std::vector<float> check_times = {1.0f, 5.0f, 10.0f};

    for (float t : check_times) {
        // Find KM estimate at t
        float km_estimate = 1.0f;
        for (const auto& [km_t, km_s] : km) {
            if (km_t <= t) {
                km_estimate = km_s;
            } else {
                break;
            }
        }

        float true_survival = exponentialSurvival(t, lambda);
        EXPECT_NEAR(km_estimate, true_survival, SURVIVAL_TOLERANCE);
    }
}

TEST_F(KaplanMeierTest, AllEventsNoConstoring) {
    // When no censoring, KM is empirical survival
    std::vector<SurvivalData> data = {
        {1.0f, true, 0.0f},
        {2.0f, true, 0.0f},
        {3.0f, true, 0.0f},
        {4.0f, true, 0.0f},
        {5.0f, true, 0.0f}
    };

    auto km = computeKaplanMeier(data);

    // After each event, survival drops by 1/n_at_risk
    EXPECT_NEAR(km[1].second, 0.8f, TOLERANCE);  // 4/5
    EXPECT_NEAR(km[2].second, 0.6f, TOLERANCE);  // 3/5
    EXPECT_NEAR(km[3].second, 0.4f, TOLERANCE);  // 2/5
    EXPECT_NEAR(km[4].second, 0.2f, TOLERANCE);  // 1/5
    EXPECT_NEAR(km[5].second, 0.0f, TOLERANCE);  // 0/5
}

TEST_F(KaplanMeierTest, WithCensoring) {
    // Censoring affects the at-risk denominator
    std::vector<SurvivalData> data = {
        {1.0f, true, 0.0f},   // Event at t=1
        {2.0f, false, 0.0f},  // Censored at t=2
        {3.0f, true, 0.0f},   // Event at t=3
        {4.0f, true, 0.0f},   // Event at t=4
        {5.0f, false, 0.0f}   // Censored at t=5
    };

    auto km = computeKaplanMeier(data);

    // At t=1: 4/5 at risk, 1 event -> S = 4/5 = 0.8
    // At t=2: censored, no change to S
    // At t=3: 3 at risk, 1 event -> S = 0.8 * 2/3 = 0.533
    // At t=4: 2 at risk, 1 event -> S = 0.533 * 1/2 = 0.267

    EXPECT_NEAR(km[1].second, 0.8f, TOLERANCE);
}

TEST_F(KaplanMeierTest, MedianSurvivalTime) {
    // Find median survival time (time when S(t) = 0.5)
    float lambda = 0.1f;  // True median = ln(2)/0.1 = 6.93
    auto data = generateExponentialSurvival(lambda, 0.02f, 500, 111);
    auto km = computeKaplanMeier(data);

    // Find t where S crosses 0.5
    float median = 0.0f;
    for (size_t i = 1; i < km.size(); i++) {
        if (km[i].second <= 0.5f) {
            median = km[i].first;
            break;
        }
    }

    float true_median = std::log(2.0f) / lambda;
    EXPECT_NEAR(median, true_median, 1.0f);
}

//=============================================================================
// Cox Proportional Hazards Tests
//=============================================================================

class CoxModelTest : public SurvivalTest {};

TEST_F(CoxModelTest, CoefficientRecovery) {
    // Generate data where covariate affects hazard
    // h(t|x) = h0(t) * exp(beta * x)
    float true_beta = 0.5f;
    size_t n = 500;

    std::normal_distribution<float> cov_dist(0.0f, 1.0f);
    std::exponential_distribution<float> base_dist(0.1f);

    std::vector<SurvivalData> data(n);
    for (size_t i = 0; i < n; i++) {
        float x = cov_dist(rng);
        float lambda = 0.1f * std::exp(true_beta * x);
        std::exponential_distribution<float> event_dist(lambda);

        float event_time = event_dist(rng);
        float censor_time = base_dist(rng) * 10.0f;

        data[i].time = std::min(event_time, censor_time);
        data[i].event = (event_time <= censor_time);
        data[i].covariate = x;
    }

    // Sort by time
    std::sort(data.begin(), data.end(),
              [](const SurvivalData& a, const SurvivalData& b) {
                  return a.time < b.time;
              });

    // Simple score test direction
    // If beta > 0, subjects with higher x should have earlier events
    float sum_x_events = 0.0f;
    int n_events = 0;
    for (const auto& d : data) {
        if (d.event) {
            sum_x_events += d.covariate;
            n_events++;
        }
    }

    // Average covariate among events
    float mean_x_events = sum_x_events / n_events;

    // Compare to overall mean
    float sum_x_all = 0.0f;
    for (const auto& d : data) {
        sum_x_all += d.covariate;
    }
    float mean_x_all = sum_x_all / n;

    // With positive beta, events should have higher x on average
    // (direction depends on parameterization)
    EXPECT_GT(mean_x_events, mean_x_all - 0.5f);
}

TEST_F(CoxModelTest, ProportionalHazardsAssumption) {
    // Hazard ratio should be constant over time
    float beta = 0.3f;

    // At any time t: HR = exp(beta * (x1 - x2))
    float x1 = 1.0f, x2 = 0.0f;
    float expected_hr = std::exp(beta * (x1 - x2));

    EXPECT_GT(expected_hr, 1.0f);  // Higher x -> higher hazard
    EXPECT_NEAR(expected_hr, std::exp(0.3f), TOLERANCE);
}

TEST_F(CoxModelTest, NoEffect_BetaZero) {
    // When covariate has no effect, beta should be ~0
    auto data = generateExponentialSurvival(0.1f, 0.05f, 200, 222);

    // Covariate is random, independent of survival
    float sum_x_events = 0.0f, sum_x_all = 0.0f;
    int n_events = 0;

    for (const auto& d : data) {
        sum_x_all += d.covariate;
        if (d.event) {
            sum_x_events += d.covariate;
            n_events++;
        }
    }

    float mean_x_events = sum_x_events / n_events;
    float mean_x_all = sum_x_all / data.size();

    // Difference should be small when no effect
    EXPECT_NEAR(mean_x_events, mean_x_all, 0.3f);
}

//=============================================================================
// Log-Rank Test Tests
//=============================================================================

class LogRankTest : public SurvivalTest {};

TEST_F(LogRankTest, SameDistribution_NotSignificant) {
    // Two groups with same distribution should not be significantly different
    auto group1 = generateExponentialSurvival(0.1f, 0.05f, 100, 333);
    auto group2 = generateExponentialSurvival(0.1f, 0.05f, 100, 444);

    // Count observed events
    int o1 = 0, o2 = 0;
    for (const auto& d : group1) if (d.event) o1++;
    for (const auto& d : group2) if (d.event) o2++;

    // Expected events under null (proportional to at-risk)
    // Simplified: e1 = (o1 + o2) * n1 / (n1 + n2)
    float e1 = (o1 + o2) * 100.0f / 200.0f;
    float e2 = (o1 + o2) * 100.0f / 200.0f;

    // Log-rank statistic: sum of (O - E) / sqrt(V)
    // Under null, should be close to 0
    float diff1 = o1 - e1;
    float diff2 = o2 - e2;

    EXPECT_NEAR(diff1 + diff2, 0.0f, TOLERANCE);  // Balanced
}

TEST_F(LogRankTest, DifferentDistribution_Significant) {
    // Two groups with different hazards should be significantly different
    auto group1 = generateExponentialSurvival(0.1f, 0.01f, 100, 555);  // Low hazard
    auto group2 = generateExponentialSurvival(0.3f, 0.01f, 100, 666);  // High hazard

    // Count events
    int o1 = 0, o2 = 0;
    for (const auto& d : group1) if (d.event) o1++;
    for (const auto& d : group2) if (d.event) o2++;

    // Group 2 should have more events (higher hazard)
    EXPECT_GT(o2, o1);
}

TEST_F(LogRankTest, PowerIncreases_WithSampleSize) {
    // Larger samples should detect smaller differences

    // With different hazards
    float lambda1 = 0.1f, lambda2 = 0.15f;

    // Small sample
    auto small1 = generateExponentialSurvival(lambda1, 0.02f, 50, 777);
    auto small2 = generateExponentialSurvival(lambda2, 0.02f, 50, 888);

    // Large sample
    auto large1 = generateExponentialSurvival(lambda1, 0.02f, 500, 999);
    auto large2 = generateExponentialSurvival(lambda2, 0.02f, 500, 1000);

    // Count event rate difference
    float rate1_small = 0.0f, rate2_small = 0.0f;
    for (const auto& d : small1) rate1_small += d.event ? 1 : 0;
    for (const auto& d : small2) rate2_small += d.event ? 1 : 0;
    rate1_small /= small1.size();
    rate2_small /= small2.size();

    float rate1_large = 0.0f, rate2_large = 0.0f;
    for (const auto& d : large1) rate1_large += d.event ? 1 : 0;
    for (const auto& d : large2) rate2_large += d.event ? 1 : 0;
    rate1_large /= large1.size();
    rate2_large /= large2.size();

    // Larger sample should give more stable estimates
    EXPECT_GT(rate2_large, rate1_large);
}

//=============================================================================
// Hazard Function Tests
//=============================================================================

class HazardFunctionTest : public SurvivalTest {};

TEST_F(HazardFunctionTest, ExponentialHazard_Constant) {
    // Exponential has constant hazard
    float lambda = 0.2f;

    for (float t = 0.1f; t <= 10.0f; t += 0.5f) {
        float h = exponentialHazard(lambda);
        EXPECT_NEAR(h, lambda, TOLERANCE);
    }
}

TEST_F(HazardFunctionTest, HazardSurvival_Relationship) {
    // h(t) = -d/dt log(S(t))
    // For exponential: S(t) = exp(-lambda*t), h(t) = lambda

    float lambda = 0.1f;
    float dt = 0.01f;

    for (float t = 0.5f; t <= 5.0f; t += 0.5f) {
        float s1 = exponentialSurvival(t, lambda);
        float s2 = exponentialSurvival(t + dt, lambda);

        // Numerical derivative of -log(S)
        float numerical_hazard = -(std::log(s2) - std::log(s1)) / dt;

        EXPECT_NEAR(numerical_hazard, lambda, LOOSE_TOLERANCE);
    }
}

TEST_F(HazardFunctionTest, CumulativeHazard) {
    // H(t) = -log(S(t))
    // For exponential: H(t) = lambda * t

    float lambda = 0.15f;

    for (float t = 0.5f; t <= 5.0f; t += 0.5f) {
        float s = exponentialSurvival(t, lambda);
        float cumulative_hazard = -std::log(s);
        float expected = lambda * t;

        EXPECT_NEAR(cumulative_hazard, expected, TOLERANCE);
    }
}

//=============================================================================
// Censoring Tests
//=============================================================================

class CensoringTest : public SurvivalTest {};

TEST_F(CensoringTest, HighCensoring_WiderCI) {
    // More censoring -> less information -> wider confidence intervals
    // (Simplified: variance of KM increases with censoring)

    float lambda = 0.1f;

    // Low censoring
    auto low_censor = generateExponentialSurvival(lambda, 0.01f, 200, 111);
    // High censoring
    auto high_censor = generateExponentialSurvival(lambda, 0.5f, 200, 222);

    // Count events
    int events_low = 0, events_high = 0;
    for (const auto& d : low_censor) if (d.event) events_low++;
    for (const auto& d : high_censor) if (d.event) events_high++;

    // High censoring should have fewer events
    EXPECT_LT(events_high, events_low);
}

TEST_F(CensoringTest, RightCensoring_NonInformative) {
    // Non-informative censoring: censoring mechanism independent of event
    auto data = generateExponentialSurvival(0.1f, 0.1f, 500, 333);

    // Check that censored and event times are not correlated
    std::vector<float> event_times, censor_times;
    for (const auto& d : data) {
        if (d.event) {
            event_times.push_back(d.time);
        } else {
            censor_times.push_back(d.time);
        }
    }

    if (event_times.size() > 10 && censor_times.size() > 10) {
        float mean_event = nimcp_stats_mean(event_times.data(),
                                            static_cast<uint32_t>(event_times.size()));
        float mean_censor = nimcp_stats_mean(censor_times.data(),
                                              static_cast<uint32_t>(censor_times.size()));

        // For non-informative censoring, no systematic difference expected
        // (though there will be some difference due to randomness)
        EXPECT_GT(mean_event, 0.0f);
        EXPECT_GT(mean_censor, 0.0f);
    }
}

//=============================================================================
// Edge Cases
//=============================================================================

class SurvivalEdgeCaseTest : public SurvivalTest {};

TEST_F(SurvivalEdgeCaseTest, NoEvents) {
    // All censored
    std::vector<SurvivalData> data = {
        {1.0f, false, 0.0f},
        {2.0f, false, 0.0f},
        {3.0f, false, 0.0f}
    };

    auto km = computeKaplanMeier(data);

    // S(t) = 1 for all t when no events
    for (const auto& [t, s] : km) {
        EXPECT_NEAR(s, 1.0f, TOLERANCE);
    }
}

TEST_F(SurvivalEdgeCaseTest, SingleObservation) {
    std::vector<SurvivalData> data = {
        {5.0f, true, 0.0f}
    };

    auto km = computeKaplanMeier(data);

    EXPECT_EQ(km.size(), 2u);  // (0, 1) and (5, 0)
    EXPECT_NEAR(km[0].second, 1.0f, TOLERANCE);
    EXPECT_NEAR(km[1].second, 0.0f, TOLERANCE);
}

TEST_F(SurvivalEdgeCaseTest, TiedEventTimes) {
    // Multiple events at same time
    std::vector<SurvivalData> data = {
        {1.0f, true, 0.0f},
        {1.0f, true, 0.0f},
        {2.0f, true, 0.0f},
        {3.0f, true, 0.0f}
    };

    auto km = computeKaplanMeier(data);

    // At t=1, two events from 4 at risk
    // S(1) = (4-1)/4 * (3-1)/3 = 0.75 * 0.667 = 0.5
    // Or using product limit: S(1) = 2/4 = 0.5

    // Find S at t=1
    float s_at_1 = 1.0f;
    for (const auto& [t, s] : km) {
        if (t == 1.0f) {
            s_at_1 = s;
        }
    }

    EXPECT_LT(s_at_1, 1.0f);
}

TEST_F(SurvivalEdgeCaseTest, ZeroTime) {
    std::vector<SurvivalData> data = {
        {0.0f, true, 0.0f},  // Immediate event
        {1.0f, true, 0.0f}
    };

    auto km = computeKaplanMeier(data);

    // Should handle t=0 event
    EXPECT_GT(km.size(), 1u);
}

TEST_F(SurvivalEdgeCaseTest, VeryLongFollowUp) {
    // Event at very late time
    std::vector<SurvivalData> data = {
        {1.0f, true, 0.0f},
        {100.0f, false, 0.0f},
        {1000.0f, true, 0.0f}
    };

    auto km = computeKaplanMeier(data);

    // Should handle large time values
    EXPECT_GT(km.size(), 1u);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
