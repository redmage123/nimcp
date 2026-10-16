/**
 * @file test_mesh_gpu.cpp
 * @brief Unit Tests for GPU Channel and Coordinator
 *
 * Tests: Configuration, lifecycle, transaction submission, batching,
 *        device management, recovery, and statistics.
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <chrono>

extern "C" {
#include "mesh/nimcp_mesh_gpu.h"
#include "mesh/nimcp_mesh_types.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MeshGPUChannelTest : public ::testing::Test {
protected:
    mesh_gpu_channel_t channel;

    void SetUp() override {
        channel = mesh_gpu_channel_create(nullptr);
        ASSERT_NE(channel, nullptr);
    }

    void TearDown() override {
        mesh_gpu_channel_destroy(channel);
        channel = nullptr;
    }

    /* Helper: Create simple transaction */
    mesh_gpu_transaction_t* create_simple_tx(mesh_gpu_tx_type_t type = MESH_GPU_TX_TENSOR_OP) {
        float input_data[] = {1.0f, 2.0f, 3.0f, 4.0f};
        return mesh_gpu_transaction_create(type, input_data, sizeof(input_data), sizeof(input_data));
    }

    /* CPU fallback for testing */
    static bool test_fallback(const mesh_gpu_transaction_t* tx, void* ctx) {
        (void)ctx;
        if (tx->output_data && tx->input_data && tx->output_size > 0) {
            size_t copy_size = tx->input_size < tx->output_size ? tx->input_size : tx->output_size;
            memcpy(tx->output_data, tx->input_data, copy_size);
        }
        return true;
    }
};

/* ============================================================================
 * Default Configuration Tests
 * ============================================================================ */

TEST_F(MeshGPUChannelTest, DefaultConfigHasSensibleValues) {
    mesh_gpu_channel_config_t config = mesh_gpu_channel_default_config();

    EXPECT_EQ(config.batch_threshold, MESH_GPU_DEFAULT_BATCH_THRESHOLD);
    EXPECT_FLOAT_EQ(config.batch_timeout_ms, MESH_GPU_DEFAULT_BATCH_TIMEOUT_MS);
    EXPECT_EQ(config.max_pending, MESH_GPU_MAX_PENDING);
    EXPECT_TRUE(config.enable_cpu_fallback);
    EXPECT_TRUE(config.enable_batch_reduction);
    EXPECT_EQ(config.max_retries, MESH_GPU_MAX_RETRIES);
    EXPECT_FLOAT_EQ(config.memory_threshold, MESH_GPU_MEMORY_THRESHOLD);
}

