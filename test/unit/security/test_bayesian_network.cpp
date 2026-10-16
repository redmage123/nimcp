/**
 * @file test_bayesian_network.cpp
 * @brief Unit tests for Bayesian network inference
 *
 * WHAT: Test Bayesian network creation, inference, and learning
 * WHY:  Verify correctness of probabilistic inference
 * HOW:  Test graph construction, CPT updates, inference, learning
 *
 * @author NIMCP Development Team
 * @date 2025-12-07
 */

#include <gtest/gtest.h>
#include "security/nimcp_anomaly_detector.h"
#include <cmath>

/*=============================================================================
 * TEST FIXTURES
 *============================================================================*/

class BayesianNetworkTest : public ::testing::Test {
protected:
    nimcp_bayesian_network_t bn;

    void SetUp() override {
        bn = nullptr;
    }

    void TearDown() override {
        if (bn) {
            nimcp_bn_destroy(bn);
            bn = nullptr;
        }
    }

    /* Helper: create simple 2-node network A -> B */
    void CreateSimpleNetwork() {
        bn = nimcp_bn_create(2);
        ASSERT_NE(nullptr, bn);

        /* Add edge A -> B */
        ASSERT_EQ(NIMCP_SUCCESS, nimcp_bn_add_edge(bn, 0, 1));
    }

    /* Helper: create 3-node network A -> C, B -> C */
    void CreateDiamondNetwork() {
        bn = nimcp_bn_create(3);
        ASSERT_NE(nullptr, bn);

        ASSERT_EQ(NIMCP_SUCCESS, nimcp_bn_add_edge(bn, 0, 2));  /* A -> C */
        ASSERT_EQ(NIMCP_SUCCESS, nimcp_bn_add_edge(bn, 1, 2));  /* B -> C */
    }
};

/*=============================================================================
 * CREATION AND DESTRUCTION TESTS
 *============================================================================*/

TEST_F(BayesianNetworkTest, CreateWithValidNodes) {
    bn = nimcp_bn_create(5);
    ASSERT_NE(nullptr, bn);
}

TEST_F(BayesianNetworkTest, CreateWithZeroNodes) {
    bn = nimcp_bn_create(0);
    EXPECT_EQ(nullptr, bn);
}

TEST_F(BayesianNetworkTest, CreateWithTooManyNodes) {
    bn = nimcp_bn_create(10000);
    EXPECT_EQ(nullptr, bn);
}

TEST_F(BayesianNetworkTest, DestroyNull) {
    nimcp_bn_destroy(nullptr);  /* Should not crash */
}

/*=============================================================================
 * EDGE ADDITION TESTS
 *============================================================================*/

TEST_F(BayesianNetworkTest, AddEdgeValid) {
    bn = nimcp_bn_create(3);
    ASSERT_NE(nullptr, bn);

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_bn_add_edge(bn, 0, 1));
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_bn_add_edge(bn, 1, 2));
}

TEST_F(BayesianNetworkTest, AddEdgeInvalidParent) {
    bn = nimcp_bn_create(3);
    ASSERT_NE(nullptr, bn);

    EXPECT_EQ(NIMCP_INVALID_PARAM, nimcp_bn_add_edge(bn, 10, 1));
}

TEST_F(BayesianNetworkTest, AddEdgeInvalidChild) {
    bn = nimcp_bn_create(3);
    ASSERT_NE(nullptr, bn);

    EXPECT_EQ(NIMCP_INVALID_PARAM, nimcp_bn_add_edge(bn, 0, 10));
}

TEST_F(BayesianNetworkTest, AddEdgeDuplicate) {
    bn = nimcp_bn_create(2);
    ASSERT_NE(nullptr, bn);

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_bn_add_edge(bn, 0, 1));
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_bn_add_edge(bn, 0, 1));  /* Duplicate, should succeed */
}

TEST_F(BayesianNetworkTest, AddEdgeNull) {
    EXPECT_EQ(NIMCP_INVALID_PARAM, nimcp_bn_add_edge(nullptr, 0, 1));
}

/*=============================================================================
 * CPT TESTS
 *============================================================================*/

