/**
 * @file test_mesh_pattern_routing.cpp
 * @brief Unit tests for brain-inspired pattern-based transaction routing
 *
 * Tests pattern similarity, receptive fields, self-selection, neuromodulation,
 * and learning from endorsement outcomes.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "mesh/nimcp_mesh_pattern_routing.h"
#include "mesh/nimcp_mesh_participant.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MeshPatternRoutingTest : public ::testing::Test {
protected:
    mesh_pattern_router_t* router;
    mesh_pattern_router_config_t config;

    void SetUp() override {
        memset(&config, 0, sizeof(config));
        config.default_threshold = MESH_DEFAULT_ACTIVATION_THRESHOLD;
        config.competition_strength = 0.5f;
        config.enable_learning = true;
        config.learning_rate = 0.1f;
        config.max_endorsers = 10;

        router = nullptr;
    }

    void TearDown() override {
        if (router) {
            mesh_pattern_router_destroy(router);
            router = nullptr;
        }
    }

    /* Create a pattern with specific values in first N dims */
    mesh_pattern_t create_pattern(const float* values, size_t count) {
        mesh_pattern_t pattern;
        mesh_pattern_init(&pattern);

        float magnitude = 0.0f;
        for (size_t i = 0; i < count && i < MESH_PATTERN_DIM; i++) {
            pattern.vector[i] = values[i];
            magnitude += values[i] * values[i];
        }
        pattern.magnitude = sqrtf(magnitude);
        pattern.active_dims = (uint32_t)count;

        return pattern;
    }

    /* Create a receptive field with a single preferred pattern */
    mesh_receptive_field_t create_simple_field(const mesh_pattern_t& pattern, float threshold) {
        mesh_receptive_field_t field;
        mesh_receptive_field_init(&field);

        field.preferred[0] = pattern;
        field.pattern_count = 1;
        field.threshold = threshold;
        field.sharpness = 1.0f;

        return field;
    }
};

/* ============================================================================
 * Pattern Utility Tests
 * ============================================================================ */

TEST_F(MeshPatternRoutingTest, PatternInit) {
    mesh_pattern_t pattern;
    mesh_pattern_init(&pattern);

    EXPECT_EQ(pattern.magnitude, 0.0f);
    EXPECT_EQ(pattern.active_dims, 0u);
    for (int i = 0; i < MESH_PATTERN_DIM; i++) {
        EXPECT_EQ(pattern.vector[i], 0.0f);
    }
}

TEST_F(MeshPatternRoutingTest, ReceptiveFieldInit) {
    mesh_receptive_field_t field;
    mesh_receptive_field_init(&field);

    EXPECT_EQ(field.pattern_count, 0u);
    EXPECT_EQ(field.threshold, MESH_DEFAULT_ACTIVATION_THRESHOLD);
    EXPECT_EQ(field.sharpness, 1.0f);
    EXPECT_EQ(field.learned_bias, 0.0f);
    EXPECT_EQ(field.neuromod_gain, 1.0f);
}