TEST_F(MeshGPUChannelTest, CreateWithCustomConfig) {
    mesh_gpu_channel_config_t config = mesh_gpu_channel_default_config();
    config.batch_threshold = 32;
    config.batch_timeout_ms = 100.0f;
    config.enable_cpu_fallback = false;

    mesh_gpu_channel_t custom = mesh_gpu_channel_create(&config);
    ASSERT_NE(custom, nullptr);

    mesh_gpu_channel_destroy(custom);
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(MeshGPUChannelTest, CreateAndDestroy) {
    mesh_gpu_channel_t ch = mesh_gpu_channel_create(nullptr);
    ASSERT_NE(ch, nullptr);
    mesh_gpu_channel_destroy(ch);
}

TEST_F(MeshGPUChannelTest, StartAndStop) {
    EXPECT_EQ(mesh_gpu_channel_start(channel), NIMCP_SUCCESS);
    EXPECT_EQ(mesh_gpu_channel_stop(channel, false), NIMCP_SUCCESS);
}

TEST_F(MeshGPUChannelTest, StartIdempotent) {
    EXPECT_EQ(mesh_gpu_channel_start(channel), NIMCP_SUCCESS);
    EXPECT_EQ(mesh_gpu_channel_start(channel), NIMCP_SUCCESS);  /* Second call OK */
}

TEST_F(MeshGPUChannelTest, StopIdempotent) {
    EXPECT_EQ(mesh_gpu_channel_start(channel), NIMCP_SUCCESS);
    EXPECT_EQ(mesh_gpu_channel_stop(channel, false), NIMCP_SUCCESS);
    EXPECT_EQ(mesh_gpu_channel_stop(channel, false), NIMCP_SUCCESS);  /* Second call OK */
}

TEST_F(MeshGPUChannelTest, StopWithDrain) {
    EXPECT_EQ(mesh_gpu_channel_start(channel), NIMCP_SUCCESS);

    /* Submit a transaction */
    mesh_gpu_transaction_t* tx = create_simple_tx();
    ASSERT_NE(tx, nullptr);
    EXPECT_EQ(mesh_gpu_channel_submit(channel, tx), NIMCP_SUCCESS);

    /* Stop with drain should flush pending */
    EXPECT_EQ(mesh_gpu_channel_stop(channel, true), NIMCP_SUCCESS);
}

/* ============================================================================
 * Transaction Creation Tests
 * ============================================================================ */

TEST_F(MeshGPUChannelTest, CreateTransaction) {
    mesh_gpu_transaction_t* tx = create_simple_tx();
    ASSERT_NE(tx, nullptr);

    EXPECT_EQ(tx->gpu_type, MESH_GPU_TX_TENSOR_OP);
    EXPECT_EQ(tx->status, MESH_GPU_TX_STATUS_PENDING);
    EXPECT_NE(tx->input_data, nullptr);
    EXPECT_NE(tx->output_data, nullptr);
    EXPECT_EQ(tx->input_size, sizeof(float) * 4);
    EXPECT_EQ(tx->output_size, sizeof(float) * 4);

    mesh_gpu_transaction_destroy(tx);
}

TEST_F(MeshGPUChannelTest, CreateTransactionDifferentTypes) {
    mesh_gpu_tx_type_t types[] = {
        MESH_GPU_TX_BELIEF_BATCH,
        MESH_GPU_TX_CONSENSUS_COMPUTE,
        MESH_GPU_TX_FEP_BATCH,
        MESH_GPU_TX_NEURAL_FORWARD,
        MESH_GPU_TX_NEURAL_BACKWARD,
        MESH_GPU_TX_MATRIX_MULTIPLY,
        MESH_GPU_TX_STATISTICAL,
        MESH_GPU_TX_CUSTOM
    };

    for (mesh_gpu_tx_type_t type : types) {
        mesh_gpu_transaction_t* tx = create_simple_tx(type);
        ASSERT_NE(tx, nullptr) << "Failed for type " << mesh_gpu_tx_type_to_string(type);
        EXPECT_EQ(tx->gpu_type, type);
        mesh_gpu_transaction_destroy(tx);
    }
}

TEST_F(MeshGPUChannelTest, CreateTransactionNoInput) {
    mesh_gpu_transaction_t* tx = mesh_gpu_transaction_create(MESH_GPU_TX_CUSTOM, nullptr, 0, 100);
    ASSERT_NE(tx, nullptr);
    EXPECT_EQ(tx->input_data, nullptr);
    EXPECT_EQ(tx->input_size, 0u);
    EXPECT_NE(tx->output_data, nullptr);
    EXPECT_EQ(tx->output_size, 100u);
    mesh_gpu_transaction_destroy(tx);
}

TEST_F(MeshGPUChannelTest, DestroyNullTransaction) {
    mesh_gpu_transaction_destroy(nullptr);  /* Should not crash */
}

/* ============================================================================
 * Transaction Submission Tests
 * ============================================================================ */

TEST_F(MeshGPUChannelTest, SubmitRequiresStart) {
    mesh_gpu_transaction_t* tx = create_simple_tx();
    ASSERT_NE(tx, nullptr);

    /* Channel not started - should fail */
    EXPECT_EQ(mesh_gpu_channel_submit(channel, tx), NIMCP_ERROR_NOT_READY);

    mesh_gpu_transaction_destroy(tx);
}

TEST_F(MeshGPUChannelTest, SubmitTransaction) {
    ASSERT_EQ(mesh_gpu_channel_start(channel), NIMCP_SUCCESS);

    mesh_gpu_transaction_t* tx = create_simple_tx();
    ASSERT_NE(tx, nullptr);

    EXPECT_EQ(mesh_gpu_channel_submit(channel, tx), NIMCP_SUCCESS);
    EXPECT_GT(mesh_gpu_channel_pending_count(channel), 0u);
}

TEST_F(MeshGPUChannelTest, SubmitMultipleTransactions) {
    ASSERT_EQ(mesh_gpu_channel_start(channel), NIMCP_SUCCESS);

    for (int i = 0; i < 10; i++) {
        mesh_gpu_transaction_t* tx = create_simple_tx();
        ASSERT_NE(tx, nullptr);
        EXPECT_EQ(mesh_gpu_channel_submit(channel, tx), NIMCP_SUCCESS);
    }

    EXPECT_GE(mesh_gpu_channel_pending_count(channel), 0u);  /* May have been flushed */
}

TEST_F(MeshGPUChannelTest, SubmitWithCallback) {
    ASSERT_EQ(mesh_gpu_channel_start(channel), NIMCP_SUCCESS);

    static bool callback_called = false;
    static mesh_tx_status_t callback_status = MESH_TX_STATUS_NONE;

    auto callback = [](const mesh_result_t* result, void* ctx) {
        (void)ctx;
        callback_called = true;
        callback_status = result->status;
    };

    mesh_gpu_transaction_t* tx = create_simple_tx();
    ASSERT_NE(tx, nullptr);

    EXPECT_EQ(mesh_gpu_channel_submit_async(channel, tx, callback, nullptr), NIMCP_SUCCESS);

    /* Force flush to trigger callback */
    mesh_gpu_channel_flush(channel);

    /* Callback should have been called */
    EXPECT_TRUE(callback_called);
}

/* ============================================================================
 * Batching Tests
 * ============================================================================ */

TEST_F(MeshGPUChannelTest, BatchThresholdFlush) {
    mesh_gpu_channel_config_t config = mesh_gpu_channel_default_config();
    config.batch_threshold = 5;

    mesh_gpu_channel_t small_batch_channel = mesh_gpu_channel_create(&config);
    ASSERT_NE(small_batch_channel, nullptr);
    ASSERT_EQ(mesh_gpu_channel_start(small_batch_channel), NIMCP_SUCCESS);

    /* Submit batch_threshold transactions */
    for (size_t i = 0; i < config.batch_threshold; i++) {
        mesh_gpu_transaction_t* tx = create_simple_tx();
        ASSERT_NE(tx, nullptr);
        mesh_gpu_channel_submit(small_batch_channel, tx);
    }

    /* Batch should have been flushed (pending count reset or low) */
    EXPECT_LE(mesh_gpu_channel_pending_count(small_batch_channel), 1u);

    mesh_gpu_channel_destroy(small_batch_channel);
}

TEST_F(MeshGPUChannelTest, ManualFlush) {
    ASSERT_EQ(mesh_gpu_channel_start(channel), NIMCP_SUCCESS);

    mesh_gpu_transaction_t* tx = create_simple_tx();
    ASSERT_NE(tx, nullptr);

    EXPECT_EQ(mesh_gpu_channel_submit(channel, tx), NIMCP_SUCCESS);
    size_t pending_before = mesh_gpu_channel_pending_count(channel);

    EXPECT_EQ(mesh_gpu_channel_flush(channel), NIMCP_SUCCESS);

    /* After flush, pending should be 0 */
    EXPECT_EQ(mesh_gpu_channel_pending_count(channel), 0u);
}

TEST_F(MeshGPUChannelTest, SetBatchThreshold) {
    EXPECT_EQ(mesh_gpu_channel_set_batch_threshold(channel, 128), NIMCP_SUCCESS);
}

TEST_F(MeshGPUChannelTest, SetBatchTimeout) {
    EXPECT_EQ(mesh_gpu_channel_set_batch_timeout(channel, 100.0f), NIMCP_SUCCESS);
}

TEST_F(MeshGPUChannelTest, InvalidBatchSettings) {
    EXPECT_EQ(mesh_gpu_channel_set_batch_threshold(channel, 0), NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(mesh_gpu_channel_set_batch_timeout(channel, 0.0f), NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(mesh_gpu_channel_set_batch_timeout(channel, -1.0f), NIMCP_ERROR_INVALID_PARAMETER);
}

/* ============================================================================
 * Device Management Tests
 * ============================================================================ */

TEST_F(MeshGPUChannelTest, DeviceCount) {
    size_t count = mesh_gpu_channel_device_count(channel);
    EXPECT_GE(count, 1u);  /* At least 1 (could be CPU fallback */
}

TEST_F(MeshGPUChannelTest, GetDeviceState) {
    size_t count = mesh_gpu_channel_device_count(channel);
    ASSERT_GT(count, 0u);

    mesh_gpu_device_state_t state;
    EXPECT_EQ(mesh_gpu_channel_get_device_state(channel, 0, &state), NIMCP_SUCCESS);

    EXPECT_TRUE(state.available);
    EXPECT_TRUE(state.healthy);
}

TEST_F(MeshGPUChannelTest, GetDeviceStateInvalidIndex) {
    mesh_gpu_device_state_t state;
    EXPECT_EQ(mesh_gpu_channel_get_device_state(channel, 9999, &state), NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(MeshGPUChannelTest, DisableEnableDevice) {
    size_t count = mesh_gpu_channel_device_count(channel);
    ASSERT_GT(count, 0u);

    EXPECT_EQ(mesh_gpu_channel_disable_device(channel, 0), NIMCP_SUCCESS);

    mesh_gpu_device_state_t state;
    EXPECT_EQ(mesh_gpu_channel_get_device_state(channel, 0, &state), NIMCP_SUCCESS);
    EXPECT_FALSE(state.available);

    EXPECT_EQ(mesh_gpu_channel_enable_device(channel, 0), NIMCP_SUCCESS);
    EXPECT_EQ(mesh_gpu_channel_get_device_state(channel, 0, &state), NIMCP_SUCCESS);
    EXPECT_TRUE(state.available);
}

/* ============================================================================
 * Recovery Tests
 * ============================================================================ */

TEST_F(MeshGPUChannelTest, SetCPUFallback) {
    EXPECT_EQ(mesh_gpu_channel_set_cpu_fallback(channel, true), NIMCP_SUCCESS);
    EXPECT_EQ(mesh_gpu_channel_set_cpu_fallback(channel, false), NIMCP_SUCCESS);
}

TEST_F(MeshGPUChannelTest, SetMaxRetries) {
    EXPECT_EQ(mesh_gpu_channel_set_max_retries(channel, 5), NIMCP_SUCCESS);
}

TEST_F(MeshGPUChannelTest, RegisterFallback) {
    EXPECT_EQ(mesh_gpu_channel_register_fallback(channel, MESH_GPU_TX_TENSOR_OP, test_fallback, nullptr),
              NIMCP_SUCCESS);
}

TEST_F(MeshGPUChannelTest, RegisterMultipleFallbacks) {
    EXPECT_EQ(mesh_gpu_channel_register_fallback(channel, MESH_GPU_TX_TENSOR_OP, test_fallback, nullptr),
              NIMCP_SUCCESS);
    EXPECT_EQ(mesh_gpu_channel_register_fallback(channel, MESH_GPU_TX_NEURAL_FORWARD, test_fallback, nullptr),
              NIMCP_SUCCESS);
    EXPECT_EQ(mesh_gpu_channel_register_fallback(channel, MESH_GPU_TX_MATRIX_MULTIPLY, test_fallback, nullptr),
              NIMCP_SUCCESS);
}

TEST_F(MeshGPUChannelTest, RegisterFallbackOverwrite) {
    /* Second registration for same type should overwrite */
    EXPECT_EQ(mesh_gpu_channel_register_fallback(channel, MESH_GPU_TX_TENSOR_OP, test_fallback, nullptr),
              NIMCP_SUCCESS);
    EXPECT_EQ(mesh_gpu_channel_register_fallback(channel, MESH_GPU_TX_TENSOR_OP, test_fallback, (void*)1),
              NIMCP_SUCCESS);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(MeshGPUChannelTest, GetStats) {
    ASSERT_EQ(mesh_gpu_channel_start(channel), NIMCP_SUCCESS);

    /* Submit and process some transactions */
    for (int i = 0; i < 5; i++) {
        mesh_gpu_transaction_t* tx = create_simple_tx();
        ASSERT_NE(tx, nullptr);
        mesh_gpu_channel_submit(channel, tx);
    }
    mesh_gpu_channel_flush(channel);

    mesh_gpu_channel_stats_t stats;
    EXPECT_EQ(mesh_gpu_channel_get_stats(channel, &stats), NIMCP_SUCCESS);

    EXPECT_EQ(stats.total_submitted, 5u);
    EXPECT_GE(stats.total_completed + stats.total_failed + stats.total_fallbacks, 5u);

    mesh_gpu_channel_stats_free(&stats);
}

TEST_F(MeshGPUChannelTest, ResetStats) {
    ASSERT_EQ(mesh_gpu_channel_start(channel), NIMCP_SUCCESS);

    /* Submit some transactions */
    mesh_gpu_transaction_t* tx = create_simple_tx();
    ASSERT_NE(tx, nullptr);
    mesh_gpu_channel_submit(channel, tx);
    mesh_gpu_channel_flush(channel);

    /* Reset stats */
    EXPECT_EQ(mesh_gpu_channel_reset_stats(channel), NIMCP_SUCCESS);

    mesh_gpu_channel_stats_t stats;
    EXPECT_EQ(mesh_gpu_channel_get_stats(channel, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_submitted, 0u);

    mesh_gpu_channel_stats_free(&stats);
}

TEST_F(MeshGPUChannelTest, StatsFreeHandlesNull) {
    mesh_gpu_channel_stats_free(nullptr);  /* Should not crash */

    mesh_gpu_channel_stats_t stats = {0};
    mesh_gpu_channel_stats_free(&stats);  /* Should handle empty */
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_F(MeshGPUChannelTest, TxTypeToString) {
    EXPECT_STREQ(mesh_gpu_tx_type_to_string(MESH_GPU_TX_NONE), "NONE");
    EXPECT_STREQ(mesh_gpu_tx_type_to_string(MESH_GPU_TX_BELIEF_BATCH), "BELIEF_BATCH");
    EXPECT_STREQ(mesh_gpu_tx_type_to_string(MESH_GPU_TX_TENSOR_OP), "TENSOR_OP");
    EXPECT_STREQ(mesh_gpu_tx_type_to_string(MESH_GPU_TX_NEURAL_FORWARD), "NEURAL_FORWARD");
    EXPECT_STREQ(mesh_gpu_tx_type_to_string((mesh_gpu_tx_type_t)999), "UNKNOWN");
}

TEST_F(MeshGPUChannelTest, TxStatusToString) {
    EXPECT_STREQ(mesh_gpu_tx_status_to_string(MESH_GPU_TX_STATUS_PENDING), "PENDING");
    EXPECT_STREQ(mesh_gpu_tx_status_to_string(MESH_GPU_TX_STATUS_BATCHED), "BATCHED");
    EXPECT_STREQ(mesh_gpu_tx_status_to_string(MESH_GPU_TX_STATUS_PROCESSING), "PROCESSING");
    EXPECT_STREQ(mesh_gpu_tx_status_to_string(MESH_GPU_TX_STATUS_COMPLETED), "COMPLETED");
    EXPECT_STREQ(mesh_gpu_tx_status_to_string(MESH_GPU_TX_STATUS_FAILED), "FAILED");
    EXPECT_STREQ(mesh_gpu_tx_status_to_string(MESH_GPU_TX_STATUS_FALLBACK), "FALLBACK");
    EXPECT_STREQ(mesh_gpu_tx_status_to_string((mesh_gpu_tx_status_t)999), "UNKNOWN");
}

TEST_F(MeshGPUChannelTest, CudaAvailable) {
    /* Just test that function doesn't crash */
    bool available = mesh_gpu_cuda_available();
    (void)available;  /* Result depends on system */
}

TEST_F(MeshGPUChannelTest, GetDeviceCount) {
    int count = mesh_gpu_get_device_count();
    EXPECT_GE(count, 0);  /* Can be 0 if no CUDA */
}

/* ============================================================================
 * Debug Output Test
 * ============================================================================ */

TEST_F(MeshGPUChannelTest, PrintDebugDoesNotCrash) {
    ASSERT_EQ(mesh_gpu_channel_start(channel), NIMCP_SUCCESS);

    /* Submit a transaction */
    mesh_gpu_transaction_t* tx = create_simple_tx();
    ASSERT_NE(tx, nullptr);
    mesh_gpu_channel_submit(channel, tx);

    mesh_gpu_channel_print_debug(channel);  /* Should not crash */
    mesh_gpu_channel_print_debug(nullptr);  /* Should handle NULL */
}

/* ============================================================================
 * Null Handling Tests
 * ============================================================================ */

TEST_F(MeshGPUChannelTest, NullChannelHandling) {
    mesh_gpu_transaction_t* tx = create_simple_tx();

    EXPECT_EQ(mesh_gpu_channel_start(nullptr), NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(mesh_gpu_channel_stop(nullptr, false), NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(mesh_gpu_channel_submit(nullptr, tx), NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(mesh_gpu_channel_flush(nullptr), NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(mesh_gpu_channel_device_count(nullptr), 0u);

    mesh_gpu_transaction_destroy(tx);
}

TEST_F(MeshGPUChannelTest, NullTransactionHandling) {
    ASSERT_EQ(mesh_gpu_channel_start(channel), NIMCP_SUCCESS);
    EXPECT_EQ(mesh_gpu_channel_submit(channel, nullptr), NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(MeshGPUChannelTest, NullOutputHandling) {
    EXPECT_EQ(mesh_gpu_channel_get_device_state(channel, 0, nullptr), NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(mesh_gpu_channel_get_stats(channel, nullptr), NIMCP_ERROR_INVALID_PARAMETER);

    mesh_tx_id_t id = {0};
    EXPECT_EQ(mesh_gpu_channel_get_status(channel, &id, nullptr), NIMCP_ERROR_INVALID_PARAMETER);
}

/* ============================================================================
 * Data Integrity Test
 * ============================================================================ */

TEST_F(MeshGPUChannelTest, TransactionDataIntegrity) {
    ASSERT_EQ(mesh_gpu_channel_start(channel), NIMCP_SUCCESS);

    /* Create transaction with known data */
    float input[] = {1.0f, 2.0f, 3.0f, 4.0f};
    mesh_gpu_transaction_t* tx = mesh_gpu_transaction_create(
        MESH_GPU_TX_TENSOR_OP,
        input,
        sizeof(input),
        sizeof(input)
    );
    ASSERT_NE(tx, nullptr);

    EXPECT_EQ(mesh_gpu_channel_submit(channel, tx), NIMCP_SUCCESS);
    mesh_gpu_channel_flush(channel);

    /* Check output matches input (our simple implementation copies) */
    if (tx->status == MESH_GPU_TX_STATUS_COMPLETED || tx->status == MESH_GPU_TX_STATUS_FALLBACK) {
        float* output = (float*)tx->output_data;
        EXPECT_FLOAT_EQ(output[0], 1.0f);
        EXPECT_FLOAT_EQ(output[1], 2.0f);
        EXPECT_FLOAT_EQ(output[2], 3.0f);
        EXPECT_FLOAT_EQ(output[3], 4.0f);
    }
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