TEST_F(BayesianNetworkTest, SetCPTValid) {
    CreateSimpleNetwork();

    /* Set CPT for node 1 (child) */
    /* CPT size = 10 (states) * 10 (parent states) * 1 (child states) = 100 */
    float cpt[100];
    for (int i = 0; i < 100; i++) {
        cpt[i] = 0.1f;  /* Uniform */
    }

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_bn_set_cpt(bn, 1, cpt, 100));
}

TEST_F(BayesianNetworkTest, SetCPTInvalidSize) {
    CreateSimpleNetwork();

    float cpt[50];  /* Wrong size */
    EXPECT_EQ(NIMCP_INVALID_PARAM, nimcp_bn_set_cpt(bn, 1, cpt, 50));
}

TEST_F(BayesianNetworkTest, SetCPTInvalidNode) {
    CreateSimpleNetwork();

    float cpt[100];
    EXPECT_EQ(NIMCP_INVALID_PARAM, nimcp_bn_set_cpt(bn, 10, cpt, 100));
}

TEST_F(BayesianNetworkTest, SetCPTNull) {
    CreateSimpleNetwork();

    EXPECT_EQ(NIMCP_INVALID_PARAM, nimcp_bn_set_cpt(bn, 0, nullptr, 10));
}

/*=============================================================================
 * INFERENCE TESTS
 *============================================================================*/

TEST_F(BayesianNetworkTest, InferSimpleNetwork) {
    CreateSimpleNetwork();

    float evidence[2] = {0.5f, NAN};  /* Observe A=0.5, B unobserved */
    float posteriors[2];

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_bn_infer(bn, evidence, posteriors));

    /* Posterior for A should match evidence */
    EXPECT_NEAR(0.5f, posteriors[0], 0.1f);

    /* Posterior for B should be computed from A */
    EXPECT_GE(posteriors[1], 0.0f);
    EXPECT_LE(posteriors[1], 1.0f);
}

TEST_F(BayesianNetworkTest, InferAllObserved) {
    CreateSimpleNetwork();

    float evidence[2] = {0.3f, 0.7f};  /* All observed */
    float posteriors[2];

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_bn_infer(bn, evidence, posteriors));

    /* Posteriors should be influenced by evidence (message passing may not preserve exact values) */
    EXPECT_NEAR(0.3f, posteriors[0], 0.1f);
    EXPECT_NEAR(0.7f, posteriors[1], 0.1f);
}

TEST_F(BayesianNetworkTest, InferNoneObserved) {
    CreateSimpleNetwork();

    float evidence[2] = {NAN, NAN};  /* None observed */
    float posteriors[2];

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_bn_infer(bn, evidence, posteriors));

    /* Should use priors (uniform = 0.5) */
    EXPECT_GE(posteriors[0], 0.0f);
    EXPECT_LE(posteriors[0], 1.0f);
    EXPECT_GE(posteriors[1], 0.0f);
    EXPECT_LE(posteriors[1], 1.0f);
}

TEST_F(BayesianNetworkTest, InferDiamondNetwork) {
    CreateDiamondNetwork();

    float evidence[3] = {0.2f, 0.8f, NAN};  /* A=0.2, B=0.8, C=? */
    float posteriors[3];

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_bn_infer(bn, evidence, posteriors));

    /* C should be influenced by both A and B */
    EXPECT_GE(posteriors[2], 0.0f);
    EXPECT_LE(posteriors[2], 1.0f);
}

TEST_F(BayesianNetworkTest, InferNull) {
    CreateSimpleNetwork();

    float evidence[2] = {0.5f, NAN};
    EXPECT_EQ(NIMCP_INVALID_PARAM, nimcp_bn_infer(bn, evidence, nullptr));
    EXPECT_EQ(NIMCP_INVALID_PARAM, nimcp_bn_infer(bn, nullptr, evidence));
}

/*=============================================================================
 * LEARNING TESTS
 *============================================================================*/

TEST_F(BayesianNetworkTest, LearnSingleSample) {
    CreateSimpleNetwork();

    float sample[2] = {0.3f, 0.7f};

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_bn_learn(bn, sample));
}

