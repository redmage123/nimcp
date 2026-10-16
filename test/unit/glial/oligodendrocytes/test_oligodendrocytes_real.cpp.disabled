/**
 * @file test_oligodendrocytes_real.cpp
 * @brief Real tests for oligodendrocyte glial cells
 *
 * COVERAGE TARGET: oligodendrocytes module (currently 0%)
 * APPROACH: Test all real functions with actual instances
 * FOCUS: Myelination, neuron assignment, activity tracking, conduction velocity, ATP management
 */

#include <gtest/gtest.h>
#include <cmath>

#include "glial/oligodendrocytes/nimcp_oligodendrocytes.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"

//=============================================================================
// Test Fixture
//=============================================================================

class OligodendrocytesRealTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();
    }

    void TearDown() override {
        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_EQ(stats.current_allocated, 0) << "Memory leak detected";
    }
};

//=============================================================================
// Creation and Destruction Tests
//=============================================================================

TEST_F(OligodendrocytesRealTest, CreateDestroy) {
    oligodendrocyte_t* oligo = oligodendrocyte_create(0, 50);

    ASSERT_NE(oligo, nullptr);
    EXPECT_EQ(oligo->id, 0);
    EXPECT_EQ(oligo->max_neurons, 50);
    EXPECT_EQ(oligo->num_myelinated_neurons, 0);
    EXPECT_FLOAT_EQ(oligo->atp_level, 1.0f);

    oligodendrocyte_destroy(oligo);
}

TEST_F(OligodendrocytesRealTest, CreateWithDifferentCapacity) {
    oligodendrocyte_t* oligo = oligodendrocyte_create(42, 30);

    ASSERT_NE(oligo, nullptr);
    EXPECT_EQ(oligo->id, 42);
    EXPECT_EQ(oligo->max_neurons, 30);

    oligodendrocyte_destroy(oligo);
}

TEST_F(OligodendrocytesRealTest, DestroyNull) {
    // Should handle null gracefully
    oligodendrocyte_destroy(nullptr);
    SUCCEED();
}

TEST_F(OligodendrocytesRealTest, MultipleCreateDestroy) {
    const int count = 50;
    oligodendrocyte_t* oligos[count];

    for (int i = 0; i < count; i++) {
        oligos[i] = oligodendrocyte_create(i, 50);
        ASSERT_NE(oligos[i], nullptr);
    }

    for (int i = 0; i < count; i++) {
        oligodendrocyte_destroy(oligos[i]);
    }
}

//=============================================================================
// Neuron Assignment Tests
//=============================================================================

TEST_F(OligodendrocytesRealTest, AssignNeuron_Single) {
    oligodendrocyte_t* oligo = oligodendrocyte_create(0, 50);
    ASSERT_NE(oligo, nullptr);

    nimcp_result_t result = oligodendrocyte_assign_neuron(oligo, 5);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(oligo->num_myelinated_neurons, 1);

    oligodendrocyte_destroy(oligo);
}

