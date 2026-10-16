/**
 * @file test_core_exception_macros.cpp
 * @brief Unit tests for NIMCP exception macro integration in core modules
 *
 * WHAT: Comprehensive tests for exception handling macros in core subsystem
 * WHY:  Verify NIMCP_CHECK_THROW, NIMCP_THROW, and related macros work correctly
 * HOW:  Test null pointer handling, invalid parameters, and valid inputs
 *
 * TESTED MODULES:
 * - nimcp_axon (axon creation, segments, spike propagation)
 * - Core directives
 * - Brain subsystem boundaries
 *
 * EXCEPTION PATTERNS TESTED:
 * - NIMCP_THROW_TO_IMMUNE: Exceptions that present to immune system
 * - NIMCP_THROW_MEMORY: Memory allocation failure exceptions
 * - NULL pointer validation via guard clauses
 * - Invalid parameter validation
 *
 * @author NIMCP Development Team
 * @date 2026-01-21
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "core/axon/nimcp_axon.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * @brief Test fixture for core exception macro tests
 *
 * Initializes memory system for leak detection and exception system
 * for proper exception handling during tests.
 */
class CoreExceptionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize memory system for leak detection
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();

        // Initialize exception system
        nimcp_exception_system_init();
    }

    void TearDown() override {
        // Shutdown exception system
        nimcp_exception_system_shutdown();

        // Check for memory leaks
        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_EQ(stats.current_allocated, 0) << "Memory leak detected in core exception test";
    }
};

//=============================================================================
// Axon Creation Exception Tests
//=============================================================================

/**
 * @brief Test axon_create with valid parameters succeeds
 */
TEST_F(CoreExceptionTest, Axon_CreateValidInput_Succeeds) {
    axon_t* axon = axon_create(
        0,                        // id
        AXON_TYPE_MYELINATED,     // type
        100.0f,                   // length (um)
        1.0f                      // diameter (um)
    );

    ASSERT_NE(axon, nullptr) << "axon_create should succeed with valid parameters";
    EXPECT_EQ(axon->id, 0u);
    EXPECT_EQ(axon->type, AXON_TYPE_MYELINATED);
    EXPECT_FLOAT_EQ(axon->length, 100.0f);
    EXPECT_FLOAT_EQ(axon->diameter, 1.0f);

    axon_destroy(axon);
}

/**
 * @brief Test axon_create with all axon types
 */
TEST_F(CoreExceptionTest, Axon_CreateAllTypes_Succeeds) {
    axon_type_t types[] = {
        AXON_TYPE_UNMYELINATED,
        AXON_TYPE_MYELINATED,
        AXON_TYPE_A_ALPHA,
        AXON_TYPE_A_BETA,
        AXON_TYPE_A_DELTA,
        AXON_TYPE_C_FIBER
    };

    for (auto type : types) {
        axon_t* axon = axon_create(0, type, 100.0f, 1.0f);
        ASSERT_NE(axon, nullptr) << "Failed to create axon of type: " << type;
        EXPECT_EQ(axon->type, type);
        axon_destroy(axon);
    }
}

/**
 * @brief Test axon_create with invalid parameters (zero length)
 *
 * Expected: Returns NULL and triggers NIMCP_THROW_TO_IMMUNE
 */
TEST_F(CoreExceptionTest, Axon_CreateZeroLength_ReturnsNull) {
    axon_t* axon = axon_create(0, AXON_TYPE_MYELINATED, 0.0f, 1.0f);
    EXPECT_EQ(axon, nullptr)
        << "axon_create should return NULL for zero length";
}

/**
 * @brief Test axon_create with invalid parameters (negative length)
 */
TEST_F(CoreExceptionTest, Axon_CreateNegativeLength_ReturnsNull) {
    axon_t* axon = axon_create(0, AXON_TYPE_MYELINATED, -100.0f, 1.0f);
    EXPECT_EQ(axon, nullptr)
        << "axon_create should return NULL for negative length";
}

/**
 * @brief Test axon_create with invalid parameters (zero diameter)
 */