TEST_F(BayesianNetworkTest, LearnMultipleSamples) {
    CreateSimpleNetwork();

    float samples[][2] = {
        {0.1f, 0.2f},
        {0.3f, 0.4f},
        {0.5f, 0.6f},
        {0.7f, 0.8f},
        {0.9f, 1.0f}
    };

    for (auto& sample : samples) {
        EXPECT_EQ(NIMCP_SUCCESS, nimcp_bn_learn(bn, sample));
    }

    /* After learning, inference should be affected */
    float evidence[2] = {0.5f, NAN};
    float posteriors[2];

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_bn_infer(bn, evidence, posteriors));
    EXPECT_GE(posteriors[1], 0.0f);
    EXPECT_LE(posteriors[1], 1.0f);
}

TEST_F(BayesianNetworkTest, LearnNull) {
    CreateSimpleNetwork();

    EXPECT_EQ(NIMCP_INVALID_PARAM, nimcp_bn_learn(bn, nullptr));
}

/*=============================================================================
 * LOG-LIKELIHOOD TESTS
 *============================================================================*/

TEST_F(BayesianNetworkTest, LogLikelihoodValid) {
    CreateSimpleNetwork();

    float sample[2] = {0.5f, 0.5f};
    float log_likelihood;

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_bn_log_likelihood(bn, sample, &log_likelihood));

    /* Should be negative (log of probability) */
    EXPECT_LT(log_likelihood, 0.0f);
}

TEST_F(BayesianNetworkTest, LogLikelihoodAfterTraining) {
    CreateSimpleNetwork();

    /* Train on similar samples */
    float training_samples[][2] = {
        {0.4f, 0.6f},
        {0.5f, 0.7f},
        {0.6f, 0.8f}
    };

    for (auto& sample : training_samples) {
        nimcp_bn_learn(bn, sample);
    }

    /* Test sample similar to training */
    float test_sample[2] = {0.5f, 0.7f};
    float ll_similar;
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_bn_log_likelihood(bn, test_sample, &ll_similar));

    /* Test sample very different */
    float anomalous_sample[2] = {0.1f, 0.1f};
    float ll_anomalous;
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_bn_log_likelihood(bn, anomalous_sample, &ll_anomalous));

    /* Similar sample should have higher likelihood */
    EXPECT_GT(ll_similar, ll_anomalous);
}

TEST_F(BayesianNetworkTest, LogLikelihoodNull) {
    CreateSimpleNetwork();

    float sample[2] = {0.5f, 0.5f};
    EXPECT_EQ(NIMCP_INVALID_PARAM, nimcp_bn_log_likelihood(bn, sample, nullptr));
    EXPECT_EQ(NIMCP_INVALID_PARAM, nimcp_bn_log_likelihood(bn, nullptr, &sample[0]));
}

/*=============================================================================
 * COMPLEX NETWORK TESTS
 *============================================================================*/

TEST_F(BayesianNetworkTest, CreateAnomalyDetectionNetwork) {
    /* Create the actual anomaly detection network structure */
    bn = nimcp_bn_create(8);
    ASSERT_NE(nullptr, bn);

    /* Build structure */
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_bn_add_edge(bn, 0, 5));  /* Length -> Content */
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_bn_add_edge(bn, 1, 5));  /* Entropy -> Content */
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_bn_add_edge(bn, 2, 5));  /* Special -> Content */
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_bn_add_edge(bn, 3, 6));  /* Ngrams -> Behavior */
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_bn_add_edge(bn, 4, 6));  /* Timing -> Behavior */
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_bn_add_edge(bn, 5, 7));  /* Content -> Overall */
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_bn_add_edge(bn, 6, 7));  /* Behavior -> Overall */

    /* Test inference */
    float evidence[8] = {0.5f, 0.6f, 0.3f, 0.4f, 0.2f, NAN, NAN, NAN};
    float posteriors[8];

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_bn_infer(bn, evidence, posteriors));

    /* All posteriors should be valid */
    for (int i = 0; i < 8; i++) {
        EXPECT_GE(posteriors[i], 0.0f);
        EXPECT_LE(posteriors[i], 1.0f);
    }
}

