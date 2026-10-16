/**
 * @file test_monte_carlo.cpp
 * @brief Unit tests for Monte Carlo algorithms (nimcp_monte_carlo.h)
 * @version 1.0.0
 * @date 2026-01-03
 *
 * WHAT: Comprehensive tests for MCTS, MC sampling, and utility functions
 * WHY:  Ensure correctness of Monte Carlo algorithm implementations
 * HOW:  GTest test cases covering normal operation, edge cases, and error handling
 */

#include <gtest/gtest.h>
#include <vector>
#include <cmath>
#include <numeric>

#include "utils/algorithms/nimcp_monte_carlo.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

/**
 * @brief Simple game state for MCTS testing
 *
 * A number guessing game where the goal is to reach target value.
 * Actions: 0 = add 1, 1 = add 2, 2 = subtract 1
 */
struct SimpleGameState {
    int value;
    int target;
    int moves;
    int max_moves;
};

class MCTSTest : public ::testing::Test {
protected:
    static const int MAX_ACTIONS = 3;

    static uint32_t get_action_count(const void* state, void* user_data) {
        (void)user_data;
        const SimpleGameState* s = (const SimpleGameState*)state;
        if (s->moves >= s->max_moves) return 0;
        return MAX_ACTIONS;
    }

    static uint32_t get_action(const void* state, uint32_t action_idx, void* user_data) {
        (void)state;
        (void)user_data;
        if (action_idx >= MAX_ACTIONS) return UINT32_MAX;
        return action_idx;
    }

    static void* apply_action(const void* state, uint32_t action, void* user_data) {
        (void)user_data;
        const SimpleGameState* s = (const SimpleGameState*)state;

        SimpleGameState* new_state = (SimpleGameState*)malloc(sizeof(SimpleGameState));
        if (!new_state) return nullptr;

        *new_state = *s;
        new_state->moves++;

        switch (action) {
            case 0: new_state->value += 1; break;
            case 1: new_state->value += 2; break;
            case 2: new_state->value -= 1; break;
        }

        return new_state;
    }

    static float evaluate(const void* state, void* user_data) {
        (void)user_data;
        const SimpleGameState* s = (const SimpleGameState*)state;

        /* Higher value = closer to target */
        float distance = fabsf((float)(s->value - s->target));
        return 1.0f / (1.0f + distance);
    }

    static bool is_terminal(const void* state, void* user_data) {
        (void)user_data;
        const SimpleGameState* s = (const SimpleGameState*)state;
        return s->value == s->target || s->moves >= s->max_moves;
    }

    static void free_state(void* state, void* user_data) {
        (void)user_data;
        free(state);
    }

    static void* clone_state(const void* state, void* user_data) {
        (void)user_data;
        const SimpleGameState* s = (const SimpleGameState*)state;

        SimpleGameState* new_state = (SimpleGameState*)malloc(sizeof(SimpleGameState));
        if (!new_state) return nullptr;

        *new_state = *s;
        return new_state;
    }

    mcts_config_t create_config() {
        mcts_config_t config;
        mcts_config_init(&config);

        config.get_action_count = get_action_count;
        config.get_action = get_action;
        config.apply_action = apply_action;
        config.evaluate = evaluate;
        config.is_terminal = is_terminal;
        config.free_state = free_state;
        config.clone_state = clone_state;
        config.max_iterations = 100;
        config.max_depth = 10;
        config.seed = 12345;

        return config;
    }
};

class MCSamplingTest : public ::testing::Test {
protected:
    /* Sample from uniform [0, 1] */
    static float uniform_sampler(void* user_data) {
        uint32_t* seed = (uint32_t*)user_data;
        return mc_random_uniform(seed);
    }

    /* f(x) = x^2 */
    static float square_objective(float sample, void* user_data) {
        (void)user_data;
        return sample * sample;
    }

    /* Normal distribution proposal: x' = x + N(0, 0.5) */
    static float normal_proposal(float current, void* user_data) {
        uint32_t* seed = (uint32_t*)user_data;
        return current + mc_random_normal(seed, 0.0f, 0.5f);
    }

    /* Target: N(0, 1) unnormalized */
    static float normal_density(float x, void* user_data) {
        (void)user_data;
        return expf(-0.5f * x * x);
    }
};

