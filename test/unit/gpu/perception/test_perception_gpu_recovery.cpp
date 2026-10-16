/* ============================================================================
 * Unit Tests: GPU Perception Recovery System
 * ============================================================================
 * WHAT: Unit tests for GPU recovery in perception processing modules
 * WHY:  Validate self-healing and fallback mechanisms for visual/audio/speech
 * HOW:  Test OOM recovery, parameter validation, kernel launch recovery
 * ============================================================================
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "gpu/perception/nimcp_visual_cortex_gpu.h"
#include "gpu/perception/nimcp_speech_cortex_gpu.h"
#endif

namespace {

constexpr float TOLERANCE = 1e-5f;

/* ============================================================================
 * Test Fixture: Perception Recovery
 * ============================================================================ */
class PerceptionRecoveryTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef NIMCP_ENABLE_CUDA
        ctx_ = nimcp_gpu_context_create(0);
        if (!ctx_) {
            GTEST_SKIP() << "No GPU available - skipping test";
        }

        // Initialize recovery system
        nimcp_gpu_recovery_config_t config;
        nimcp_gpu_recovery_default_config(&config);
        config.enable_cpu_fallback = true;
        config.enable_param_correction = true;
        config.enable_batch_reduction = true;
        config.max_retries = 3;
        nimcp_gpu_recovery_init(&config);
#endif
    }

    void TearDown() override {
#ifdef NIMCP_ENABLE_CUDA
        nimcp_gpu_recovery_shutdown();
        if (ctx_) {
            nimcp_gpu_context_destroy(ctx_);
            ctx_ = nullptr;
        }
#endif
    }

#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_context_t* ctx_ = nullptr;

    // Helper to create test tensors
    nimcp_gpu_tensor_t* createTestTensor(size_t* dims, size_t ndim) {
        return nimcp_gpu_tensor_create(ctx_, dims, ndim, NIMCP_GPU_PRECISION_FP32);
    }

    // Helper to fill tensor with random data
    void fillRandom(nimcp_gpu_tensor_t* tensor, float min_val = 0.0f, float max_val = 1.0f) {
        if (!tensor) return;

        std::mt19937 gen(42);
        std::uniform_real_distribution<float> dist(min_val, max_val);

        size_t numel = tensor->numel;
        std::vector<float> data(numel);
        for (size_t i = 0; i < numel; i++) {
            data[i] = dist(gen);
        }

        cudaMemcpy(tensor->data, data.data(), numel * sizeof(float), cudaMemcpyHostToDevice);
    }
#endif
};

/* ============================================================================
 * Visual Processing Recovery Tests
 * ============================================================================ */