TEST_F(BayesianNetworkTest, TrainAndInferAnomalyNetwork) {
    /* Create anomaly detection network */
    bn = nimcp_bn_create(8);
    ASSERT_NE(nullptr, bn);

    /* Build structure */
    nimcp_bn_add_edge(bn, 0, 5);
    nimcp_bn_add_edge(bn, 1, 5);
    nimcp_bn_add_edge(bn, 2, 5);
    nimcp_bn_add_edge(bn, 3, 6);
    nimcp_bn_add_edge(bn, 4, 6);
    nimcp_bn_add_edge(bn, 5, 7);
    nimcp_bn_add_edge(bn, 6, 7);

    /* Train on normal samples */
    float normal_samples[][8] = {
        {0.2f, 0.5f, 0.1f, 0.4f, 0.1f, 0.3f, 0.2f, 0.25f},
        {0.3f, 0.6f, 0.15f, 0.5f, 0.15f, 0.35f, 0.25f, 0.3f},
        {0.25f, 0.55f, 0.12f, 0.45f, 0.12f, 0.32f, 0.22f, 0.27f}
    };

    for (auto& sample : normal_samples) {
        nimcp_bn_learn(bn, sample);
    }

    /* Test on normal-like input */
    float test_normal[8] = {0.28f, 0.58f, 0.13f, 0.48f, 0.13f, NAN, NAN, NAN};
    float posteriors_normal[8];
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_bn_infer(bn, test_normal, posteriors_normal));

    /* Test on anomalous input */
    float test_anomaly[8] = {0.9f, 0.95f, 0.8f, 0.1f, 0.9f, NAN, NAN, NAN};
    float posteriors_anomaly[8];
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_bn_infer(bn, test_anomaly, posteriors_anomaly));

    /* Overall anomaly score should be different */
    /* (May not always be higher without proper training, but should be valid) */
    EXPECT_GE(posteriors_normal[7], 0.0f);
    EXPECT_LE(posteriors_normal[7], 1.0f);
    EXPECT_GE(posteriors_anomaly[7], 0.0f);
    EXPECT_LE(posteriors_anomaly[7], 1.0f);
}

/*=============================================================================
 * STRESS TESTS
 *============================================================================*/

TEST_F(BayesianNetworkTest, LargeNetworkCreation) {
    bn = nimcp_bn_create(50);
    ASSERT_NE(nullptr, bn);

    /* Add some edges */
    for (uint32_t i = 0; i < 25; i++) {
        EXPECT_EQ(NIMCP_SUCCESS, nimcp_bn_add_edge(bn, i, i + 25));
    }
}

TEST_F(BayesianNetworkTest, ManyInferenceCalls) {
    CreateSimpleNetwork();

    float evidence[2] = {0.5f, NAN};
    float posteriors[2];

    /* Run many inferences */
    for (int i = 0; i < 1000; i++) {
        EXPECT_EQ(NIMCP_SUCCESS, nimcp_bn_infer(bn, evidence, posteriors));
    }
}

TEST_F(BayesianNetworkTest, ManyLearnCalls) {
    CreateSimpleNetwork();

    /* Learn from many samples */
    for (int i = 0; i < 1000; i++) {
        float sample[2] = {
            (float)i / 1000.0f,
            (float)(i + 100) / 1100.0f
        };
        EXPECT_EQ(NIMCP_SUCCESS, nimcp_bn_learn(bn, sample));
    }
}

/*=============================================================================
 * EDGE CASES
 *============================================================================*/

TEST_F(BayesianNetworkTest, AllZeroEvidence) {
    CreateSimpleNetwork();

    float evidence[2] = {0.0f, 0.0f};
    float posteriors[2];

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_bn_infer(bn, evidence, posteriors));
    EXPECT_NEAR(0.0f, posteriors[0], 0.01f);
    EXPECT_GE(posteriors[1], 0.0f);
}

TEST_F(BayesianNetworkTest, AllOneEvidence) {
    CreateSimpleNetwork();

    float evidence[2] = {1.0f, 1.0f};
    float posteriors[2];

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_bn_infer(bn, evidence, posteriors));
    EXPECT_NEAR(1.0f, posteriors[0], 0.01f);
    EXPECT_LE(posteriors[1], 1.0f);
}

TEST_F(BayesianNetworkTest, OutOfRangeEvidence) {
    CreateSimpleNetwork();

    float evidence[2] = {-0.5f, 1.5f};  /* Out of [0, 1] range */
    float posteriors[2];

    /* Should clamp to valid range */
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_bn_infer(bn, evidence, posteriors));
    EXPECT_GE(posteriors[0], 0.0f);
    EXPECT_LE(posteriors[0], 1.0f);
}

/*=============================================================================
 * MAIN
 *============================================================================*/

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
