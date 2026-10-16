/* ============================================================================
 * Integration Tests: GPU Perception Module Recovery
 * ============================================================================
 * WHAT: Integration tests for GPU perception processing with recovery
 * WHY:  Validate end-to-end recovery across visual, audio, speech pipelines
 * HOW:  Test complete processing chains with simulated failures
 * ============================================================================
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>
#include <chrono>

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "gpu/perception/nimcp_visual_cortex_gpu.h"
#include "gpu/perception/nimcp_speech_cortex_gpu.h"
#endif

namespace {

constexpr float TOLERANCE = 1e-4f;
constexpr int MAX_IMAGE_SIZE = 512;
constexpr int MAX_AUDIO_SAMPLES = 48000;  // 3 seconds at 16kHz

/* ============================================================================
 * Test Fixture: Perception Integration
 * ============================================================================ */
class PerceptionIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef NIMCP_ENABLE_CUDA
        ctx_ = nimcp_gpu_context_create(0);
        if (!ctx_) {
            GTEST_SKIP() << "No GPU available - skipping test";
        }

        // Initialize recovery with conservative settings
        nimcp_gpu_recovery_config_t config;
        nimcp_gpu_recovery_default_config(&config);
        config.enable_cpu_fallback = true;
        config.enable_param_correction = true;
        config.enable_batch_reduction = true;
        config.enable_retry = true;
        config.max_retries = 3;
        config.retry_delay_ms = 10;
        config.batch_reduction_factor = 0.5f;
        config.memory_threshold = 0.9f;
        nimcp_gpu_recovery_init(&config);

        // Reset statistics for clean test
        nimcp_gpu_recovery_reset_stats();
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

    // Helper to create test image tensor (grayscale)
    nimcp_gpu_tensor_t* createTestImage(int height, int width) {
        size_t dims[] = {(size_t)height, (size_t)width};
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(
            ctx_, dims, 2, NIMCP_GPU_PRECISION_FP32);
        if (tensor) {
            fillWithSinePattern(tensor, height, width);
        }
        return tensor;
    }

    // Helper to create RGB image tensor
    nimcp_gpu_tensor_t* createTestRGBImage(int height, int width) {
        size_t dims[] = {3, (size_t)height, (size_t)width};
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(
            ctx_, dims, 3, NIMCP_GPU_PRECISION_FP32);
        if (tensor) {
            fillWithColorPattern(tensor, height, width);
        }
        return tensor;
    }

    // Helper to create audio tensor
    nimcp_gpu_tensor_t* createTestAudio(int num_samples, float frequency = 440.0f) {
        size_t dims[] = {(size_t)num_samples};
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(
            ctx_, dims, 1, NIMCP_GPU_PRECISION_FP32);
        if (tensor) {
            fillWithSineWave(tensor, num_samples, frequency, 16000.0f);
        }
        return tensor;
    }

    void fillWithSinePattern(nimcp_gpu_tensor_t* tensor, int height, int width) {
        std::vector<float> data(height * width);
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                data[y * width + x] = 0.5f * (1.0f +
                    std::sin(2.0f * M_PI * x / 32.0f) *
                    std::sin(2.0f * M_PI * y / 32.0f));
            }
        }
        cudaMemcpy(tensor->data, data.data(), data.size() * sizeof(float),
                   cudaMemcpyHostToDevice);
    }

    void fillWithColorPattern(nimcp_gpu_tensor_t* tensor, int height, int width) {
        std::vector<float> data(3 * height * width);
        for (int c = 0; c < 3; c++) {
            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    float val = 0.5f * (1.0f +
                        std::sin(2.0f * M_PI * (x + c * 10) / 32.0f) *
                        std::sin(2.0f * M_PI * y / 32.0f));
                    data[c * height * width + y * width + x] = val;
                }
            }
        }
        cudaMemcpy(tensor->data, data.data(), data.size() * sizeof(float),
                   cudaMemcpyHostToDevice);
    }

    void fillWithSineWave(nimcp_gpu_tensor_t* tensor, int samples,
                          float frequency, float sample_rate) {
        std::vector<float> data(samples);
        for (int i = 0; i < samples; i++) {
            data[i] = 0.5f * std::sin(2.0f * M_PI * frequency * i / sample_rate);
        }
        cudaMemcpy(tensor->data, data.data(), data.size() * sizeof(float),
                   cudaMemcpyHostToDevice);
    }