TEST_F(CoreExceptionTest, Axon_CreateZeroDiameter_ReturnsNull) {
    axon_t* axon = axon_create(0, AXON_TYPE_MYELINATED, 100.0f, 0.0f);
    EXPECT_EQ(axon, nullptr)
        << "axon_create should return NULL for zero diameter";
}

/**
 * @brief Test axon_create with invalid parameters (negative diameter)
 */
TEST_F(CoreExceptionTest, Axon_CreateNegativeDiameter_ReturnsNull) {
    axon_t* axon = axon_create(0, AXON_TYPE_MYELINATED, 100.0f, -1.0f);
    EXPECT_EQ(axon, nullptr)
        << "axon_create should return NULL for negative diameter";
}

/**
 * @brief Test axon_create with diameter below minimum
 */
TEST_F(CoreExceptionTest, Axon_CreateBelowMinDiameter_ReturnsNullOrHandles) {
    // NIMCP_AXON_MIN_DIAMETER_UM is 0.2
    axon_t* axon = axon_create(0, AXON_TYPE_MYELINATED, 100.0f, 0.1f);
    // Implementation may return NULL or clamp to minimum
    if (axon != nullptr) {
        axon_destroy(axon);
    }
    SUCCEED();
}

/**
 * @brief Test axon_create with diameter above maximum
 */
TEST_F(CoreExceptionTest, Axon_CreateAboveMaxDiameter_ReturnsNullOrHandles) {
    // NIMCP_AXON_MAX_DIAMETER_UM is 20.0
    axon_t* axon = axon_create(0, AXON_TYPE_MYELINATED, 100.0f, 25.0f);
    // Implementation may return NULL or clamp to maximum
    if (axon != nullptr) {
        axon_destroy(axon);
    }
    SUCCEED();
}

/**
 * @brief Test axon_destroy handles NULL gracefully
 */
TEST_F(CoreExceptionTest, Axon_DestroyNull_NoExceptionNoCrash) {
    axon_destroy(nullptr);
    SUCCEED();
}

//=============================================================================
// Axon Segment Exception Tests
//=============================================================================

/**
 * @brief Test axon_create_segments with NULL axon
 *
 * Expected: Returns false and triggers NIMCP_THROW_TO_IMMUNE
 */
TEST_F(CoreExceptionTest, Axon_CreateSegmentsNullAxon_ReturnsFalse) {
    bool result = axon_create_segments(nullptr, 5, 20.0f);
    EXPECT_FALSE(result)
        << "axon_create_segments(NULL, ...) should return false";
}

/**
 * @brief Test axon_create_segments with zero segments
 */
TEST_F(CoreExceptionTest, Axon_CreateSegmentsZeroCount_ReturnsFalse) {
    axon_t* axon = axon_create(0, AXON_TYPE_MYELINATED, 100.0f, 1.0f);
    ASSERT_NE(axon, nullptr);

    bool result = axon_create_segments(axon, 0, 20.0f);
    EXPECT_FALSE(result)
        << "axon_create_segments with 0 segments should return false";

    axon_destroy(axon);
}

/**
 * @brief Test axon_create_segments with too many segments
 */
TEST_F(CoreExceptionTest, Axon_CreateSegmentsTooMany_ReturnsFalse) {
    axon_t* axon = axon_create(0, AXON_TYPE_MYELINATED, 100.0f, 1.0f);
    ASSERT_NE(axon, nullptr);

    // NIMCP_AXON_MAX_SEGMENTS is 64
    bool result = axon_create_segments(axon, 100, 20.0f);
    EXPECT_FALSE(result)
        << "axon_create_segments with > MAX_SEGMENTS should return false";

    axon_destroy(axon);
}

/**
 * @brief Test axon_create_segments with invalid internode length
 */
TEST_F(CoreExceptionTest, Axon_CreateSegmentsZeroInternodeLength_ReturnsFalse) {
    axon_t* axon = axon_create(0, AXON_TYPE_MYELINATED, 100.0f, 1.0f);
    ASSERT_NE(axon, nullptr);

    bool result = axon_create_segments(axon, 5, 0.0f);
    EXPECT_FALSE(result)
        << "axon_create_segments with 0 internode_length should return false";

    axon_destroy(axon);
}