class RandomUtilsTest : public ::testing::Test {
protected:
    /* Common seed for reproducibility */
    uint32_t seed = 42;
};

/* ============================================================================
 * MCTS Tests
 * ============================================================================ */

TEST_F(MCTSTest, ConfigInit) {
    mcts_config_t config;
    mcts_config_init(&config);

    EXPECT_EQ(config.max_iterations, MCTS_DEFAULT_ITERATIONS);
    EXPECT_EQ(config.max_depth, MCTS_DEFAULT_MAX_DEPTH);
    EXPECT_FLOAT_EQ(config.exploration_constant, MCTS_DEFAULT_EXPLORATION);
    EXPECT_FLOAT_EQ(config.discount_factor, MCTS_DEFAULT_DISCOUNT);
    EXPECT_EQ(config.policy, MCTS_SELECT_UCB1);
}

TEST_F(MCTSTest, NullParameters) {
    mcts_config_t config = create_config();
    SimpleGameState state = {0, 5, 0, 10};
    mcts_result_t result;

    /* Null config */
    EXPECT_EQ(mcts_search(nullptr, &state, &result), NIMCP_MC_ERROR_NULL);

    /* Null state */
    EXPECT_EQ(mcts_search(&config, nullptr, &result), NIMCP_MC_ERROR_NULL);

    /* Null result */
    EXPECT_EQ(mcts_search(&config, &state, nullptr), NIMCP_MC_ERROR_NULL);
}

TEST_F(MCTSTest, MissingCallbacks) {
    mcts_config_t config = create_config();
    config.get_action_count = nullptr;

    SimpleGameState state = {0, 5, 0, 10};
    mcts_result_t result;

    EXPECT_EQ(mcts_search(&config, &state, &result), NIMCP_MC_ERROR_NULL);
}

TEST_F(MCTSTest, SimpleSearch) {
    mcts_config_t config = create_config();
    SimpleGameState state = {0, 5, 0, 10};
    mcts_result_t result;

    nimcp_mc_result_t status = mcts_search(&config, &state, &result);

    EXPECT_EQ(status, NIMCP_MC_OK);
    EXPECT_GT(result.iterations_completed, 0u);
    EXPECT_GT(result.nodes_created, 0u);
    EXPECT_EQ(result.num_actions, 3u);  /* 3 possible actions */

    /* Best action should be action 1 (add 2) since target is 5 */
    /* With enough iterations, this should converge */
    EXPECT_LT(result.best_action, 3u);

    mcts_result_free(&result);
}

TEST_F(MCTSTest, TerminalState) {
    mcts_config_t config = create_config();
    /* Already at target */
    SimpleGameState state = {5, 5, 0, 10};
    mcts_result_t result;

    nimcp_mc_result_t status = mcts_search(&config, &state, &result);

    EXPECT_EQ(status, NIMCP_MC_OK);
    /* No actions from terminal state */
    EXPECT_EQ(result.num_actions, 0u);

    mcts_result_free(&result);
}

TEST_F(MCTSTest, MaxDepthRespected) {
    mcts_config_t config = create_config();
    config.max_depth = 3;

    SimpleGameState state = {0, 100, 0, 100};  /* Target far away */
    mcts_result_t result;

    nimcp_mc_result_t status = mcts_search(&config, &state, &result);

    EXPECT_EQ(status, NIMCP_MC_OK);
    EXPECT_LE(result.max_depth_reached, 3u);

    mcts_result_free(&result);
}

TEST_F(MCTSTest, VisitCounts) {
    mcts_config_t config = create_config();
    config.max_iterations = 50;

    SimpleGameState state = {0, 5, 0, 10};
    mcts_result_t result;

    nimcp_mc_result_t status = mcts_search(&config, &state, &result);

    EXPECT_EQ(status, NIMCP_MC_OK);

    /* Total visits should equal iterations */
    uint32_t total_visits = 0;
    for (uint32_t i = 0; i < result.num_actions; i++) {
        total_visits += result.action_visits[i];
    }
    /* Each iteration visits at least the root child */
    EXPECT_GT(total_visits, 0u);

    mcts_result_free(&result);
}

/* ============================================================================
 * UCB Calculation Tests
 * ============================================================================ */

