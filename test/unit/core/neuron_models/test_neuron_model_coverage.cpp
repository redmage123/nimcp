/**
 * @file test_neuron_model_coverage.cpp
 * @brief Comprehensive tests for nimcp_neuron_model.c (TARGET: 100% coverage)
 *
 * WHAT: Test generic neuron model interface
 * WHY:  Achieve 100% line/branch coverage for nimcp_neuron_model.c
 * HOW:  Test all public functions with valid/invalid inputs
 *
 * COVERAGE GOALS:
 * - Line coverage: 100%
 * - Branch coverage: 100%
 * - Function coverage: 100%
 *
 * @author NIMCP Development Team
 * @date 2025-11-10
 */

#include <gtest/gtest.h>

#include "core/neuron_models/nimcp_neuron_model.h"
#include "core/neuron_models/nimcp_neuron_model_internal.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * Mock neuron model for testing
 *
 * WHAT: Minimal neuron model implementation
 * WHY:  Test the plugin framework without complex model logic
 * HOW:  Implement all required vtable functions
 */
struct mock_neuron_state {
    float voltage;
    bool spiked;
    uint32_t update_count;
};

// Mock vtable functions
static void mock_init(neuron_model_state_t state, const void* params) {
    struct mock_neuron_state* s = (struct mock_neuron_state*)state->model_state;
    s->voltage = 0.0f;
    s->spiked = false;
    s->update_count = 0;

    // Apply params if provided
    if (params) {
        const float* v = (const float*)params;
        s->voltage = *v;
    }
}

static void mock_destroy(neuron_model_state_t state) {
    // Nothing to clean up for mock
    (void)state;
}

static void mock_update(neuron_model_state_t state, float dt, float input_current) {
    struct mock_neuron_state* s = (struct mock_neuron_state*)state->model_state;
    s->voltage += input_current * dt;
    s->update_count++;

    // Spike if voltage > 1.0
    if (s->voltage > 1.0f) {
        s->spiked = true;
    }
}

static bool mock_check_spike(const neuron_model_state_t state) {
    const struct mock_neuron_state* s = (const struct mock_neuron_state*)state->model_state;
    return s->spiked;
}

static void mock_post_spike(neuron_model_state_t state) {
    struct mock_neuron_state* s = (struct mock_neuron_state*)state->model_state;
    s->voltage = 0.0f;
    s->spiked = false;
}

static float mock_get_voltage(const neuron_model_state_t state) {
    const struct mock_neuron_state* s = (const struct mock_neuron_state*)state->model_state;
    return s->voltage;
}

static void mock_set_voltage(neuron_model_state_t state, float voltage) {
    struct mock_neuron_state* s = (struct mock_neuron_state*)state->model_state;
    s->voltage = voltage;
}

static void mock_reset(neuron_model_state_t state) {
    struct mock_neuron_state* s = (struct mock_neuron_state*)state->model_state;
    s->voltage = 0.0f;
    s->spiked = false;
    s->update_count = 0;
}

// Complete mock vtable
static const neuron_model_vtable_t mock_vtable = {
    .name = "MockNeuron",
    .type = NEURON_MODEL_LIF,
    .state_size = sizeof(struct mock_neuron_state),
    .init = mock_init,
    .destroy = mock_destroy,
    .update = mock_update,
    .check_spike = mock_check_spike,
    .post_spike = mock_post_spike,
    .get_voltage = mock_get_voltage,
    .set_voltage = mock_set_voltage,
    .reset = mock_reset
};

// Minimal vtable (only required functions)
static const neuron_model_vtable_t minimal_vtable = {
    .name = "MinimalNeuron",
    .type = NEURON_MODEL_IZHIKEVICH,
    .state_size = sizeof(struct mock_neuron_state),
    .init = mock_init,
    .destroy = NULL,  // Optional
    .update = mock_update,
    .check_spike = mock_check_spike,
    .post_spike = mock_post_spike,
    .get_voltage = mock_get_voltage,
    .set_voltage = mock_set_voltage,
    .reset = mock_reset
};

//=============================================================================
// Test Suite: Factory Functions
//=============================================================================

class NeuronModelFactoryTest : public ::testing::Test {
protected:
    void TearDown() override {
        // Clean up any leaked memory
    }
};

TEST_F(NeuronModelFactoryTest, CreateValidModel) {
    // Create with valid vtable
    neuron_model_state_t state = neuron_model_create(&mock_vtable, NULL);

    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->vtable, &mock_vtable);

    neuron_model_destroy(state);
}