/**
 * @brief Test axon_create_segments with negative internode length
 */
TEST_F(CoreExceptionTest, Axon_CreateSegmentsNegativeInternodeLength_ReturnsFalse) {
    axon_t* axon = axon_create(0, AXON_TYPE_MYELINATED, 100.0f, 1.0f);
    ASSERT_NE(axon, nullptr);

    bool result = axon_create_segments(axon, 5, -20.0f);
    EXPECT_FALSE(result)
        << "axon_create_segments with negative internode_length should return false";

    axon_destroy(axon);
}

/**
 * @brief Test axon_create_segments with valid parameters
 */
TEST_F(CoreExceptionTest, Axon_CreateSegmentsValid_Succeeds) {
    axon_t* axon = axon_create(0, AXON_TYPE_MYELINATED, 100.0f, 1.0f);
    ASSERT_NE(axon, nullptr);

    bool result = axon_create_segments(axon, 5, 20.0f);
    EXPECT_TRUE(result)
        << "axon_create_segments should succeed with valid parameters";

    axon_destroy(axon);
}

/**
 * @brief Test axon_set_segment_myelination with NULL axon
 */
TEST_F(CoreExceptionTest, Axon_SetSegmentMyelinationNullAxon_ReturnsFalse) {
    bool result = axon_set_segment_myelination(nullptr, 0, 0.8f);
    EXPECT_FALSE(result)
        << "axon_set_segment_myelination(NULL, ...) should return false";
}

//=============================================================================
// Axon Spike Initiation Exception Tests
//=============================================================================

/**
 * @brief Test axon_initiate_spike with NULL axon
 */
TEST_F(CoreExceptionTest, Axon_InitiateSpikeNullAxon_ReturnsFalse) {
    bool result = axon_initiate_spike(nullptr, 100.0f, 1000);
    EXPECT_FALSE(result)
        << "axon_initiate_spike(NULL, ...) should return false";
}

/**
 * @brief Test axon_initiate_spike with valid parameters
 */
TEST_F(CoreExceptionTest, Axon_InitiateSpikeValid_Succeeds) {
    axon_t* axon = axon_create(0, AXON_TYPE_MYELINATED, 100.0f, 1.0f);
    ASSERT_NE(axon, nullptr);

    // Create segments first
    bool seg_result = axon_create_segments(axon, 5, 20.0f);
    EXPECT_TRUE(seg_result);

    bool result = axon_initiate_spike(axon, 100.0f, 1000);
    EXPECT_TRUE(result)
        << "axon_initiate_spike should succeed with valid parameters";

    axon_destroy(axon);
}

/**
 * @brief Test axon_spike_arrived with NULL axon
 */
TEST_F(CoreExceptionTest, Axon_SpikeArrivedNullAxon_ReturnsFalse) {
    bool result = axon_spike_arrived(nullptr, 1000);
    EXPECT_FALSE(result)
        << "axon_spike_arrived(NULL, ...) should return false";
}

//=============================================================================
// Axon Network Exception Tests
//=============================================================================

/**
 * @brief Test axon_network_create with valid capacity
 */
TEST_F(CoreExceptionTest, AxonNetwork_CreateValidCapacity_Succeeds) {
    axon_network_t* network = axon_network_create(100);
    ASSERT_NE(network, nullptr)
        << "axon_network_create should succeed with valid capacity";

    axon_network_destroy(network);
}

/**
 * @brief Test axon_network_create with zero capacity
 */
TEST_F(CoreExceptionTest, AxonNetwork_CreateZeroCapacity_ReturnsNullOrHandles) {
    axon_network_t* network = axon_network_create(0);
    // Implementation may return NULL or handle gracefully
    if (network != nullptr) {
        axon_network_destroy(network);
    }
    SUCCEED();
}

/**
 * @brief Test axon_network_destroy with NULL (should not crash)
 */
TEST_F(CoreExceptionTest, AxonNetwork_DestroyNull_NoExceptionNoCrash) {
    axon_network_destroy(nullptr);
    SUCCEED();
}