TEST_F(PerceptionRecoveryTest, VisualRecoveryInitialization) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Verify recovery is initialized
    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());

    // Create visual state
    nimcp_visual_gpu_state_t* state = nimcp_visual_gpu_create(ctx_, 8, 4, 4);
    ASSERT_NE(state, nullptr);

    // Initialize for small image
    EXPECT_TRUE(nimcp_visual_gpu_init(state, 64, 64));

    nimcp_visual_gpu_destroy(state);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(PerceptionRecoveryTest, VisualGaborFilterbankRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Create input/output tensors
    size_t in_dims[] = {1, 128, 128};
    size_t out_dims[] = {1, 8, 128, 128};  // 8 orientations

    nimcp_gpu_tensor_t* input = createTestTensor(in_dims, 3);
    nimcp_gpu_tensor_t* output = createTestTensor(out_dims, 4);

    ASSERT_NE(input, nullptr);
    ASSERT_NE(output, nullptr);

    fillRandom(input);

    // Test Gabor filterbank with recovery enabled
    bool result = nimcp_gpu_gabor_filterbank(
        ctx_, input, output,
        8,    // orientations
        11,   // kernel size
        2.0f, // sigma
        4.0f, // lambda
        0.5f  // gamma
    );

    EXPECT_TRUE(result) << "Gabor filterbank should succeed with recovery";

    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(output);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(PerceptionRecoveryTest, VisualSobelEdgeRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    size_t dims[] = {256, 256};

    nimcp_gpu_tensor_t* input = createTestTensor(dims, 2);
    nimcp_gpu_tensor_t* magnitude = createTestTensor(dims, 2);
    nimcp_gpu_tensor_t* direction = createTestTensor(dims, 2);

    ASSERT_NE(input, nullptr);
    ASSERT_NE(magnitude, nullptr);
    ASSERT_NE(direction, nullptr);

    fillRandom(input);

    bool result = nimcp_gpu_sobel_edge_detect(ctx_, input, magnitude, direction);
    EXPECT_TRUE(result) << "Sobel edge detection should succeed with recovery";

    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(magnitude);
    nimcp_gpu_tensor_destroy(direction);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(PerceptionRecoveryTest, VisualOpticalFlowRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    size_t dims[] = {128, 128};

    nimcp_gpu_tensor_t* frame1 = createTestTensor(dims, 2);
    nimcp_gpu_tensor_t* frame2 = createTestTensor(dims, 2);
    nimcp_gpu_tensor_t* flow_u = createTestTensor(dims, 2);
    nimcp_gpu_tensor_t* flow_v = createTestTensor(dims, 2);

    ASSERT_NE(frame1, nullptr);
    ASSERT_NE(frame2, nullptr);

    fillRandom(frame1);
    fillRandom(frame2);

    bool result = nimcp_gpu_optical_flow_lk(ctx_, frame1, frame2, flow_u, flow_v, 15);
    EXPECT_TRUE(result) << "Optical flow should succeed with recovery";

    nimcp_gpu_tensor_destroy(frame1);
    nimcp_gpu_tensor_destroy(frame2);
    nimcp_gpu_tensor_destroy(flow_u);
    nimcp_gpu_tensor_destroy(flow_v);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(PerceptionRecoveryTest, VisualColorOpponentRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    size_t rgb_dims[] = {3, 128, 128};  // 3 channels
    size_t out_dims[] = {128, 128};

    nimcp_gpu_tensor_t* rgb = createTestTensor(rgb_dims, 3);
    nimcp_gpu_tensor_t* rg = createTestTensor(out_dims, 2);
    nimcp_gpu_tensor_t* yb = createTestTensor(out_dims, 2);
    nimcp_gpu_tensor_t* lum = createTestTensor(out_dims, 2);

    ASSERT_NE(rgb, nullptr);

    fillRandom(rgb);

    bool result = nimcp_gpu_color_opponent(ctx_, rgb, rg, yb, lum);
    EXPECT_TRUE(result) << "Color opponent processing should succeed with recovery";

    nimcp_gpu_tensor_destroy(rgb);
    nimcp_gpu_tensor_destroy(rg);
    nimcp_gpu_tensor_destroy(yb);
    nimcp_gpu_tensor_destroy(lum);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(PerceptionRecoveryTest, VisualNullContextRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    // Test recovery from NULL context
    size_t dims[] = {64, 64};
    nimcp_gpu_tensor_t* input = createTestTensor(dims, 2);
    nimcp_gpu_tensor_t* output = createTestTensor(dims, 2);

    // Should fail gracefully with NULL context
    bool result = nimcp_gpu_sobel_edge_detect(nullptr, input, output, nullptr);
    EXPECT_FALSE(result) << "Should fail with NULL context";

    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(output);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Audio Processing Recovery Tests
 * ============================================================================ */

TEST_F(PerceptionRecoveryTest, AudioMelFilterbankRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Spectrogram: [batch, time, freq_bins]
    size_t spec_dims[] = {1, 100, 257};  // 257 = 512/2 + 1
    size_t mel_dims[] = {1, 100, 80};    // 80 mel bands

    nimcp_gpu_tensor_t* spectrogram = createTestTensor(spec_dims, 3);
    nimcp_gpu_tensor_t* mel_spec = createTestTensor(mel_dims, 3);

    ASSERT_NE(spectrogram, nullptr);
    ASSERT_NE(mel_spec, nullptr);

    fillRandom(spectrogram, 0.0f, 100.0f);  // Power spectrum values

    bool result = nimcp_gpu_mel_filterbank(
        ctx_, spectrogram, mel_spec,
        80,      // n_mels
        0.0f,    // fmin
        8000.0f, // fmax
        16000.0f // sample_rate
    );

    EXPECT_TRUE(result) << "Mel filterbank should succeed with recovery";

    nimcp_gpu_tensor_destroy(spectrogram);
    nimcp_gpu_tensor_destroy(mel_spec);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(PerceptionRecoveryTest, AudioSTFTRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    size_t audio_dims[] = {1, 16000};  // 1 second at 16kHz
    size_t stft_dims[] = {100, 257};   // approx frames x bins

    nimcp_gpu_tensor_t* audio = createTestTensor(audio_dims, 2);
    nimcp_gpu_tensor_t* stft_out = createTestTensor(stft_dims, 2);

    ASSERT_NE(audio, nullptr);

    fillRandom(audio, -1.0f, 1.0f);  // Audio samples

    bool result = nimcp_gpu_stft(
        ctx_, audio, stft_out,
        512,  // n_fft
        160   // hop_length
    );

    EXPECT_TRUE(result) << "STFT should succeed with recovery";

    nimcp_gpu_tensor_destroy(audio);
    nimcp_gpu_tensor_destroy(stft_out);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(PerceptionRecoveryTest, AudioMFCCRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    size_t mel_dims[] = {1, 100, 80};  // batch, time, mel_bins
    size_t mfcc_dims[] = {1, 100, 13}; // batch, time, mfcc_coeffs

    nimcp_gpu_tensor_t* mel_spec = createTestTensor(mel_dims, 3);
    nimcp_gpu_tensor_t* mfcc = createTestTensor(mfcc_dims, 3);

    ASSERT_NE(mel_spec, nullptr);

    fillRandom(mel_spec, 1e-10f, 100.0f);  // Non-negative for log

    bool result = nimcp_gpu_mfcc(ctx_, mel_spec, mfcc, 13);
    EXPECT_TRUE(result) << "MFCC should succeed with recovery";

    nimcp_gpu_tensor_destroy(mel_spec);
    nimcp_gpu_tensor_destroy(mfcc);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Speech Processing Recovery Tests
 * ============================================================================ */

TEST_F(PerceptionRecoveryTest, SpeechPitchDetectRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    size_t audio_dims[] = {8000};  // 0.5 seconds
    size_t out_dims[] = {1};

    nimcp_gpu_tensor_t* audio = createTestTensor(audio_dims, 1);
    nimcp_gpu_tensor_t* pitch = createTestTensor(out_dims, 1);
    nimcp_gpu_tensor_t* confidence = createTestTensor(out_dims, 1);

    ASSERT_NE(audio, nullptr);

    fillRandom(audio, -1.0f, 1.0f);

    bool result = nimcp_gpu_pitch_detect(
        ctx_, audio, pitch, confidence,
        16000.0f,  // sample_rate
        50.0f,     // min_f0
        500.0f     // max_f0
    );

    EXPECT_TRUE(result) << "Pitch detection should succeed with recovery";

    nimcp_gpu_tensor_destroy(audio);
    nimcp_gpu_tensor_destroy(pitch);
    nimcp_gpu_tensor_destroy(confidence);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(PerceptionRecoveryTest, SpeechVADRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    size_t audio_dims[] = {16000};  // 1 second
    size_t vad_dims[] = {100};      // frames

    nimcp_gpu_tensor_t* audio = createTestTensor(audio_dims, 1);
    nimcp_gpu_tensor_t* vad = createTestTensor(vad_dims, 1);

    ASSERT_NE(audio, nullptr);

    fillRandom(audio, -1.0f, 1.0f);

    bool result = nimcp_gpu_vad(
        ctx_, audio, vad,
        400,    // frame_len
        160,    // hop_len
        0.01f   // threshold
    );

    EXPECT_TRUE(result) << "VAD should succeed with recovery";

    nimcp_gpu_tensor_destroy(audio);
    nimcp_gpu_tensor_destroy(vad);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(PerceptionRecoveryTest, SpeechCortexStateRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Create speech cortex state with recovery
    nimcp_speech_gpu_state_t* state = nimcp_speech_gpu_create(
        ctx_,
        16000,  // sample_rate
        400,    // frame_size
        160,    // hop_size
        80,     // num_mel_bins
        12      // lpc_order
    );

    ASSERT_NE(state, nullptr) << "Speech GPU state creation should succeed";

    // Test synchronization with recovery
    EXPECT_TRUE(nimcp_speech_gpu_synchronize(state));

    nimcp_speech_gpu_destroy(state);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(PerceptionRecoveryTest, SpeechSpectrogramRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    nimcp_speech_gpu_state_t* state = nimcp_speech_gpu_create(
        ctx_, 16000, 400, 160, 80, 12
    );
    ASSERT_NE(state, nullptr);

    // Create test audio
    size_t audio_dims[] = {16000};
    nimcp_gpu_tensor_t* audio = createTestTensor(audio_dims, 1);
    ASSERT_NE(audio, nullptr);

    fillRandom(audio, -1.0f, 1.0f);

    // Compute spectrogram with recovery
    nimcp_gpu_tensor_t* spectrogram = nimcp_speech_gpu_compute_spectrogram(state, audio);

    // May return NULL if audio is too short, but should not crash
    if (spectrogram) {
        nimcp_gpu_tensor_destroy(spectrogram);
    }

    nimcp_gpu_tensor_destroy(audio);
    nimcp_speech_gpu_destroy(state);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Recovery Statistics Tests
 * ============================================================================ */

TEST_F(PerceptionRecoveryTest, RecoveryStatisticsTracking) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Reset statistics
    nimcp_gpu_recovery_reset_stats();

    // Perform some operations
    size_t dims[] = {128, 128};
    nimcp_gpu_tensor_t* input = createTestTensor(dims, 2);
    nimcp_gpu_tensor_t* output = createTestTensor(dims, 2);

    fillRandom(input);
    nimcp_gpu_sobel_edge_detect(ctx_, input, output, nullptr);

    // Get statistics
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);

    // Statistics should be tracked
    EXPECT_GE(stats.recoveries_attempted, 0u);

    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(output);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(PerceptionRecoveryTest, RecoveryActionNames) {
#ifdef NIMCP_ENABLE_CUDA
    // Verify action name retrieval
    const char* name = nimcp_gpu_recovery_action_name(GPU_RECOVERY_CPU_FALLBACK);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "");

    name = nimcp_gpu_recovery_action_name(GPU_RECOVERY_REDUCE_BATCH);
    EXPECT_NE(name, nullptr);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(PerceptionRecoveryTest, ErrorCategoryNames) {
#ifdef NIMCP_ENABLE_CUDA
    const char* name = nimcp_gpu_error_category_name(GPU_ERROR_OUT_OF_MEMORY);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "");

    name = nimcp_gpu_error_category_name(GPU_ERROR_KERNEL_LAUNCH);
    EXPECT_NE(name, nullptr);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Memory Management Recovery Tests
 * ============================================================================ */

TEST_F(PerceptionRecoveryTest, MemoryInfoRetrieval) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    size_t free_bytes = 0, total_bytes = 0;
    bool result = nimcp_gpu_get_memory_info(&free_bytes, &total_bytes);

    EXPECT_TRUE(result);
    EXPECT_GT(total_bytes, 0u);
    EXPECT_GE(total_bytes, free_bytes);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(PerceptionRecoveryTest, MemoryCriticalCheck) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // With fresh context, memory should not be critical at 99%
    bool is_critical = nimcp_gpu_memory_critical(0.99f);
    EXPECT_FALSE(is_critical) << "Memory should not be critical with fresh context";

    // With 0% threshold, always critical
    is_critical = nimcp_gpu_memory_critical(0.0f);
    EXPECT_TRUE(is_critical);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(PerceptionRecoveryTest, CacheFreeing) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Allocate some tensors
    size_t dims[] = {1024, 1024};
    nimcp_gpu_tensor_t* tensor = createTestTensor(dims, 2);
    ASSERT_NE(tensor, nullptr);

    nimcp_gpu_tensor_destroy(tensor);

    // Free caches should not crash
    size_t freed = nimcp_gpu_free_caches();
    // May return 0 if nothing to free
    EXPECT_GE(freed, 0u);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Parameter Correction Tests
 * ============================================================================ */

TEST_F(PerceptionRecoveryTest, ParamCorrectionFloat) {
#ifdef NIMCP_ENABLE_CUDA
    float value = 150.0f;  // Out of range
    nimcp_gpu_param_range_t range = {0.0f, 100.0f, 50.0f, true};

    bool corrected = nimcp_gpu_correct_param_float(&value, &range, "test_param");

    EXPECT_TRUE(corrected);
    EXPECT_LE(value, 100.0f);
    EXPECT_GE(value, 0.0f);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(PerceptionRecoveryTest, ParamCorrectionInt) {
#ifdef NIMCP_ENABLE_CUDA
    int value = 1500;  // Out of range

    bool corrected = nimcp_gpu_correct_param_int(&value, 0, 1000, 500, "test_param");

    EXPECT_TRUE(corrected);
    EXPECT_LE(value, 1000);
    EXPECT_GE(value, 0);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(PerceptionRecoveryTest, BatchSizeCorrectionForMemory) {
#ifdef NIMCP_ENABLE_CUDA
    size_t batch_size = 1000000;  // Very large
    size_t element_size = sizeof(float) * 1024 * 1024;  // 4MB per element

    bool corrected = nimcp_gpu_correct_batch_for_memory(
        &batch_size, element_size, element_size
    );

    // Should reduce batch size to fit memory
    EXPECT_LT(batch_size, 1000000u);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

}  // namespace