TEST_F(OligodendrocytesRealTest, AssignNeuron_Multiple) {
    oligodendrocyte_t* oligo = oligodendrocyte_create(0, 50);
    ASSERT_NE(oligo, nullptr);

    // Assign multiple neurons
    for (uint32_t i = 1; i <= 20; i++) {
        nimcp_result_t result = oligodendrocyte_assign_neuron(oligo, i);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    EXPECT_EQ(oligo->num_myelinated_neurons, 20);

    oligodendrocyte_destroy(oligo);
}

TEST_F(OligodendrocytesRealTest, AssignNeuron_UpToCapacity) {
    oligodendrocyte_t* oligo = oligodendrocyte_create(0, 10);
    ASSERT_NE(oligo, nullptr);

    // Fill to capacity
    for (uint32_t i = 1; i <= 10; i++) {
        oligodendrocyte_assign_neuron(oligo, i);
    }

    EXPECT_EQ(oligo->num_myelinated_neurons, 10);

    oligodendrocyte_destroy(oligo);
}

TEST_F(OligodendrocytesRealTest, AssignNeuron_Duplicate) {
    oligodendrocyte_t* oligo = oligodendrocyte_create(0, 50);
    ASSERT_NE(oligo, nullptr);

    oligodendrocyte_assign_neuron(oligo, 5);
    uint32_t count_before = oligo->num_myelinated_neurons;

    // Assign same neuron again (should be idempotent)
    oligodendrocyte_assign_neuron(oligo, 5);
    uint32_t count_after = oligo->num_myelinated_neurons;

    // Count should not increase
    EXPECT_EQ(count_before, count_after);

    oligodendrocyte_destroy(oligo);
}

//=============================================================================
// Myelination Level Tests
//=============================================================================

TEST_F(OligodendrocytesRealTest, GetMyelinationLevel_Initial) {
    oligodendrocyte_t* oligo = oligodendrocyte_create(0, 50);
    ASSERT_NE(oligo, nullptr);

    oligodendrocyte_assign_neuron(oligo, 5);

    // Initial myelination should be 0.0 (unmyelinated)
    float level = oligodendrocyte_get_myelination_level(oligo, 5);
    EXPECT_FLOAT_EQ(level, 0.0f);

    oligodendrocyte_destroy(oligo);
}

TEST_F(OligodendrocytesRealTest, GetMyelinationLevel_NotAssigned) {
    oligodendrocyte_t* oligo = oligodendrocyte_create(0, 50);
    ASSERT_NE(oligo, nullptr);

    // Get level for neuron not assigned
    float level = oligodendrocyte_get_myelination_level(oligo, 99);
    EXPECT_FLOAT_EQ(level, 0.0f);

    oligodendrocyte_destroy(oligo);
}

//=============================================================================
// Conduction Velocity Tests
//=============================================================================

TEST_F(OligodendrocytesRealTest, ComputeConductionVelocity_Unmyelinated) {
    oligodendrocyte_t* oligo = oligodendrocyte_create(0, 50);
    ASSERT_NE(oligo, nullptr);

    oligodendrocyte_assign_neuron(oligo, 5);

    float base_velocity = 1.0f;
    float velocity = oligodendrocyte_compute_conduction_velocity(oligo, 5, base_velocity);

    // Unmyelinated velocity should equal base velocity
    EXPECT_FLOAT_EQ(velocity, base_velocity);

    oligodendrocyte_destroy(oligo);
}

TEST_F(OligodendrocytesRealTest, ComputeConductionVelocity_NotAssigned) {
    oligodendrocyte_t* oligo = oligodendrocyte_create(0, 50);
    ASSERT_NE(oligo, nullptr);

    float base_velocity = 1.0f;
    float velocity = oligodendrocyte_compute_conduction_velocity(oligo, 99, base_velocity);

    // Non-assigned neuron should return base velocity
    EXPECT_FLOAT_EQ(velocity, base_velocity);

    oligodendrocyte_destroy(oligo);
}

TEST_F(OligodendrocytesRealTest, ComputeConductionVelocity_VaryBaseVelocity) {
    oligodendrocyte_t* oligo = oligodendrocyte_create(0, 50);
    ASSERT_NE(oligo, nullptr);

    oligodendrocyte_assign_neuron(oligo, 5);

    // Test with different base velocities
    for (float base = 0.5f; base <= 2.0f; base += 0.5f) {
        float velocity = oligodendrocyte_compute_conduction_velocity(oligo, 5, base);
        EXPECT_GE(velocity, base);
    }

    oligodendrocyte_destroy(oligo);
}

//=============================================================================
// Activity Tracking Tests
//=============================================================================

TEST_F(OligodendrocytesRealTest, TrackActivity_Single) {
    oligodendrocyte_t* oligo = oligodendrocyte_create(0, 50);
    ASSERT_NE(oligo, nullptr);

    oligodendrocyte_assign_neuron(oligo, 5);

    // Track activity
    oligodendrocyte_track_activity(oligo, 5, 1.0f, 1000);

    SUCCEED();

    oligodendrocyte_destroy(oligo);
}

TEST_F(OligodendrocytesRealTest, TrackActivity_Multiple) {
    oligodendrocyte_t* oligo = oligodendrocyte_create(0, 50);
    ASSERT_NE(oligo, nullptr);

    oligodendrocyte_assign_neuron(oligo, 5);

    // Track multiple activity events
    for (int i = 0; i < 10; i++) {
        oligodendrocyte_track_activity(oligo, 5, 1.0f, 1000 + i * 1000);
    }

    SUCCEED();

    oligodendrocyte_destroy(oligo);
}

TEST_F(OligodendrocytesRealTest, TrackActivity_NotAssigned) {
    oligodendrocyte_t* oligo = oligodendrocyte_create(0, 50);
    ASSERT_NE(oligo, nullptr);

    // Track activity for neuron not assigned (should not crash)
    oligodendrocyte_track_activity(oligo, 99, 1.0f, 1000);

    SUCCEED();

    oligodendrocyte_destroy(oligo);
}

TEST_F(OligodendrocytesRealTest, TrackActivity_VaryingLevels) {
    oligodendrocyte_t* oligo = oligodendrocyte_create(0, 50);
    ASSERT_NE(oligo, nullptr);

    oligodendrocyte_assign_neuron(oligo, 5);

    // Track with varying activity levels
    for (int i = 0; i < 10; i++) {
        float activity = i / 10.0f;
        oligodendrocyte_track_activity(oligo, 5, activity, 1000 + i * 1000);
    }

    SUCCEED();

    oligodendrocyte_destroy(oligo);
}

//=============================================================================
// Myelination Remodeling Tests
//=============================================================================

TEST_F(OligodendrocytesRealTest, RemodelMyelination) {
    oligodendrocyte_t* oligo = oligodendrocyte_create(0, 50);
    ASSERT_NE(oligo, nullptr);

    oligodendrocyte_assign_neuron(oligo, 5);
    oligodendrocyte_track_activity(oligo, 5, 1.0f, 1000);

    // Remodel with different time steps
    oligodendrocyte_remodel_myelination(oligo, 0.1f);
    oligodendrocyte_remodel_myelination(oligo, 0.1f);

    SUCCEED();

    oligodendrocyte_destroy(oligo);
}

TEST_F(OligodendrocytesRealTest, RemodelMyelination_MultipleSteps) {
    oligodendrocyte_t* oligo = oligodendrocyte_create(0, 50);
    ASSERT_NE(oligo, nullptr);

    oligodendrocyte_assign_neuron(oligo, 5);
    oligodendrocyte_track_activity(oligo, 5, 1.0f, 1000);

    // Many remodeling steps
    for (int i = 0; i < 100; i++) {
        oligodendrocyte_remodel_myelination(oligo, 0.01f);
    }

    SUCCEED();

    oligodendrocyte_destroy(oligo);
}

TEST_F(OligodendrocytesRealTest, RemodelMyelination_VaryingDt) {
    oligodendrocyte_t* oligo = oligodendrocyte_create(0, 50);
    ASSERT_NE(oligo, nullptr);

    oligodendrocyte_assign_neuron(oligo, 5);

    // Test with different time steps
    oligodendrocyte_remodel_myelination(oligo, 0.001f);
    oligodendrocyte_remodel_myelination(oligo, 0.01f);
    oligodendrocyte_remodel_myelination(oligo, 0.1f);
    oligodendrocyte_remodel_myelination(oligo, 1.0f);

    SUCCEED();

    oligodendrocyte_destroy(oligo);
}

//=============================================================================
// ATP Management Tests
//=============================================================================

TEST_F(OligodendrocytesRealTest, UpdateATP_Initial) {
    oligodendrocyte_t* oligo = oligodendrocyte_create(0, 50);
    ASSERT_NE(oligo, nullptr);

    // Initial ATP should be 1.0
    EXPECT_FLOAT_EQ(oligo->atp_level, 1.0f);

    oligodendrocyte_destroy(oligo);
}

TEST_F(OligodendrocytesRealTest, UpdateATP) {
    oligodendrocyte_t* oligo = oligodendrocyte_create(0, 50);
    ASSERT_NE(oligo, nullptr);

    oligodendrocyte_update_atp(oligo, 0.1f);

    // ATP should remain within bounds
    EXPECT_GE(oligo->atp_level, 0.0f);
    EXPECT_LE(oligo->atp_level, 1.0f);

    oligodendrocyte_destroy(oligo);
}

TEST_F(OligodendrocytesRealTest, UpdateATP_MultipleSteps) {
    oligodendrocyte_t* oligo = oligodendrocyte_create(0, 50);
    ASSERT_NE(oligo, nullptr);

    for (int i = 0; i < 100; i++) {
        oligodendrocyte_update_atp(oligo, 0.01f);
    }

    // ATP should remain bounded
    EXPECT_GE(oligo->atp_level, 0.0f);
    EXPECT_LE(oligo->atp_level, 1.0f);

    oligodendrocyte_destroy(oligo);
}

//=============================================================================
// Total Myelination Tests
//=============================================================================

TEST_F(OligodendrocytesRealTest, GetTotalMyelination_Initial) {
    oligodendrocyte_t* oligo = oligodendrocyte_create(0, 50);
    ASSERT_NE(oligo, nullptr);

    float total = oligodendrocyte_get_total_myelination(oligo);

    // Initially should be 0 (no neurons assigned)
    EXPECT_FLOAT_EQ(total, 0.0f);

    oligodendrocyte_destroy(oligo);
}

TEST_F(OligodendrocytesRealTest, GetTotalMyelination_WithNeurons) {
    oligodendrocyte_t* oligo = oligodendrocyte_create(0, 50);
    ASSERT_NE(oligo, nullptr);

    // Assign multiple neurons
    for (uint32_t i = 1; i <= 10; i++) {
        oligodendrocyte_assign_neuron(oligo, i);
    }

    float total = oligodendrocyte_get_total_myelination(oligo);

    // Total should be sum of all myelination levels (initially 0)
    EXPECT_GE(total, 0.0f);

    oligodendrocyte_destroy(oligo);
}

//=============================================================================
// Network Management Tests
//=============================================================================

TEST_F(OligodendrocytesRealTest, NetworkCreate) {
    oligodendrocyte_network_t* network = oligodendrocyte_network_create(10);

    ASSERT_NE(network, nullptr);
    EXPECT_EQ(network->num_oligodendrocytes, 0);
    EXPECT_EQ(network->capacity, 10);

    oligodendrocyte_network_destroy(network);
}

TEST_F(OligodendrocytesRealTest, NetworkAdd) {
    oligodendrocyte_network_t* network = oligodendrocyte_network_create(10);
    ASSERT_NE(network, nullptr);

    oligodendrocyte_t* oligo = oligodendrocyte_create(0, 50);
    ASSERT_NE(oligo, nullptr);

    nimcp_result_t result = oligodendrocyte_network_add(network, oligo);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(network->num_oligodendrocytes, 1);

    oligodendrocyte_network_destroy(network);
}

TEST_F(OligodendrocytesRealTest, NetworkAddMultiple) {
    oligodendrocyte_network_t* network = oligodendrocyte_network_create(10);
    ASSERT_NE(network, nullptr);

    for (int i = 0; i < 5; i++) {
        oligodendrocyte_t* oligo = oligodendrocyte_create(i, 50);
        oligodendrocyte_network_add(network, oligo);
    }

    EXPECT_EQ(network->num_oligodendrocytes, 5);

    oligodendrocyte_network_destroy(network);
}

TEST_F(OligodendrocytesRealTest, NetworkStep) {
    oligodendrocyte_network_t* network = oligodendrocyte_network_create(10);
    ASSERT_NE(network, nullptr);

    oligodendrocyte_t* oligo = oligodendrocyte_create(0, 50);
    oligodendrocyte_network_add(network, oligo);

    // Step network
    oligodendrocyte_network_step(network, 0.1f);
    oligodendrocyte_network_step(network, 0.1f);
    oligodendrocyte_network_step(network, 0.1f);

    SUCCEED();

    oligodendrocyte_network_destroy(network);
}

TEST_F(OligodendrocytesRealTest, NetworkFindByNeuron_NotFound) {
    oligodendrocyte_network_t* network = oligodendrocyte_network_create(10);
    ASSERT_NE(network, nullptr);

    oligodendrocyte_t* found = oligodendrocyte_network_find_by_neuron(network, 99);
    EXPECT_EQ(found, nullptr);

    oligodendrocyte_network_destroy(network);
}

TEST_F(OligodendrocytesRealTest, NetworkFindByNeuron_Found) {
    oligodendrocyte_network_t* network = oligodendrocyte_network_create(10);
    ASSERT_NE(network, nullptr);

    oligodendrocyte_t* oligo = oligodendrocyte_create(0, 50);
    oligodendrocyte_assign_neuron(oligo, 5);
    oligodendrocyte_network_add(network, oligo);

    oligodendrocyte_t* found = oligodendrocyte_network_find_by_neuron(network, 5);
    EXPECT_EQ(found, oligo);

    oligodendrocyte_network_destroy(network);
}

//=============================================================================
// Integration Tests (Activity -> Remodeling -> Velocity)
//=============================================================================

TEST_F(OligodendrocytesRealTest, FullCycle_ActivityToVelocity) {
    oligodendrocyte_t* oligo = oligodendrocyte_create(0, 50);
    ASSERT_NE(oligo, nullptr);

    oligodendrocyte_assign_neuron(oligo, 5);

    // Track high activity
    for (int i = 0; i < 10; i++) {
        oligodendrocyte_track_activity(oligo, 5, 1.0f, 1000 + i * 1000);
    }

    // Remodel (increase myelination based on activity)
    for (int i = 0; i < 10; i++) {
        oligodendrocyte_remodel_myelination(oligo, 0.1f);
    }

    // Check velocity boost
    float base_velocity = 1.0f;
    float velocity = oligodendrocyte_compute_conduction_velocity(oligo, 5, base_velocity);
    EXPECT_GE(velocity, base_velocity);

    oligodendrocyte_destroy(oligo);
}

TEST_F(OligodendrocytesRealTest, MultipleNeurons_DifferentActivity) {
    oligodendrocyte_t* oligo = oligodendrocyte_create(0, 50);
    ASSERT_NE(oligo, nullptr);

    // Assign multiple neurons
    for (uint32_t i = 1; i <= 10; i++) {
        oligodendrocyte_assign_neuron(oligo, i);
    }

    // Track varying activity levels
    for (uint32_t i = 1; i <= 10; i++) {
        float activity = i / 10.0f;
        oligodendrocyte_track_activity(oligo, i, activity, 1000);
    }

    // Remodel
    oligodendrocyte_remodel_myelination(oligo, 0.1f);

    SUCCEED();

    oligodendrocyte_destroy(oligo);
}

TEST_F(OligodendrocytesRealTest, LongTermSimulation) {
    oligodendrocyte_t* oligo = oligodendrocyte_create(0, 50);
    ASSERT_NE(oligo, nullptr);

    oligodendrocyte_assign_neuron(oligo, 5);

    // Simulate long-term activity and remodeling
    for (int step = 0; step < 1000; step++) {
        float activity = (step % 10 < 5) ? 1.0f : 0.0f;
        oligodendrocyte_track_activity(oligo, 5, activity, step * 1000);

        if (step % 10 == 0) {
            oligodendrocyte_remodel_myelination(oligo, 0.1f);
            oligodendrocyte_update_atp(oligo, 0.1f);
        }
    }

    // Check final state
    float level = oligodendrocyte_get_myelination_level(oligo, 5);
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 1.0f);

    oligodendrocyte_destroy(oligo);
}