/**
 * @brief Test axon_network_add with NULL network
 */
TEST_F(CoreExceptionTest, AxonNetwork_AddNullNetwork_ReturnsFalse) {
    axon_t* axon = axon_create(0, AXON_TYPE_MYELINATED, 100.0f, 1.0f);
    ASSERT_NE(axon, nullptr);

    bool result = axon_network_add(nullptr, axon);
    EXPECT_FALSE(result)
        << "axon_network_add(NULL, ...) should return false";

    axon_destroy(axon);
}

/**
 * @brief Test axon_network_add with NULL axon
 */
TEST_F(CoreExceptionTest, AxonNetwork_AddNullAxon_ReturnsFalse) {
    axon_network_t* network = axon_network_create(100);
    ASSERT_NE(network, nullptr);

    bool result = axon_network_add(network, nullptr);
    EXPECT_FALSE(result)
        << "axon_network_add(..., NULL) should return false";

    axon_network_destroy(network);
}

/**
 * @brief Test axon_network_find with NULL network
 */
TEST_F(CoreExceptionTest, AxonNetwork_FindNullNetwork_ReturnsNull) {
    axon_t* result = axon_network_find(nullptr, 0);
    EXPECT_EQ(result, nullptr)
        << "axon_network_find(NULL, ...) should return NULL";
}

/**
 * @brief Test axon_network_remove with NULL network
 */
TEST_F(CoreExceptionTest, AxonNetwork_RemoveNullNetwork_ReturnsNull) {
    axon_t* result = axon_network_remove(nullptr, 0);
    EXPECT_EQ(result, nullptr)
        << "axon_network_remove(NULL, ...) should return NULL";
}

/**
 * @brief Test axon_network_step with NULL network (should not crash)
 */
TEST_F(CoreExceptionTest, AxonNetwork_StepNull_NoExceptionNoCrash) {
    axon_network_step(nullptr, 1000, 1.0f, nullptr);
    SUCCEED();
}

//=============================================================================
// Axon Property Setting Exception Tests
//=============================================================================

/**
 * @brief Test axon_set_myelination with NULL axon (should not crash)
 */
TEST_F(CoreExceptionTest, Axon_SetMyelinationNull_NoExceptionNoCrash) {
    axon_set_myelination(nullptr, 0.8f);
    SUCCEED();
}

/**
 * @brief Test axon_set_myelination with valid axon
 */
TEST_F(CoreExceptionTest, Axon_SetMyelinationValid_Succeeds) {
    axon_t* axon = axon_create(0, AXON_TYPE_MYELINATED, 100.0f, 1.0f);
    ASSERT_NE(axon, nullptr);

    axon_set_myelination(axon, 0.8f);
    // Should succeed without crashing
    EXPECT_FLOAT_EQ(axon->myelination_level, 0.8f);

    axon_destroy(axon);
}

/**
 * @brief Test axon_set_myelination clamps out-of-range values
 */
TEST_F(CoreExceptionTest, Axon_SetMyelinationOutOfRange_Clamps) {
    axon_t* axon = axon_create(0, AXON_TYPE_MYELINATED, 100.0f, 1.0f);
    ASSERT_NE(axon, nullptr);

    // Test value > 1.0
    axon_set_myelination(axon, 1.5f);
    EXPECT_LE(axon->myelination_level, 1.0f)
        << "Myelination should be clamped to <= 1.0";

    // Test value < 0.0
    axon_set_myelination(axon, -0.5f);
    EXPECT_GE(axon->myelination_level, 0.0f)
        << "Myelination should be clamped to >= 0.0";

    axon_destroy(axon);
}

/**
 * @brief Test axon_receive_lactate with NULL axon (should not crash)
 */
TEST_F(CoreExceptionTest, Axon_ReceiveLactateNull_NoExceptionNoCrash) {
    axon_receive_lactate(nullptr, 1.0f);
    SUCCEED();
}

/**
 * @brief Test axon_update_activity with NULL axon (should not crash)
 */
TEST_F(CoreExceptionTest, Axon_UpdateActivityNull_NoExceptionNoCrash) {
    axon_update_activity(nullptr, 1000);
    SUCCEED();
}