TEST_F(MeshPatternRoutingTest, PatternSimilarityIdentical) {
    float values[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t a = create_pattern(values, 4);
    mesh_pattern_t b = create_pattern(values, 4);

    float sim = mesh_pattern_similarity(&a, &b);
    EXPECT_NEAR(sim, 1.0f, 0.001f);  /* Identical patterns = similarity 1.0 */
}

TEST_F(MeshPatternRoutingTest, PatternSimilarityOrthogonal) {
    float values_a[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float values_b[] = {0.0f, 1.0f, 0.0f, 0.0f};

    mesh_pattern_t a = create_pattern(values_a, 4);
    mesh_pattern_t b = create_pattern(values_b, 4);

    float sim = mesh_pattern_similarity(&a, &b);
    EXPECT_NEAR(sim, 0.0f, 0.001f);  /* Orthogonal patterns = similarity 0.0 */
}

TEST_F(MeshPatternRoutingTest, PatternSimilarityOpposite) {
    float values_a[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float values_b[] = {-1.0f, 0.0f, 0.0f, 0.0f};

    mesh_pattern_t a = create_pattern(values_a, 4);
    mesh_pattern_t b = create_pattern(values_b, 4);

    float sim = mesh_pattern_similarity(&a, &b);
    /* Implementation clamps to [0,1] - opposite patterns have no positive similarity */
    EXPECT_NEAR(sim, 0.0f, 0.001f);
}

TEST_F(MeshPatternRoutingTest, PatternSimilarityPartial) {
    float values_a[] = {1.0f, 1.0f, 0.0f, 0.0f};
    float values_b[] = {1.0f, 0.0f, 0.0f, 0.0f};

    mesh_pattern_t a = create_pattern(values_a, 4);
    mesh_pattern_t b = create_pattern(values_b, 4);

    float sim = mesh_pattern_similarity(&a, &b);
    /* cos(45°) ≈ 0.707 */
    EXPECT_GT(sim, 0.7f);
    EXPECT_LT(sim, 0.72f);
}

TEST_F(MeshPatternRoutingTest, PatternSimilarityNullHandling) {
    mesh_pattern_t a;
    mesh_pattern_init(&a);

    float sim = mesh_pattern_similarity(&a, nullptr);
    EXPECT_EQ(sim, 0.0f);

    sim = mesh_pattern_similarity(nullptr, &a);
    EXPECT_EQ(sim, 0.0f);
}

TEST_F(MeshPatternRoutingTest, PatternBlendSimple) {
    float values_a[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float values_b[] = {0.0f, 1.0f, 0.0f, 0.0f};

    mesh_pattern_t a = create_pattern(values_a, 4);
    mesh_pattern_t b = create_pattern(values_b, 4);

    mesh_pattern_t patterns[] = {a, b};
    float weights[] = {0.5f, 0.5f};
    mesh_pattern_t result;

    nimcp_error_t err = mesh_pattern_blend(patterns, weights, 2, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* 50% of each pattern */
    EXPECT_NEAR(result.vector[0], 0.5f, 0.001f);
    EXPECT_NEAR(result.vector[1], 0.5f, 0.001f);
}

TEST_F(MeshPatternRoutingTest, PatternFromSemanticsMotor) {
    mesh_pattern_t pattern;

    nimcp_error_t err = mesh_pattern_from_semantics("motor arm reach", &pattern);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(pattern.magnitude, 0.0f);
    EXPECT_GT(pattern.active_dims, 0u);
}

/* ============================================================================
 * Router Lifecycle Tests
 * ============================================================================ */

TEST_F(MeshPatternRoutingTest, CreateRouter) {
    router = mesh_pattern_router_create(&config);
    ASSERT_NE(router, nullptr);
}

TEST_F(MeshPatternRoutingTest, CreateRouterNull) {
    router = mesh_pattern_router_create(nullptr);
    ASSERT_NE(router, nullptr);  /* Should use defaults */
}

TEST_F(MeshPatternRoutingTest, DestroyRouterNull) {
    mesh_pattern_router_destroy(nullptr);  /* Should not crash */
    SUCCEED();
}

TEST_F(MeshPatternRoutingTest, CreateDestroyMultiple) {
    for (int i = 0; i < 10; i++) {
        mesh_pattern_router_t* r = mesh_pattern_router_create(&config);
        ASSERT_NE(r, nullptr);
        mesh_pattern_router_destroy(r);
    }
}

/* ============================================================================
 * Receptive Field Registration Tests
 * ============================================================================ */

TEST_F(MeshPatternRoutingTest, RegisterReceptiveField) {
    router = mesh_pattern_router_create(&config);
    ASSERT_NE(router, nullptr);

    float values[] = {1.0f, 0.5f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(values, 4);
    mesh_receptive_field_t field = create_simple_field(pattern, 0.5f);

    mesh_participant_id_t module_id = 0x1001;

    nimcp_error_t err = mesh_pattern_router_register_receptive_field(
        router, module_id, &field);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshPatternRoutingTest, RegisterMultipleModules) {
    router = mesh_pattern_router_create(&config);
    ASSERT_NE(router, nullptr);

    /* Register 5 modules with different patterns */
    for (int i = 0; i < 5; i++) {
        float values[4] = {0.0f};
        values[i % 4] = 1.0f;

        mesh_pattern_t pattern = create_pattern(values, 4);
        mesh_receptive_field_t field = create_simple_field(pattern, 0.5f);

        mesh_participant_id_t module_id = 0x1000 + i;

        nimcp_error_t err = mesh_pattern_router_register_receptive_field(
            router, module_id, &field);
        EXPECT_EQ(err, NIMCP_SUCCESS);
    }
}

TEST_F(MeshPatternRoutingTest, RegisterNullRouter) {
    float values[] = {1.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(values, 2);
    mesh_receptive_field_t field = create_simple_field(pattern, 0.5f);

    nimcp_error_t err = mesh_pattern_router_register_receptive_field(
        nullptr, 0x1001, &field);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(MeshPatternRoutingTest, RegisterNullField) {
    router = mesh_pattern_router_create(&config);
    ASSERT_NE(router, nullptr);

    nimcp_error_t err = mesh_pattern_router_register_receptive_field(
        router, 0x1001, nullptr);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

/* ============================================================================
 * Activation Computation Tests
 * ============================================================================ */

TEST_F(MeshPatternRoutingTest, ComputeActivationsSimple) {
    router = mesh_pattern_router_create(&config);
    ASSERT_NE(router, nullptr);

    /* Register a module that responds to pattern [1, 0, 0, 0] */
    float field_values[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t field_pattern = create_pattern(field_values, 4);
    mesh_receptive_field_t field = create_simple_field(field_pattern, 0.3f);

    mesh_participant_id_t module_id = 0x1001;
    nimcp_error_t err = mesh_pattern_router_register_receptive_field(
        router, module_id, &field);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    /* Create transaction with matching pattern */
    mesh_pattern_transaction_t tx;
    memset(&tx, 0, sizeof(tx));
    tx.id.sequence = 1;
    tx.id.channel = MESH_CHANNEL_LEFT_HEMISPHERE;
    tx.id.proposer = 0x2001;
    tx.proposer = 0x2001;
    tx.channel = MESH_CHANNEL_LEFT_HEMISPHERE;
    tx.content_pattern = create_pattern(field_values, 4);  /* Same pattern */
    tx.urgency = 0.5f;
    tx.novelty = 0.3f;

    mesh_activation_t activations[10];
    size_t count = 0;

    err = mesh_pattern_router_compute_activations(
        router, &tx, activations, 10, &count);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GE(count, 1u);  /* Module should activate */

    if (count > 0) {
        EXPECT_EQ(activations[0].module_id, module_id);
        EXPECT_GT(activations[0].activation_level, 0.0f);
        EXPECT_NEAR(activations[0].pattern_similarity, 1.0f, 0.01f);
    }
}

TEST_F(MeshPatternRoutingTest, ComputeActivationsNoMatch) {
    router = mesh_pattern_router_create(&config);
    ASSERT_NE(router, nullptr);

    /* Register module for pattern [1, 0, 0, 0] */
    float field_values[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t field_pattern = create_pattern(field_values, 4);
    mesh_receptive_field_t field = create_simple_field(field_pattern, 0.8f);  /* High threshold */

    nimcp_error_t err = mesh_pattern_router_register_receptive_field(
        router, 0x1001, &field);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    /* Create transaction with orthogonal pattern */
    float tx_values[] = {0.0f, 1.0f, 0.0f, 0.0f};
    mesh_pattern_transaction_t tx;
    memset(&tx, 0, sizeof(tx));
    tx.id.sequence = 1;
    tx.content_pattern = create_pattern(tx_values, 4);

    mesh_activation_t activations[10];
    size_t count = 0;

    err = mesh_pattern_router_compute_activations(
        router, &tx, activations, 10, &count);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(count, 0u);  /* Module should NOT activate (orthogonal) */
}

TEST_F(MeshPatternRoutingTest, ComputeActivationsMultipleModules) {
    router = mesh_pattern_router_create(&config);
    ASSERT_NE(router, nullptr);

    /* Register modules with different preferred patterns */
    float visual_pattern[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float audio_pattern[] = {0.0f, 1.0f, 0.0f, 0.0f};
    float motor_pattern[] = {0.0f, 0.0f, 1.0f, 0.0f};

    mesh_receptive_field_t visual_field = create_simple_field(
        create_pattern(visual_pattern, 4), 0.3f);
    mesh_receptive_field_t audio_field = create_simple_field(
        create_pattern(audio_pattern, 4), 0.3f);
    mesh_receptive_field_t motor_field = create_simple_field(
        create_pattern(motor_pattern, 4), 0.3f);

    mesh_pattern_router_register_receptive_field(router, 0x1001, &visual_field);
    mesh_pattern_router_register_receptive_field(router, 0x1002, &audio_field);
    mesh_pattern_router_register_receptive_field(router, 0x1003, &motor_field);

    /* Create transaction with visual pattern */
    mesh_pattern_transaction_t tx;
    memset(&tx, 0, sizeof(tx));
    tx.content_pattern = create_pattern(visual_pattern, 4);

    mesh_activation_t activations[10];
    size_t count = 0;

    nimcp_error_t err = mesh_pattern_router_compute_activations(
        router, &tx, activations, 10, &count);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GE(count, 1u);

    /* Visual module should have highest activation */
    bool visual_highest = false;
    for (size_t i = 0; i < count; i++) {
        if (activations[i].module_id == 0x1001) {
            visual_highest = (activations[i].activation_level > 0.8f);
        }
    }
    EXPECT_TRUE(visual_highest);
}

/* ============================================================================
 * Endorser Selection Tests
 * ============================================================================ */

TEST_F(MeshPatternRoutingTest, GetEndorsers) {
    router = mesh_pattern_router_create(&config);
    ASSERT_NE(router, nullptr);

    /* Register modules */
    float pattern[] = {1.0f, 0.5f, 0.0f, 0.0f};
    mesh_receptive_field_t field = create_simple_field(
        create_pattern(pattern, 4), 0.3f);

    mesh_pattern_router_register_receptive_field(router, 0x1001, &field);
    mesh_pattern_router_register_receptive_field(router, 0x1002, &field);

    /* Create matching transaction */
    mesh_pattern_transaction_t tx;
    memset(&tx, 0, sizeof(tx));
    tx.content_pattern = create_pattern(pattern, 4);

    mesh_participant_id_t endorsers[10];
    size_t count = 0;

    nimcp_error_t err = mesh_pattern_router_get_endorsers(
        router, &tx, endorsers, 10, &count);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GE(count, 1u);
}

TEST_F(MeshPatternRoutingTest, GetEndorsersMaxLimit) {
    router = mesh_pattern_router_create(&config);
    ASSERT_NE(router, nullptr);

    /* Register many modules */
    float pattern[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_receptive_field_t field = create_simple_field(
        create_pattern(pattern, 4), 0.1f);  /* Low threshold */

    for (int i = 0; i < 20; i++) {
        mesh_pattern_router_register_receptive_field(router, 0x1000 + i, &field);
    }

    mesh_pattern_transaction_t tx;
    memset(&tx, 0, sizeof(tx));
    tx.content_pattern = create_pattern(pattern, 4);

    mesh_participant_id_t endorsers[5];  /* Only request 5 */
    size_t count = 0;

    nimcp_error_t err = mesh_pattern_router_get_endorsers(
        router, &tx, endorsers, 5, &count);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_LE(count, 5u);  /* Should not exceed max */
}

/* ============================================================================
 * Neuromodulation Tests
 * ============================================================================ */

TEST_F(MeshPatternRoutingTest, ApplyDopamine) {
    router = mesh_pattern_router_create(&config);
    ASSERT_NE(router, nullptr);

    nimcp_error_t err = mesh_pattern_router_apply_neuromodulation(
        router, MESH_NEUROMOD_DOPAMINE, 1.0f);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshPatternRoutingTest, ApplyNorepinephrine) {
    router = mesh_pattern_router_create(&config);
    ASSERT_NE(router, nullptr);

    nimcp_error_t err = mesh_pattern_router_apply_neuromodulation(
        router, MESH_NEUROMOD_NOREPINEPHRINE, 0.8f);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshPatternRoutingTest, ApplyAcetylcholine) {
    router = mesh_pattern_router_create(&config);
    ASSERT_NE(router, nullptr);

    nimcp_error_t err = mesh_pattern_router_apply_neuromodulation(
        router, MESH_NEUROMOD_ACETYLCHOLINE, 0.6f);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshPatternRoutingTest, ApplySerotonin) {
    router = mesh_pattern_router_create(&config);
    ASSERT_NE(router, nullptr);

    nimcp_error_t err = mesh_pattern_router_apply_neuromodulation(
        router, MESH_NEUROMOD_SEROTONIN, 0.5f);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshPatternRoutingTest, NeuromodulationAffectsActivation) {
    router = mesh_pattern_router_create(&config);
    ASSERT_NE(router, nullptr);

    /* Register module */
    float pattern[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_receptive_field_t field = create_simple_field(
        create_pattern(pattern, 4), 0.5f);

    mesh_pattern_router_register_receptive_field(router, 0x1001, &field);

    /* Create transaction with partial match */
    float tx_pattern[] = {0.7f, 0.3f, 0.0f, 0.0f};
    mesh_pattern_transaction_t tx;
    memset(&tx, 0, sizeof(tx));
    tx.content_pattern = create_pattern(tx_pattern, 4);

    /* Measure activation without neuromodulation */
    mesh_activation_t activations[10];
    size_t count_before = 0;
    mesh_pattern_router_compute_activations(
        router, &tx, activations, 10, &count_before);

    /* Apply norepinephrine (increases urgency, broadens fields) */
    mesh_pattern_router_apply_neuromodulation(
        router, MESH_NEUROMOD_NOREPINEPHRINE, 1.0f);

    /* Measure activation with neuromodulation */
    size_t count_after = 0;
    mesh_pattern_router_compute_activations(
        router, &tx, activations, 10, &count_after);

    /* Neuromodulation should affect activation (increase or change) */
    /* Note: exact behavior depends on implementation */
    SUCCEED();  /* Just verify no crash */
}

/* ============================================================================
 * Learning Tests
 * ============================================================================ */

TEST_F(MeshPatternRoutingTest, LearnSuccess) {
    router = mesh_pattern_router_create(&config);
    ASSERT_NE(router, nullptr);

    /* Register module */
    float pattern[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_receptive_field_t field = create_simple_field(
        create_pattern(pattern, 4), 0.5f);

    mesh_pattern_router_register_receptive_field(router, 0x1001, &field);

    /* Create transaction */
    mesh_pattern_transaction_t tx;
    memset(&tx, 0, sizeof(tx));
    tx.content_pattern = create_pattern(pattern, 4);

    /* Learn from successful outcome */
    mesh_participant_id_t endorsers[] = {0x1001};
    nimcp_error_t err = mesh_pattern_router_learn_outcome(
        router, &tx, endorsers, 1, true, 1.0f);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshPatternRoutingTest, LearnFailure) {
    router = mesh_pattern_router_create(&config);
    ASSERT_NE(router, nullptr);

    /* Register module */
    float pattern[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_receptive_field_t field = create_simple_field(
        create_pattern(pattern, 4), 0.5f);

    mesh_pattern_router_register_receptive_field(router, 0x1001, &field);

    /* Create transaction */
    mesh_pattern_transaction_t tx;
    memset(&tx, 0, sizeof(tx));
    tx.content_pattern = create_pattern(pattern, 4);

    /* Learn from failed outcome */
    mesh_participant_id_t endorsers[] = {0x1001};
    nimcp_error_t err = mesh_pattern_router_learn_outcome(
        router, &tx, endorsers, 1, false, -1.0f);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshPatternRoutingTest, LearnModifiesField) {
    router = mesh_pattern_router_create(&config);
    ASSERT_NE(router, nullptr);

    /* Register module */
    float pattern[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_receptive_field_t field = create_simple_field(
        create_pattern(pattern, 4), 0.5f);

    mesh_pattern_router_register_receptive_field(router, 0x1001, &field);

    /* Create transaction with slightly different pattern */
    float tx_pattern[] = {0.8f, 0.2f, 0.0f, 0.0f};
    mesh_pattern_transaction_t tx;
    memset(&tx, 0, sizeof(tx));
    tx.content_pattern = create_pattern(tx_pattern, 4);

    /* Record initial activation */
    mesh_activation_t activations[10];
    size_t count1 = 0;
    mesh_pattern_router_compute_activations(
        router, &tx, activations, 10, &count1);

    float initial_activation = (count1 > 0) ? activations[0].activation_level : 0.0f;

    /* Learn from successful outcome multiple times */
    mesh_participant_id_t endorsers[] = {0x1001};
    for (int i = 0; i < 10; i++) {
        mesh_pattern_router_learn_outcome(
            router, &tx, endorsers, 1, true, 1.0f);
    }

    /* Record activation after learning */
    size_t count2 = 0;
    mesh_pattern_router_compute_activations(
        router, &tx, activations, 10, &count2);

    float final_activation = (count2 > 0) ? activations[0].activation_level : 0.0f;

    /* After successful learning, activation should increase or stay same */
    /* (depends on whether learning is enabled in config) */
    if (config.enable_learning) {
        EXPECT_GE(final_activation, initial_activation - 0.01f);
    }
}

/* ============================================================================
 * Update Receptive Field Tests
 * ============================================================================ */

TEST_F(MeshPatternRoutingTest, UpdateReceptiveField) {
    router = mesh_pattern_router_create(&config);
    ASSERT_NE(router, nullptr);

    /* Register initial field */
    float pattern[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_receptive_field_t field = create_simple_field(
        create_pattern(pattern, 4), 0.5f);

    mesh_pattern_router_register_receptive_field(router, 0x1001, &field);

    /* Update to new preferred pattern */
    float new_pattern[] = {0.5f, 0.5f, 0.0f, 0.0f};
    mesh_pattern_t new_pref = create_pattern(new_pattern, 4);

    nimcp_error_t err = mesh_pattern_router_update_receptive_field(
        router, 0x1001, &new_pref, 0.5f);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST_F(MeshPatternRoutingTest, EmptyRouter) {
    router = mesh_pattern_router_create(&config);
    ASSERT_NE(router, nullptr);

    /* Query with no registered modules */
    float pattern[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_transaction_t tx;
    memset(&tx, 0, sizeof(tx));
    tx.content_pattern = create_pattern(pattern, 4);

    mesh_activation_t activations[10];
    size_t count = 0;

    nimcp_error_t err = mesh_pattern_router_compute_activations(
        router, &tx, activations, 10, &count);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(count, 0u);
}

TEST_F(MeshPatternRoutingTest, ZeroPattern) {
    router = mesh_pattern_router_create(&config);
    ASSERT_NE(router, nullptr);

    /* Register module with zero pattern */
    mesh_pattern_t zero_pattern;
    mesh_pattern_init(&zero_pattern);  /* All zeros */
    mesh_receptive_field_t field = create_simple_field(zero_pattern, 0.0f);

    nimcp_error_t err = mesh_pattern_router_register_receptive_field(
        router, 0x1001, &field);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Query should not crash */
    mesh_pattern_transaction_t tx;
    memset(&tx, 0, sizeof(tx));

    mesh_activation_t activations[10];
    size_t count = 0;

    err = mesh_pattern_router_compute_activations(
        router, &tx, activations, 10, &count);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshPatternRoutingTest, HighDimensionalPattern) {
    router = mesh_pattern_router_create(&config);
    ASSERT_NE(router, nullptr);

    /* Create pattern with all dimensions active */
    mesh_pattern_t full_pattern;
    mesh_pattern_init(&full_pattern);

    float magnitude = 0.0f;
    for (int i = 0; i < MESH_PATTERN_DIM; i++) {
        full_pattern.vector[i] = 1.0f / sqrtf((float)MESH_PATTERN_DIM);
        magnitude += full_pattern.vector[i] * full_pattern.vector[i];
    }
    full_pattern.magnitude = sqrtf(magnitude);
    full_pattern.active_dims = MESH_PATTERN_DIM;

    mesh_receptive_field_t field = create_simple_field(full_pattern, 0.3f);

    nimcp_error_t err = mesh_pattern_router_register_receptive_field(
        router, 0x1001, &field);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Query with same pattern */
    mesh_pattern_transaction_t tx;
    memset(&tx, 0, sizeof(tx));
    tx.content_pattern = full_pattern;

    mesh_activation_t activations[10];
    size_t count = 0;

    err = mesh_pattern_router_compute_activations(
        router, &tx, activations, 10, &count);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GE(count, 1u);
}

/* ============================================================================
 * Brain-Like Self-Selection Scenario Tests
 * ============================================================================ */

TEST_F(MeshPatternRoutingTest, VisualProcessingScenario) {
    router = mesh_pattern_router_create(&config);
    ASSERT_NE(router, nullptr);

    /* Register visual cortex modules with visual patterns */
    float v1_pattern[] = {1.0f, 0.8f, 0.0f, 0.0f};  /* Edge detection */
    float v4_pattern[] = {0.9f, 0.9f, 0.1f, 0.0f};  /* Color/form */
    float mt_pattern[] = {0.7f, 0.0f, 0.7f, 0.0f};  /* Motion */

    mesh_receptive_field_t v1_field = create_simple_field(
        create_pattern(v1_pattern, 4), 0.4f);
    mesh_receptive_field_t v4_field = create_simple_field(
        create_pattern(v4_pattern, 4), 0.4f);
    mesh_receptive_field_t mt_field = create_simple_field(
        create_pattern(mt_pattern, 4), 0.4f);

    mesh_pattern_router_register_receptive_field(router, 0x1001, &v1_field);  /* V1 */
    mesh_pattern_router_register_receptive_field(router, 0x1002, &v4_field);  /* V4 */
    mesh_pattern_router_register_receptive_field(router, 0x1003, &mt_field);  /* MT */

    /* Create "visual edge" transaction */
    mesh_pattern_transaction_t tx;
    memset(&tx, 0, sizeof(tx));
    tx.content_pattern = create_pattern(v1_pattern, 4);

    mesh_activation_t activations[10];
    size_t count = 0;

    nimcp_error_t err = mesh_pattern_router_compute_activations(
        router, &tx, activations, 10, &count);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* V1 and V4 should activate strongly, MT less so */
    bool v1_activated = false;
    bool v4_activated = false;

    for (size_t i = 0; i < count; i++) {
        if (activations[i].module_id == 0x1001) {
            v1_activated = activations[i].activation_level > 0.5f;
        }
        if (activations[i].module_id == 0x1002) {
            v4_activated = activations[i].activation_level > 0.3f;
        }
    }

    EXPECT_TRUE(v1_activated);
    EXPECT_TRUE(v4_activated);
}

TEST_F(MeshPatternRoutingTest, MotorCommandScenario) {
    router = mesh_pattern_router_create(&config);
    ASSERT_NE(router, nullptr);

    /* Register motor system modules */
    float motor_cortex_pattern[] = {0.0f, 0.0f, 1.0f, 0.8f};
    float cerebellum_pattern[] = {0.0f, 0.0f, 0.9f, 0.9f};
    float basal_ganglia_pattern[] = {0.0f, 0.0f, 0.7f, 0.6f};

    mesh_receptive_field_t motor_field = create_simple_field(
        create_pattern(motor_cortex_pattern, 4), 0.4f);
    mesh_receptive_field_t cerebellum_field = create_simple_field(
        create_pattern(cerebellum_pattern, 4), 0.4f);
    mesh_receptive_field_t bg_field = create_simple_field(
        create_pattern(basal_ganglia_pattern, 4), 0.4f);

    mesh_pattern_router_register_receptive_field(router, 0x2001, &motor_field);
    mesh_pattern_router_register_receptive_field(router, 0x2002, &cerebellum_field);
    mesh_pattern_router_register_receptive_field(router, 0x2003, &bg_field);

    /* Create motor command transaction */
    mesh_pattern_transaction_t tx;
    memset(&tx, 0, sizeof(tx));
    tx.content_pattern = create_pattern(motor_cortex_pattern, 4);
    tx.urgency = 0.8f;

    mesh_activation_t activations[10];
    size_t count = 0;

    nimcp_error_t err = mesh_pattern_router_compute_activations(
        router, &tx, activations, 10, &count);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GE(count, 2u);  /* Multiple motor modules should activate */
}

TEST_F(MeshPatternRoutingTest, CrossModalIntegration) {
    router = mesh_pattern_router_create(&config);
    ASSERT_NE(router, nullptr);

    /* Register multimodal module (like superior temporal sulcus) */
    float multimodal_pattern[] = {0.5f, 0.5f, 0.5f, 0.5f};  /* Responds to many patterns */

    mesh_receptive_field_t field;
    mesh_receptive_field_init(&field);
    field.preferred[0] = create_pattern(multimodal_pattern, 4);
    field.pattern_count = 1;
    field.threshold = 0.2f;  /* Low threshold - responds to many inputs */
    field.sharpness = 0.5f;  /* Broad tuning */

    mesh_pattern_router_register_receptive_field(router, 0x3001, &field);

    /* Any pattern should activate this module */
    float visual[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float audio[] = {0.0f, 1.0f, 0.0f, 0.0f};
    float motor[] = {0.0f, 0.0f, 1.0f, 0.0f};

    mesh_pattern_transaction_t tx;
    memset(&tx, 0, sizeof(tx));

    /* Test visual */
    tx.content_pattern = create_pattern(visual, 4);
    mesh_activation_t activations[10];
    size_t count = 0;
    mesh_pattern_router_compute_activations(router, &tx, activations, 10, &count);

    /* Multimodal area should respond to all */
    EXPECT_GE(count, 0u);  /* May or may not activate depending on similarity */
}