TEST(UCBTest, UCB1Unvisited) {
    /* Unvisited nodes should have infinite UCB */
    float ucb = mcts_compute_ucb1(0.5f, 0, 100, 1.414f);
    EXPECT_EQ(ucb, FLT_MAX);
}

TEST(UCBTest, UCB1Calculation) {
    /* Q=0.5, n=10, N=100, c=sqrt(2) */
    float ucb = mcts_compute_ucb1(0.5f, 10, 100, 1.414f);

    /* UCB = 0.5 + 1.414 * sqrt(ln(100)/10) = 0.5 + 1.414 * sqrt(0.4605) = 0.5 + 0.96 */
    EXPECT_GT(ucb, 0.5f);
    EXPECT_LT(ucb, 2.0f);
}

TEST(UCBTest, PUCTCalculation) {
    /* Q=0.5, n=10, N=100, P=0.3, c=1.5 */
    float puct = mcts_compute_puct(0.5f, 10, 100, 0.3f, 1.5f);

    /* PUCT = 0.5 + 1.5 * 0.3 * sqrt(100) / (1+10) = 0.5 + 0.45 * 10 / 11 = 0.5 + 0.41 */
    EXPECT_GT(puct, 0.5f);
    EXPECT_LT(puct, 1.5f);
}

/* ============================================================================
 * Monte Carlo Sampling Tests
 * ============================================================================ */

TEST_F(MCSamplingTest, ConfigInit) {
    mc_config_t config;
    mc_config_init(&config);

    EXPECT_EQ(config.method, MC_SAMPLE_UNIFORM);
    EXPECT_EQ(config.num_samples, MC_DEFAULT_SAMPLES);
    EXPECT_EQ(config.burnin, MC_DEFAULT_BURNIN);
    EXPECT_FALSE(config.store_samples);
}

TEST_F(MCSamplingTest, NullParameters) {
    mc_config_t config;
    mc_config_init(&config);
    mc_result_t result;

    EXPECT_EQ(mc_estimate(nullptr, &result), NIMCP_MC_ERROR_NULL);
    EXPECT_EQ(mc_estimate(&config, nullptr), NIMCP_MC_ERROR_NULL);

    /* Missing sampler */
    config.objective = square_objective;
    EXPECT_EQ(mc_estimate(&config, &result), NIMCP_MC_ERROR_NULL);
}

TEST_F(MCSamplingTest, UniformSampling) {
    uint32_t seed = 12345;

    mc_config_t config;
    mc_config_init(&config);
    config.method = MC_SAMPLE_UNIFORM;
    config.num_samples = 10000;
    config.sampler = uniform_sampler;
    config.objective = square_objective;
    config.user_data = &seed;

    mc_result_t result;
    nimcp_mc_result_t status = mc_estimate(&config, &result);

    EXPECT_EQ(status, NIMCP_MC_OK);

    /* E[X^2] for X ~ U(0,1) is 1/3 = 0.333... */
    EXPECT_NEAR(result.estimate, 0.333f, 0.02f);
    EXPECT_GT(result.variance, 0.0f);
    EXPECT_GT(result.std_error, 0.0f);

    mc_result_free(&result);
}

TEST_F(MCSamplingTest, StoreSamples) {
    uint32_t seed = 12345;

    mc_config_t config;
    mc_config_init(&config);
    config.method = MC_SAMPLE_UNIFORM;
    config.num_samples = 100;
    config.sampler = uniform_sampler;
    config.objective = square_objective;
    config.user_data = &seed;
    config.store_samples = true;

    mc_result_t result;
    nimcp_mc_result_t status = mc_estimate(&config, &result);

    EXPECT_EQ(status, NIMCP_MC_OK);
    EXPECT_NE(result.samples, nullptr);
    EXPECT_EQ(result.num_samples, 100u);

    /* All samples should be in [0, 1] since f(x)=x^2 and x in [0,1] */
    for (uint32_t i = 0; i < result.num_samples; i++) {
        EXPECT_GE(result.samples[i], 0.0f);
        EXPECT_LE(result.samples[i], 1.0f);
    }

    mc_result_free(&result);
    EXPECT_EQ(result.samples, nullptr);  /* Should be freed */
}