TEST_F(NeuronModelFactoryTest, CreateWithParams) {
    // Create with initialization parameters
    float init_voltage = 0.5f;
    neuron_model_state_t state = neuron_model_create(&mock_vtable, &init_voltage);

    ASSERT_NE(state, nullptr);
    EXPECT_FLOAT_EQ(neuron_model_get_voltage(state), 0.5f);

    neuron_model_destroy(state);
}

TEST_F(NeuronModelFactoryTest, CreateNullVtable) {
    // Guard: NULL vtable should return NULL
    neuron_model_state_t state = neuron_model_create(NULL, NULL);
    EXPECT_EQ(state, nullptr);
}

TEST_F(NeuronModelFactoryTest, CreateInvalidVtable_NullInit) {
    // Guard: vtable without init function
    neuron_model_vtable_t invalid_vtable = mock_vtable;
    invalid_vtable.init = NULL;

    neuron_model_state_t state = neuron_model_create(&invalid_vtable, NULL);
    EXPECT_EQ(state, nullptr);
}

TEST_F(NeuronModelFactoryTest, CreateInvalidVtable_ZeroStateSize) {
    // Guard: vtable with zero state size
    neuron_model_vtable_t invalid_vtable = mock_vtable;
    invalid_vtable.state_size = 0;

    neuron_model_state_t state = neuron_model_create(&invalid_vtable, NULL);
    EXPECT_EQ(state, nullptr);
}

TEST_F(NeuronModelFactoryTest, DestroyNull) {
    // Guard: destroying NULL should be safe
    neuron_model_destroy(NULL);
    // If we get here, test passed (no crash)
    SUCCEED();
}

TEST_F(NeuronModelFactoryTest, DestroyWithDestructor) {
    // Destroy with destructor function
    neuron_model_state_t state = neuron_model_create(&mock_vtable, NULL);
    ASSERT_NE(state, nullptr);

    neuron_model_destroy(state);
    // No crash = success
    SUCCEED();
}

TEST_F(NeuronModelFactoryTest, DestroyWithoutDestructor) {
    // Destroy with NULL destructor (optional)
    neuron_model_state_t state = neuron_model_create(&minimal_vtable, NULL);
    ASSERT_NE(state, nullptr);

    neuron_model_destroy(state);
    SUCCEED();
}

//=============================================================================
// Test Suite: Dynamics Functions
//=============================================================================

class NeuronModelDynamicsTest : public ::testing::Test {
protected:
    neuron_model_state_t state;

    void SetUp() override {
        state = neuron_model_create(&mock_vtable, NULL);
        ASSERT_NE(state, nullptr);
    }

    void TearDown() override {
        neuron_model_destroy(state);
    }
};

TEST_F(NeuronModelDynamicsTest, UpdateNormal) {
    // Update with valid inputs
    neuron_model_update(state, 0.001f, 10.0f);

    struct mock_neuron_state* s = (struct mock_neuron_state*)state->model_state;
    EXPECT_EQ(s->update_count, 1);
    EXPECT_GT(s->voltage, 0.0f);
}

TEST_F(NeuronModelDynamicsTest, UpdateNullState) {
    // Guard: NULL state
    neuron_model_update(NULL, 0.001f, 10.0f);
    SUCCEED();  // No crash
}

TEST_F(NeuronModelDynamicsTest, CheckSpikeAfterUpdate) {
    // Update until spike
    neuron_model_update(state, 0.001f, 1500.0f);  // Large current

    EXPECT_TRUE(neuron_model_check_spike(state));
}

TEST_F(NeuronModelDynamicsTest, CheckSpikeNullState) {
    // Guard: NULL state
    EXPECT_FALSE(neuron_model_check_spike(NULL));
}

TEST_F(NeuronModelDynamicsTest, PostSpike) {
    // Trigger spike
    neuron_model_update(state, 0.001f, 1500.0f);
    EXPECT_TRUE(neuron_model_check_spike(state));

    // Reset after spike
    neuron_model_post_spike(state);

    EXPECT_FALSE(neuron_model_check_spike(state));
    EXPECT_FLOAT_EQ(neuron_model_get_voltage(state), 0.0f);
}

TEST_F(NeuronModelDynamicsTest, PostSpikeNullState) {
    // Guard: NULL state
    neuron_model_post_spike(NULL);
    SUCCEED();
}

//=============================================================================
// Test Suite: State Access Functions
//=============================================================================

class NeuronModelStateTest : public ::testing::Test {
protected:
    neuron_model_state_t state;

    void SetUp() override {
        state = neuron_model_create(&mock_vtable, NULL);
        ASSERT_NE(state, nullptr);
    }

    void TearDown() override {
        neuron_model_destroy(state);
    }
};

TEST_F(NeuronModelStateTest, GetVoltage) {
    // Initial voltage should be 0
    EXPECT_FLOAT_EQ(neuron_model_get_voltage(state), 0.0f);
}