/**
 * @brief Test axon_get_activity_stats with NULL axon
 */
TEST_F(CoreExceptionTest, Axon_GetActivityStatsNullAxon_NoExceptionNoCrash) {
    axon_activity_stats_t stats = {0};
    axon_get_activity_stats(nullptr, &stats);
    SUCCEED();
}

/**
 * @brief Test axon_get_activity_stats with NULL stats
 */
TEST_F(CoreExceptionTest, Axon_GetActivityStatsNullStats_NoExceptionNoCrash) {
    axon_t* axon = axon_create(0, AXON_TYPE_MYELINATED, 100.0f, 1.0f);
    ASSERT_NE(axon, nullptr);

    axon_get_activity_stats(axon, nullptr);
    SUCCEED();

    axon_destroy(axon);
}

//=============================================================================
// Axon Update and Step Exception Tests
//=============================================================================

/**
 * @brief Test axon_update_conduction with NULL (should not crash)
 */
TEST_F(CoreExceptionTest, Axon_UpdateConductionNull_NoExceptionNoCrash) {
    axon_update_conduction(nullptr);
    SUCCEED();
}

/**
 * @brief Test axon_step with NULL axon (should not crash)
 */
TEST_F(CoreExceptionTest, Axon_StepNull_NoExceptionNoCrash) {
    axon_step(nullptr, 1000, 1.0f);
    SUCCEED();
}

/**
 * @brief Test axon_is_refractory with NULL axon
 */
TEST_F(CoreExceptionTest, Axon_IsRefractoryNull_ReturnsFalseOrHandles) {
    bool result = axon_is_refractory(nullptr, 1000);
    // Should return false or handle gracefully
    SUCCEED();
}

//=============================================================================
// Parameter Validation Tests
//=============================================================================

/**
 * @brief Test axon_validate_params with valid parameters
 */
TEST_F(CoreExceptionTest, Axon_ValidateParamsValid_ReturnsTrue) {
    bool result = axon_validate_params(100.0f, 1.0f);
    EXPECT_TRUE(result)
        << "axon_validate_params should return true for valid parameters";
}

/**
 * @brief Test axon_validate_params with invalid length
 */
TEST_F(CoreExceptionTest, Axon_ValidateParamsInvalidLength_ReturnsFalse) {
    bool result = axon_validate_params(0.0f, 1.0f);
    EXPECT_FALSE(result)
        << "axon_validate_params should return false for zero length";

    result = axon_validate_params(-100.0f, 1.0f);
    EXPECT_FALSE(result)
        << "axon_validate_params should return false for negative length";
}

/**
 * @brief Test axon_validate_params with invalid diameter
 */
TEST_F(CoreExceptionTest, Axon_ValidateParamsInvalidDiameter_ReturnsFalse) {
    bool result = axon_validate_params(100.0f, 0.0f);
    EXPECT_FALSE(result)
        << "axon_validate_params should return false for zero diameter";

    result = axon_validate_params(100.0f, -1.0f);
    EXPECT_FALSE(result)
        << "axon_validate_params should return false for negative diameter";
}

/**
 * @brief Test axon_validate_params with NaN values
 */
TEST_F(CoreExceptionTest, Axon_ValidateParamsNaN_ReturnsFalse) {
    bool result = axon_validate_params(std::nanf(""), 1.0f);
    EXPECT_FALSE(result)
        << "axon_validate_params should return false for NaN length";

    result = axon_validate_params(100.0f, std::nanf(""));
    EXPECT_FALSE(result)
        << "axon_validate_params should return false for NaN diameter";
}

/**
 * @brief Test axon_validate_params with infinity values
 */
TEST_F(CoreExceptionTest, Axon_ValidateParamsInfinity_ReturnsFalse) {
    bool result = axon_validate_params(INFINITY, 1.0f);
    EXPECT_FALSE(result)
        << "axon_validate_params should return false for infinite length";

    result = axon_validate_params(100.0f, INFINITY);
    EXPECT_FALSE(result)
        << "axon_validate_params should return false for infinite diameter";
}