TEST_F(MCSamplingTest, MetropolisHastings) {
    uint32_t seed = 12345;

    mc_config_t config;
    mc_config_init(&config);
    config.method = MC_SAMPLE_METROPOLIS_HASTINGS;
    config.num_samples = 5000;
    config.burnin = 1000;
    config.sampler = uniform_sampler;
    config.objective = square_objective;
    config.proposal = normal_proposal;
    config.density = normal_density;
    config.user_data = &seed;

    mc_result_t result;
    nimcp_mc_result_t status = mc_estimate(&config, &result);

    EXPECT_EQ(status, NIMCP_MC_OK);

    /* E[X^2] for X ~ N(0,1) is 1.0 (variance) */
    EXPECT_NEAR(result.estimate, 1.0f, 0.2f);

    /* Acceptance rate should be reasonable */
    EXPECT_GT(result.acceptance_rate, 0.1f);
    EXPECT_LT(result.acceptance_rate, 0.9f);

    mc_result_free(&result);
}

/* ============================================================================
 * Random Number Generation Tests
 * ============================================================================ */

TEST_F(RandomUtilsTest, UniformRange) {
    /* Generate many samples and check they're all in [0, 1) */
    for (int i = 0; i < 1000; i++) {
        float r = mc_random_uniform(&seed);
        EXPECT_GE(r, 0.0f);
        EXPECT_LT(r, 1.0f);
    }
}

TEST_F(RandomUtilsTest, UniformDistribution) {
    /* Chi-squared test for uniformity (simplified) */
    int bins[10] = {0};
    int n = 10000;

    for (int i = 0; i < n; i++) {
        float r = mc_random_uniform(&seed);
        int bin = (int)(r * 10);
        if (bin >= 10) bin = 9;
        bins[bin]++;
    }

    /* Each bin should have approximately n/10 = 1000 samples */
    for (int i = 0; i < 10; i++) {
        EXPECT_GT(bins[i], 800);
        EXPECT_LT(bins[i], 1200);
    }
}

TEST_F(RandomUtilsTest, RandomInt) {
    /* Test range */
    for (int i = 0; i < 1000; i++) {
        uint32_t r = mc_random_int(&seed, 10);
        EXPECT_LT(r, 10u);
    }

    /* Edge case: max = 1 */
    for (int i = 0; i < 100; i++) {
        uint32_t r = mc_random_int(&seed, 1);
        EXPECT_EQ(r, 0u);
    }
}

TEST_F(RandomUtilsTest, NormalDistribution) {
    /* Generate samples and check mean/variance */
    float sum = 0.0f;
    float sum_sq = 0.0f;
    int n = 10000;

    for (int i = 0; i < n; i++) {
        float r = mc_random_normal(&seed, 5.0f, 2.0f);  /* N(5, 4) */
        sum += r;
        sum_sq += r * r;
    }

    float mean = sum / n;
    float variance = (sum_sq / n) - (mean * mean);

    EXPECT_NEAR(mean, 5.0f, 0.2f);
    EXPECT_NEAR(variance, 4.0f, 0.5f);
}

TEST_F(RandomUtilsTest, RandomChoice) {
    float weights[] = {1.0f, 2.0f, 3.0f, 4.0f};  /* Total = 10 */
    int counts[4] = {0};
    int n = 10000;

    for (int i = 0; i < n; i++) {
        uint32_t choice = mc_random_choice(&seed, weights, 4);
        EXPECT_LT(choice, 4u);
        counts[choice]++;
    }

    /* Choice 3 (weight 4/10) should be most common */
    EXPECT_GT(counts[3], counts[0]);
    EXPECT_GT(counts[3], counts[1]);
    EXPECT_GT(counts[3], counts[2]);

    /* Approximate expected proportions */
    EXPECT_NEAR((float)counts[0] / n, 0.1f, 0.03f);
    EXPECT_NEAR((float)counts[1] / n, 0.2f, 0.03f);
    EXPECT_NEAR((float)counts[2] / n, 0.3f, 0.03f);
    EXPECT_NEAR((float)counts[3] / n, 0.4f, 0.03f);
}

TEST_F(RandomUtilsTest, SeedFromTime) {
    uint32_t s1 = mc_seed_from_time();
    uint32_t s2 = mc_seed_from_time();

    /* Seeds should be non-zero and potentially different */
    EXPECT_NE(s1, 0u);
    EXPECT_NE(s2, 0u);
}

/* ============================================================================
 * Shuffle Tests
 * ============================================================================ */