TEST_F(NeuronModelStateTest, GetVoltageNullState) {
    // Guard: NULL state
    EXPECT_FLOAT_EQ(neuron_model_get_voltage(NULL), 0.0f);
}

TEST_F(NeuronModelStateTest, SetVoltage) {
    // Set voltage
    neuron_model_set_voltage(state, 0.75f);
    EXPECT_FLOAT_EQ(neuron_model_get_voltage(state), 0.75f);
}

TEST_F(NeuronModelStateTest, SetVoltageNullState) {
    // Guard: NULL state
    neuron_model_set_voltage(NULL, 0.75f);
    SUCCEED();
}

TEST_F(NeuronModelStateTest, Reset) {
    // Modify state
    neuron_model_set_voltage(state, 0.5f);
    neuron_model_update(state, 0.001f, 100.0f);

    // Reset
    neuron_model_reset(state);

    EXPECT_FLOAT_EQ(neuron_model_get_voltage(state), 0.0f);
    struct mock_neuron_state* s = (struct mock_neuron_state*)state->model_state;
    EXPECT_EQ(s->update_count, 0);
}

TEST_F(NeuronModelStateTest, ResetNullState) {
    // Guard: NULL state
    neuron_model_reset(NULL);
    SUCCEED();
}

//=============================================================================
// Test Suite: Introspection Functions
//=============================================================================

class NeuronModelIntrospectionTest : public ::testing::Test {
protected:
    neuron_model_state_t state;

    void SetUp() override {
        state = neuron_model_create(&mock_vtable, NULL);
        ASSERT_NE(state, nullptr);
    }

    void TearDown() override {
        neuron_model_destroy(state);
    }
};

TEST_F(NeuronModelIntrospectionTest, GetName) {
    const char* name = neuron_model_get_name(state);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "MockNeuron");
}

TEST_F(NeuronModelIntrospectionTest, GetNameNullState) {
    // Guard: NULL state returns "unknown"
    const char* name = neuron_model_get_name(NULL);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "unknown");
}

TEST_F(NeuronModelIntrospectionTest, GetType) {
    neuron_model_type_t type = neuron_model_get_type(state);
    EXPECT_EQ(type, NEURON_MODEL_LIF);
}

TEST_F(NeuronModelIntrospectionTest, GetTypeNullState) {
    // Guard: NULL state returns default
    neuron_model_type_t type = neuron_model_get_type(NULL);
    EXPECT_EQ(type, NEURON_MODEL_LIF);
}

//=============================================================================
// Test Suite: Edge Cases and Integration
//=============================================================================

class NeuronModelEdgeCasesTest : public ::testing::Test {};

TEST_F(NeuronModelEdgeCasesTest, MultipleUpdates) {
    neuron_model_state_t state = neuron_model_create(&mock_vtable, NULL);
    ASSERT_NE(state, nullptr);

    // Many updates
    for (int i = 0; i < 100; i++) {
        neuron_model_update(state, 0.001f, 5.0f);
    }

    struct mock_neuron_state* s = (struct mock_neuron_state*)state->model_state;
    EXPECT_EQ(s->update_count, 100);
    EXPECT_GT(s->voltage, 0.0f);

    neuron_model_destroy(state);
}

TEST_F(NeuronModelEdgeCasesTest, SpikeResetCycle) {
    neuron_model_state_t state = neuron_model_create(&mock_vtable, NULL);
    ASSERT_NE(state, nullptr);

    // Spike-reset cycle
    for (int cycle = 0; cycle < 10; cycle++) {
        // Drive to spike
        while (!neuron_model_check_spike(state)) {
            neuron_model_update(state, 0.001f, 200.0f);
        }

        EXPECT_TRUE(neuron_model_check_spike(state));

        // Reset
        neuron_model_post_spike(state);
        EXPECT_FALSE(neuron_model_check_spike(state));
    }

    neuron_model_destroy(state);
}

TEST_F(NeuronModelEdgeCasesTest, DifferentModelTypes) {
    // Test with minimal vtable
    neuron_model_state_t state1 = neuron_model_create(&minimal_vtable, NULL);
    ASSERT_NE(state1, nullptr);
    EXPECT_EQ(neuron_model_get_type(state1), NEURON_MODEL_IZHIKEVICH);

    // Test with mock vtable
    neuron_model_state_t state2 = neuron_model_create(&mock_vtable, NULL);
    ASSERT_NE(state2, nullptr);
    EXPECT_EQ(neuron_model_get_type(state2), NEURON_MODEL_LIF);

    neuron_model_destroy(state1);
    neuron_model_destroy(state2);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