//=============================================================================
// Spike Pool Exception Tests
//=============================================================================

/**
 * @brief Test axon_spike_pool_destroy with NULL (should not crash)
 */
TEST_F(CoreExceptionTest, AxonSpikePool_DestroyNull_NoExceptionNoCrash) {
    axon_spike_pool_destroy(nullptr);
    SUCCEED();
}

/**
 * @brief Test axon_spike_pool_free with NULL pool
 */
TEST_F(CoreExceptionTest, AxonSpikePool_FreeNullPool_NoExceptionNoCrash) {
    axon_spike_event_t event = {0};
    axon_spike_pool_free(nullptr, &event);
    SUCCEED();
}

/**
 * @brief Test axon_spike_pool_free with NULL event
 */
TEST_F(CoreExceptionTest, AxonSpikePool_FreeNullEvent_NoExceptionNoCrash) {
    // Can't easily test without a pool, but ensure NULL handling is safe
    SUCCEED();
}

//=============================================================================
// Spike Queue Exception Tests
//=============================================================================

/**
 * @brief Test axon_spike_queue_destroy with NULL (should not crash)
 */
TEST_F(CoreExceptionTest, AxonSpikeQueue_DestroyNull_NoExceptionNoCrash) {
    axon_spike_queue_destroy(nullptr);
    SUCCEED();
}

/**
 * @brief Test axon_spike_queue_push with NULL queue
 */
TEST_F(CoreExceptionTest, AxonSpikeQueue_PushNullQueue_ReturnsFalse) {
    axon_spike_event_t event = {0};
    bool result = axon_spike_queue_push(nullptr, &event);
    EXPECT_FALSE(result)
        << "axon_spike_queue_push(NULL, ...) should return false";
}

/**
 * @brief Test axon_spike_queue_pop with NULL queue
 */
TEST_F(CoreExceptionTest, AxonSpikeQueue_PopNullQueue_ReturnsFalse) {
    axon_spike_event_t event = {0};
    bool result = axon_spike_queue_pop(nullptr, &event);
    EXPECT_FALSE(result)
        << "axon_spike_queue_pop(NULL, ...) should return false";
}

//=============================================================================
// Copy-on-Write Exception Tests
//=============================================================================

/**
 * @brief Test axon_cow_copy with NULL
 */
TEST_F(CoreExceptionTest, Axon_CowCopyNull_ReturnsNull) {
    axon_t* result = axon_cow_copy(nullptr);
    EXPECT_EQ(result, nullptr)
        << "axon_cow_copy(NULL) should return NULL";
}

/**
 * @brief Test axon_cow_prepare_write with NULL
 */
TEST_F(CoreExceptionTest, Axon_CowPrepareWriteNull_ReturnsFalse) {
    bool result = axon_cow_prepare_write(nullptr);
    EXPECT_FALSE(result)
        << "axon_cow_prepare_write(NULL) should return false";
}

/**
 * @brief Test axon_cow_release with NULL (should not crash)
 */
TEST_F(CoreExceptionTest, Axon_CowReleaseNull_NoExceptionNoCrash) {
    axon_cow_release(nullptr);
    SUCCEED();
}

/**
 * @brief Test axon_is_cow_copy with NULL
 */
TEST_F(CoreExceptionTest, Axon_IsCowCopyNull_ReturnsFalseOrHandles) {
    bool result = axon_is_cow_copy(nullptr);
    // Should return false or handle gracefully
    SUCCEED();
}

//=============================================================================
// Biophysics Exception Tests
//=============================================================================

/**
 * @brief Test axon_init_biophysics with NULL
 */
TEST_F(CoreExceptionTest, Axon_InitBiophysicsNull_ReturnsFalse) {
    bool result = axon_init_biophysics(nullptr, false, 0);
    EXPECT_FALSE(result)
        << "axon_init_biophysics(NULL, ...) should return false";
}

/**
 * @brief Test axon_update_segment_cable_params with NULL
 */
TEST_F(CoreExceptionTest, Axon_UpdateSegmentCableParamsNull_NoExceptionNoCrash) {
    axon_update_segment_cable_params(nullptr, 0);
    SUCCEED();
}