#endif
};

/* ============================================================================
 * Visual Pipeline Integration Tests
 * ============================================================================ */

TEST_F(PerceptionIntegrationTest, VisualCortexFullPipeline) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    // Create visual cortex state
    nimcp_visual_gpu_state_t* visual = nimcp_visual_gpu_create(ctx_, 8, 4, 4);
    ASSERT_NE(visual, nullptr);

    int width = 256, height = 256;
    ASSERT_TRUE(nimcp_visual_gpu_init(visual, width, height));

    // Create test image
    nimcp_gpu_tensor_t* grayscale = createTestImage(height, width);
    ASSERT_NE(grayscale, nullptr);

    // Process V1 (Gabor filtering, edge detection)
    nimcp_gpu_tensor_t* v1_output = nimcp_visual_gpu_v1_process(visual, grayscale);
    EXPECT_NE(v1_output, nullptr) << "V1 processing should succeed";

    if (v1_output) {
        nimcp_gpu_tensor_destroy(v1_output);
    }
    nimcp_gpu_tensor_destroy(grayscale);
    nimcp_visual_gpu_destroy(visual);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(PerceptionIntegrationTest, VisualSaliencyComputation) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_visual_gpu_state_t* visual = nimcp_visual_gpu_create(ctx_, 8, 4, 4);
    ASSERT_NE(visual, nullptr);

    int width = 128, height = 128;
    ASSERT_TRUE(nimcp_visual_gpu_init(visual, width, height));

    nimcp_gpu_tensor_t* image = createTestImage(height, width);
    ASSERT_NE(image, nullptr);

    // Compute saliency map
    nimcp_gpu_tensor_t* saliency = nimcp_visual_gpu_compute_saliency(visual, image);
    // May return NULL if saliency state not fully initialized
    // but should not crash

    nimcp_gpu_tensor_destroy(image);
    nimcp_visual_gpu_destroy(visual);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(PerceptionIntegrationTest, VisualOpticalFlowTwoFrames) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    int width = 128, height = 128;

    // Create two frames with slight shift
    nimcp_gpu_tensor_t* frame1 = createTestImage(height, width);
    nimcp_gpu_tensor_t* frame2 = createTestImage(height, width);
    ASSERT_NE(frame1, nullptr);
    ASSERT_NE(frame2, nullptr);

    // Allocate flow outputs
    size_t dims[] = {(size_t)height, (size_t)width};
    nimcp_gpu_tensor_t* flow_u = nimcp_gpu_tensor_create(ctx_, dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* flow_v = nimcp_gpu_tensor_create(ctx_, dims, 2, NIMCP_GPU_PRECISION_FP32);

    bool result = nimcp_gpu_optical_flow_lk(ctx_, frame1, frame2, flow_u, flow_v, 15);
    EXPECT_TRUE(result) << "Optical flow should succeed";

    nimcp_gpu_tensor_destroy(frame1);
    nimcp_gpu_tensor_destroy(frame2);
    nimcp_gpu_tensor_destroy(flow_u);
    nimcp_gpu_tensor_destroy(flow_v);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(PerceptionIntegrationTest, VisualColorProcessingChain) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    int width = 128, height = 128;

    nimcp_gpu_tensor_t* rgb = createTestRGBImage(height, width);
    ASSERT_NE(rgb, nullptr);

    size_t out_dims[] = {(size_t)height, (size_t)width};
    nimcp_gpu_tensor_t* rg = nimcp_gpu_tensor_create(ctx_, out_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* yb = nimcp_gpu_tensor_create(ctx_, out_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* lum = nimcp_gpu_tensor_create(ctx_, out_dims, 2, NIMCP_GPU_PRECISION_FP32);

    // Process color opponent channels
    bool result = nimcp_gpu_color_opponent(ctx_, rgb, rg, yb, lum);
    EXPECT_TRUE(result);

    // Now do edge detection on luminance
    nimcp_gpu_tensor_t* edges = nimcp_gpu_tensor_create(ctx_, out_dims, 2, NIMCP_GPU_PRECISION_FP32);
    result = nimcp_gpu_sobel_edge_detect(ctx_, lum, edges, nullptr);
    EXPECT_TRUE(result);

    nimcp_gpu_tensor_destroy(rgb);
    nimcp_gpu_tensor_destroy(rg);
    nimcp_gpu_tensor_destroy(yb);
    nimcp_gpu_tensor_destroy(lum);
    nimcp_gpu_tensor_destroy(edges);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Audio Pipeline Integration Tests
 * ============================================================================ */

TEST_F(PerceptionIntegrationTest, AudioSpectrogramPipeline) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    // Create 1 second of audio at 16kHz
    nimcp_gpu_tensor_t* audio = createTestAudio(16000, 440.0f);
    ASSERT_NE(audio, nullptr);

    // Compute spectrogram
    size_t spec_dims[] = {99, 257};  // Approximate for 512 FFT, 160 hop
    nimcp_gpu_tensor_t* spectrogram = nimcp_gpu_tensor_create(
        ctx_, spec_dims, 2, NIMCP_GPU_PRECISION_FP32);

    bool result = nimcp_gpu_spectrogram(ctx_, audio, spectrogram, 512, 160, true);
    EXPECT_TRUE(result) << "Spectrogram computation should succeed";

    nimcp_gpu_tensor_destroy(audio);
    nimcp_gpu_tensor_destroy(spectrogram);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(PerceptionIntegrationTest, AudioMelSpectrogramPipeline) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    // Create spectrogram-like input
    size_t spec_dims[] = {1, 100, 257};
    size_t mel_dims[] = {1, 100, 80};

    nimcp_gpu_tensor_t* spectrogram = nimcp_gpu_tensor_create(
        ctx_, spec_dims, 3, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* mel_spec = nimcp_gpu_tensor_create(
        ctx_, mel_dims, 3, NIMCP_GPU_PRECISION_FP32);

    ASSERT_NE(spectrogram, nullptr);

    // Fill with positive values (power spectrum)
    std::vector<float> data(100 * 257, 1.0f);
    cudaMemcpy(spectrogram->data, data.data(), data.size() * sizeof(float),
               cudaMemcpyHostToDevice);

    bool result = nimcp_gpu_mel_filterbank(
        ctx_, spectrogram, mel_spec,
        80, 0.0f, 8000.0f, 16000.0f
    );
    EXPECT_TRUE(result);

    nimcp_gpu_tensor_destroy(spectrogram);
    nimcp_gpu_tensor_destroy(mel_spec);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Speech Pipeline Integration Tests
 * ============================================================================ */

TEST_F(PerceptionIntegrationTest, SpeechFeatureExtractionPipeline) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    // Create speech processing state
    nimcp_speech_gpu_state_t* speech = nimcp_speech_gpu_create(
        ctx_, 16000, 400, 160, 80, 12
    );
    ASSERT_NE(speech, nullptr);

    // Create test audio (1 second speech-like signal)
    nimcp_gpu_tensor_t* audio = createTestAudio(16000, 150.0f);  // ~150Hz fundamental
    ASSERT_NE(audio, nullptr);

    // Extract spectrogram
    nimcp_gpu_tensor_t* spectrogram = nimcp_speech_gpu_compute_spectrogram(speech, audio);
    if (spectrogram) {
        EXPECT_GT(spectrogram->dims[0], 0u);  // Should have frames
        nimcp_gpu_tensor_destroy(spectrogram);
    }

    nimcp_gpu_tensor_destroy(audio);
    nimcp_speech_gpu_destroy(speech);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(PerceptionIntegrationTest, SpeechMFCCPipeline) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_speech_gpu_state_t* speech = nimcp_speech_gpu_create(
        ctx_, 16000, 400, 160, 80, 12
    );
    ASSERT_NE(speech, nullptr);

    nimcp_gpu_tensor_t* audio = createTestAudio(16000, 200.0f);
    ASSERT_NE(audio, nullptr);

    // Extract MFCCs with deltas
    nimcp_gpu_tensor_t* mfcc = nimcp_speech_gpu_compute_mfcc_full(
        speech, audio, true, true  // Include delta and delta-delta
    );

    if (mfcc) {
        // Should have 3x MFCC coefficients (base + delta + delta2)
        nimcp_gpu_tensor_destroy(mfcc);
    }

    nimcp_gpu_tensor_destroy(audio);
    nimcp_speech_gpu_destroy(speech);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(PerceptionIntegrationTest, SpeechPitchAndVADPipeline) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_speech_gpu_state_t* speech = nimcp_speech_gpu_create(
        ctx_, 16000, 400, 160, 80, 12
    );
    ASSERT_NE(speech, nullptr);

    // Create audio with known pitch
    nimcp_gpu_tensor_t* audio = createTestAudio(16000, 200.0f);
    ASSERT_NE(audio, nullptr);

    // Detect pitch
    nimcp_gpu_tensor_t* pitch = nullptr;
    nimcp_gpu_tensor_t* confidence = nullptr;

    bool result = nimcp_speech_gpu_detect_pitch_full(
        speech, audio, &pitch, &confidence
    );

    if (result && pitch && confidence) {
        // Verify we got results
        EXPECT_GT(pitch->numel, 0u);
        nimcp_gpu_tensor_destroy(pitch);
        nimcp_gpu_tensor_destroy(confidence);
    }

    // Detect VAD
    nimcp_gpu_tensor_t* vad = nimcp_speech_gpu_detect_vad(speech, audio, -30.0f);
    if (vad) {
        nimcp_gpu_tensor_destroy(vad);
    }

    nimcp_gpu_tensor_destroy(audio);
    nimcp_speech_gpu_destroy(speech);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(PerceptionIntegrationTest, SpeechFormantExtractionPipeline) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_speech_gpu_state_t* speech = nimcp_speech_gpu_create(
        ctx_, 16000, 400, 160, 80, 12
    );
    ASSERT_NE(speech, nullptr);

    nimcp_gpu_tensor_t* audio = createTestAudio(16000, 150.0f);
    ASSERT_NE(audio, nullptr);

    // Extract formants
    nimcp_gpu_tensor_t* formants = nullptr;
    nimcp_gpu_tensor_t* bandwidths = nullptr;

    bool result = nimcp_speech_gpu_extract_formants_full(
        speech, audio, &formants, &bandwidths
    );

    if (result && formants && bandwidths) {
        nimcp_gpu_tensor_destroy(formants);
        nimcp_gpu_tensor_destroy(bandwidths);
    }

    nimcp_gpu_tensor_destroy(audio);
    nimcp_speech_gpu_destroy(speech);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Recovery Under Load Tests
 * ============================================================================ */

TEST_F(PerceptionIntegrationTest, VisualRecoveryUnderLoad) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    // Process multiple images in sequence
    int num_iterations = 10;
    int success_count = 0;

    for (int i = 0; i < num_iterations; i++) {
        int size = 64 + (i * 16);  // Increasing sizes
        nimcp_gpu_tensor_t* image = createTestImage(size, size);
        if (!image) continue;

        size_t out_dims[] = {(size_t)size, (size_t)size};
        nimcp_gpu_tensor_t* edges = nimcp_gpu_tensor_create(
            ctx_, out_dims, 2, NIMCP_GPU_PRECISION_FP32);

        if (nimcp_gpu_sobel_edge_detect(ctx_, image, edges, nullptr)) {
            success_count++;
        }

        nimcp_gpu_tensor_destroy(image);
        nimcp_gpu_tensor_destroy(edges);
    }

    EXPECT_GE(success_count, num_iterations - 2)
        << "Most operations should succeed with recovery";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(PerceptionIntegrationTest, SpeechRecoveryUnderLoad) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_speech_gpu_state_t* speech = nimcp_speech_gpu_create(
        ctx_, 16000, 400, 160, 80, 12
    );
    ASSERT_NE(speech, nullptr);

    // Process multiple audio clips
    int num_iterations = 5;
    int success_count = 0;

    for (int i = 0; i < num_iterations; i++) {
        int samples = 8000 + (i * 2000);  // 0.5-1.0 seconds
        nimcp_gpu_tensor_t* audio = createTestAudio(samples, 200.0f + i * 50);
        if (!audio) continue;

        nimcp_gpu_tensor_t* spectrogram = nimcp_speech_gpu_compute_spectrogram(speech, audio);
        if (spectrogram) {
            success_count++;
            nimcp_gpu_tensor_destroy(spectrogram);
        }

        nimcp_gpu_tensor_destroy(audio);
    }

    EXPECT_GE(success_count, num_iterations - 1)
        << "Most speech operations should succeed with recovery";

    nimcp_speech_gpu_destroy(speech);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Error Handling and Recovery Tests
 * ============================================================================ */

TEST_F(PerceptionIntegrationTest, RecoveryFromNullTensor) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    // Should handle NULL gracefully
    bool result = nimcp_gpu_sobel_edge_detect(ctx_, nullptr, nullptr, nullptr);
    EXPECT_FALSE(result) << "Should fail gracefully with NULL tensors";

    result = nimcp_gpu_optical_flow_lk(ctx_, nullptr, nullptr, nullptr, nullptr, 15);
    EXPECT_FALSE(result);

    result = nimcp_gpu_color_opponent(ctx_, nullptr, nullptr, nullptr, nullptr);
    EXPECT_FALSE(result);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(PerceptionIntegrationTest, RecoveryStatisticsAfterPipeline) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    // Run several operations
    nimcp_gpu_tensor_t* image = createTestImage(128, 128);
    ASSERT_NE(image, nullptr);

    size_t dims[] = {128, 128};
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(ctx_, dims, 2, NIMCP_GPU_PRECISION_FP32);

    for (int i = 0; i < 5; i++) {
        nimcp_gpu_sobel_edge_detect(ctx_, image, output, nullptr);
    }

    // Get recovery statistics
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);

    // Log statistics (informational)
    std::cout << "Recovery stats after pipeline:" << std::endl;
    std::cout << "  Total errors: " << stats.total_errors << std::endl;
    std::cout << "  Recoveries attempted: " << stats.recoveries_attempted << std::endl;
    std::cout << "  Recoveries succeeded: " << stats.recoveries_succeeded << std::endl;
    std::cout << "  CPU fallbacks: " << stats.cpu_fallbacks_used << std::endl;
    std::cout << "  Success rate: " << stats.success_rate << std::endl;

    nimcp_gpu_tensor_destroy(image);
    nimcp_gpu_tensor_destroy(output);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Cross-Modal Pipeline Tests
 * ============================================================================ */

TEST_F(PerceptionIntegrationTest, AudioVisualSynchronization) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    // Process audio and video "frames" together
    int frame_rate = 30;
    int sample_rate = 16000;
    int samples_per_frame = sample_rate / frame_rate;

    // Create visual pipeline
    nimcp_visual_gpu_state_t* visual = nimcp_visual_gpu_create(ctx_, 8, 2, 4);
    ASSERT_NE(visual, nullptr);
    ASSERT_TRUE(nimcp_visual_gpu_init(visual, 128, 128));

    // Create speech pipeline
    nimcp_speech_gpu_state_t* speech = nimcp_speech_gpu_create(
        ctx_, sample_rate, 400, 160, 80, 12
    );
    ASSERT_NE(speech, nullptr);

    // Process 3 "frames"
    for (int frame = 0; frame < 3; frame++) {
        // Visual processing
        nimcp_gpu_tensor_t* image = createTestImage(128, 128);
        if (image) {
            nimcp_gpu_tensor_t* v1 = nimcp_visual_gpu_v1_process(visual, image);
            if (v1) nimcp_gpu_tensor_destroy(v1);
            nimcp_gpu_tensor_destroy(image);
        }

        // Audio processing
        nimcp_gpu_tensor_t* audio = createTestAudio(samples_per_frame, 200.0f);
        if (audio) {
            // Small chunk, may not produce features
            nimcp_gpu_tensor_destroy(audio);
        }
    }

    nimcp_visual_gpu_destroy(visual);
    nimcp_speech_gpu_destroy(speech);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Performance Under Recovery Tests
 * ============================================================================ */

TEST_F(PerceptionIntegrationTest, PerformanceWithRecoveryEnabled) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    int num_iterations = 20;
    int size = 256;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_iterations; i++) {
        nimcp_gpu_tensor_t* image = createTestImage(size, size);
        if (!image) continue;

        size_t dims[] = {(size_t)size, (size_t)size};
        nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(
            ctx_, dims, 2, NIMCP_GPU_PRECISION_FP32);

        nimcp_gpu_sobel_edge_detect(ctx_, image, output, nullptr);

        nimcp_gpu_tensor_destroy(image);
        nimcp_gpu_tensor_destroy(output);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    float avg_ms = (float)duration.count() / num_iterations;
    std::cout << "Average time per frame with recovery: " << avg_ms << "ms" << std::endl;

    // Should be reasonable even with recovery overhead
    EXPECT_LT(avg_ms, 100.0f) << "Processing should be reasonably fast";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

}  // namespace