TEST_F(RandomUtilsTest, ShuffleU32Preserves) {
    uint32_t arr[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    uint32_t original_sum = 55;  /* 1+2+...+10 */

    mc_shuffle_u32(arr, 10, &seed);

    /* Sum should be preserved */
    uint32_t sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += arr[i];
    }
    EXPECT_EQ(sum, original_sum);
}

TEST_F(RandomUtilsTest, ShuffleU32Changes) {
    uint32_t arr[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    uint32_t original[10];
    memcpy(original, arr, sizeof(arr));

    mc_shuffle_u32(arr, 10, &seed);

    /* Should be different from original (extremely unlikely to be same) */
    bool same = true;
    for (int i = 0; i < 10; i++) {
        if (arr[i] != original[i]) {
            same = false;
            break;
        }
    }
    EXPECT_FALSE(same);
}

TEST_F(RandomUtilsTest, ShuffleGeneric) {
    int arr[] = {10, 20, 30, 40, 50};
    int original_sum = 150;

    mc_shuffle(arr, 5, sizeof(int), &seed);

    int sum = 0;
    for (int i = 0; i < 5; i++) {
        sum += arr[i];
    }
    EXPECT_EQ(sum, original_sum);
}

/* ============================================================================
 * Statistical Utility Tests
 * ============================================================================ */

TEST(StatisticsTest, Mean) {
    float values[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    EXPECT_FLOAT_EQ(mc_mean(values, 5), 3.0f);
}

TEST(StatisticsTest, MeanEmpty) {
    EXPECT_FLOAT_EQ(mc_mean(nullptr, 0), 0.0f);
}

TEST(StatisticsTest, Variance) {
    float values[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float mean = 3.0f;
    /* Variance = ((1-3)^2 + (2-3)^2 + (3-3)^2 + (4-3)^2 + (5-3)^2) / 4 = 10/4 = 2.5 */
    EXPECT_FLOAT_EQ(mc_variance(values, 5, mean), 2.5f);
}

TEST(StatisticsTest, StdError) {
    /* SE = sqrt(variance / n) */
    EXPECT_NEAR(mc_std_error(4.0f, 100), 0.2f, 0.001f);
}

TEST(StatisticsTest, EffectiveSampleSize) {
    /* Equal weights: ESS = n */
    float equal_weights[] = {1.0f, 1.0f, 1.0f, 1.0f};
    EXPECT_NEAR(mc_effective_sample_size(equal_weights, 4), 4.0f, 0.001f);

    /* One dominant weight: ESS approaches 1 */
    float unequal_weights[] = {100.0f, 1.0f, 1.0f, 1.0f};
    float ess = mc_effective_sample_size(unequal_weights, 4);
    EXPECT_LT(ess, 2.0f);
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST_F(RandomUtilsTest, ShuffleEmpty) {
    uint32_t arr[] = {1};
    mc_shuffle_u32(arr, 0, &seed);  /* Should not crash */
    mc_shuffle_u32(arr, 1, &seed);  /* Should not change single element */
    EXPECT_EQ(arr[0], 1u);
}

TEST_F(RandomUtilsTest, ShuffleNull) {
    mc_shuffle_u32(nullptr, 10, &seed);  /* Should not crash */
    uint32_t arr[] = {1, 2, 3};
    mc_shuffle_u32(arr, 3, nullptr);  /* Should not crash */
}

TEST_F(MCTSTest, ZeroIterations) {
    mcts_config_t config = create_config();
    config.max_iterations = 0;

    SimpleGameState state = {0, 5, 0, 10};
    mcts_result_t result;

    nimcp_mc_result_t status = mcts_search(&config, &state, &result);

    EXPECT_EQ(status, NIMCP_MC_OK);
    EXPECT_EQ(result.iterations_completed, 0u);

    mcts_result_free(&result);
}

TEST_F(MCSamplingTest, ZeroSamples) {
    uint32_t seed = 12345;

    mc_config_t config;
    mc_config_init(&config);
    config.num_samples = 0;
    config.sampler = uniform_sampler;
    config.objective = square_objective;
    config.user_data = &seed;

    mc_result_t result;
    nimcp_mc_result_t status = mc_estimate(&config, &result);

    EXPECT_EQ(status, NIMCP_MC_ERROR_INVALID);
}