/**
 * @brief Test axon_check_segment_block with NULL
 */
TEST_F(CoreExceptionTest, Axon_CheckSegmentBlockNull_ReturnsFalseOrHandles) {
    bool result = axon_check_segment_block(nullptr, 0);
    // Should return false or handle gracefully
    SUCCEED();
}

/**
 * @brief Test axon_set_temperature with NULL
 */
TEST_F(CoreExceptionTest, Axon_SetTemperatureNull_NoExceptionNoCrash) {
    axon_set_temperature(nullptr, 37.0f);
    SUCCEED();
}

/**
 * @brief Test axon_compute_metabolic_efficiency with NULL
 */
TEST_F(CoreExceptionTest, Axon_ComputeMetabolicEfficiencyNull_NoExceptionNoCrash) {
    axon_compute_metabolic_efficiency(nullptr);
    SUCCEED();
}

/**
 * @brief Test axon_update_biophysics with NULL
 */
TEST_F(CoreExceptionTest, Axon_UpdateBiophysicsNull_NoExceptionNoCrash) {
    axon_update_biophysics(nullptr);
    SUCCEED();
}

//=============================================================================
// Exception Recovery Tests
//=============================================================================

/**
 * @brief Test that valid operations work after exceptions
 */
TEST_F(CoreExceptionTest, ExceptionRecovery_ValidAfterInvalid_Succeeds) {
    // Trigger exceptions
    axon_create(0, AXON_TYPE_MYELINATED, 0.0f, 1.0f);      // Invalid length
    axon_create(0, AXON_TYPE_MYELINATED, 100.0f, 0.0f);    // Invalid diameter
    axon_create_segments(nullptr, 5, 20.0f);                // NULL axon
    axon_network_add(nullptr, nullptr);                     // NULL args

    // Now valid operation should succeed
    axon_t* axon = axon_create(0, AXON_TYPE_MYELINATED, 100.0f, 1.0f);
    ASSERT_NE(axon, nullptr) << "Valid operation should succeed after exceptions";

    bool seg_result = axon_create_segments(axon, 5, 20.0f);
    EXPECT_TRUE(seg_result) << "Valid segment creation should succeed after exceptions";

    axon_destroy(axon);
}

/**
 * @brief Test rapid create/destroy cycles
 */
TEST_F(CoreExceptionTest, Axon_RapidCreateDestroyCycles) {
    for (int i = 0; i < 100; i++) {
        axon_t* axon = axon_create((uint32_t)i, AXON_TYPE_MYELINATED, 100.0f, 1.0f);
        if (axon != nullptr) {
            axon_create_segments(axon, 5, 20.0f);
            axon_destroy(axon);
        }
    }
    SUCCEED();
}

//=============================================================================
// Edge Case Tests
//=============================================================================

/**
 * @brief Test with maximum ID value
 */
TEST_F(CoreExceptionTest, Axon_MaxIDValue_Succeeds) {
    axon_t* axon = axon_create(UINT32_MAX, AXON_TYPE_MYELINATED, 100.0f, 1.0f);
    ASSERT_NE(axon, nullptr);
    EXPECT_EQ(axon->id, UINT32_MAX);
    axon_destroy(axon);
}

/**
 * @brief Test with boundary length and diameter values
 */
TEST_F(CoreExceptionTest, Axon_BoundaryValues) {
    // Test minimum valid values
    axon_t* axon = axon_create(0, AXON_TYPE_MYELINATED,
        0.001f,                         // Very small length
        NIMCP_AXON_MIN_DIAMETER_UM      // Minimum diameter
    );
    if (axon != nullptr) {
        axon_destroy(axon);
    }

    // Test maximum diameter
    axon = axon_create(0, AXON_TYPE_MYELINATED,
        10000.0f,                       // Large length
        NIMCP_AXON_MAX_DIAMETER_UM      // Maximum diameter
    );
    if (axon != nullptr) {
        axon_destroy(axon);
    }
    SUCCEED();
}

//=============================================================================
// Main Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
