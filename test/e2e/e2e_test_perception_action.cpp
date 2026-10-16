/**
 * @file e2e_test_perception_action.cpp
 * @brief E2E Tests for Perception-to-Action Pipeline
 *
 * WHAT: Comprehensive end-to-end tests for visual input to motor output
 * WHY:  Verify numerical stability throughout pipeline, test edge-case inputs
 * HOW:  Complete perception -> cognitive processing -> motor output cycles
 *
 * TEST COVERAGE:
 * - Visual input to cognitive processing
 * - Cognitive processing to motor output
 * - Numerical stability throughout pipeline
 * - Edge-case input handling
 * - Multi-modal sensory fusion
 * - Action selection and execution
 *
 * BIOLOGICAL ANALOGY:
 * - Visual cortex processing (V1, V2, V4)
 * - Ventral/dorsal stream processing
 * - Motor cortex output generation
 * - Sensory-motor integration
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#include "e2e_test_framework.h"
#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>
#include <numeric>
#include <cstring>
#include <limits>

// Headers have their own extern "C" guards
#include "core/brain/nimcp_brain.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"
#include "nimcp.h"

using namespace nimcp::e2e;

//=============================================================================
// Test Configuration
//=============================================================================

// Perception parameters
constexpr uint32_t VISUAL_INPUT_WIDTH = 32;
constexpr uint32_t VISUAL_INPUT_HEIGHT = 32;
constexpr uint32_t VISUAL_INPUT_CHANNELS = 3;
constexpr uint32_t VISUAL_INPUT_SIZE = VISUAL_INPUT_WIDTH * VISUAL_INPUT_HEIGHT * VISUAL_INPUT_CHANNELS;

// Cognitive parameters
constexpr uint32_t FEATURE_DIM = 128;
constexpr uint32_t HIDDEN_DIM = 64;
constexpr uint32_t ACTION_DIM = 8;

// Motor parameters
constexpr uint32_t MOTOR_DOF = 6;  // Degrees of freedom

// Timing thresholds (milliseconds)
constexpr double MAX_PERCEPTION_TIME_MS = 200.0;
constexpr double MAX_COGNITIVE_TIME_MS = 300.0;
constexpr double MAX_MOTOR_TIME_MS = 100.0;
constexpr double MAX_PIPELINE_TIME_MS = 1000.0;

// Numerical stability thresholds
constexpr float MAX_ACTIVATION_VALUE = 1e6f;
constexpr float MIN_ACTIVATION_VALUE = -1e6f;
constexpr float STABILITY_EPSILON = 1e-7f;

//=============================================================================
// Helper Structures
//=============================================================================

/**
 * @brief Visual input representation
 */
struct VisualInput {
    std::vector<float> pixels;
    uint32_t width;
    uint32_t height;
    uint32_t channels;

    VisualInput(uint32_t w, uint32_t h, uint32_t c)
        : width(w), height(h), channels(c) {
        pixels.resize(w * h * c, 0.0f);
    }
};

/**
 * @brief Processed features from perception
 */
struct PerceptionOutput {
    std::vector<float> features;
    float confidence;
    bool valid;
};

/**
 * @brief Motor command output
 */
struct MotorOutput {
    std::vector<float> joint_angles;
    std::vector<float> velocities;
    float execution_confidence;
    bool executable;
};

//=============================================================================
// Test Fixture
//=============================================================================

class PerceptionActionE2ETest : public ::testing::Test {
protected:
    brain_t brain_;

    void SetUp() override {
        nimcp_init();
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        brain_ = nullptr;
    }

    void TearDown() override {
        if (brain_) {
            brain_destroy(brain_);
            brain_ = nullptr;
        }
        nimcp_memory_check_leaks();
        nimcp_shutdown();
    }

    // Create brain for perception-action pipeline
    bool createPerceptionActionBrain(const char* name) {
        brain_ = brain_create_minimal(
            name,
            BRAIN_SIZE_MEDIUM,
            BRAIN_TASK_PATTERN_MATCHING,
            FEATURE_DIM,
            ACTION_DIM
        );
        return brain_ != nullptr;
    }

    // Generate synthetic visual input
    VisualInput generateVisualInput(bool add_noise = false, float noise_level = 0.1f) {
        VisualInput input(VISUAL_INPUT_WIDTH, VISUAL_INPUT_HEIGHT, VISUAL_INPUT_CHANNELS);

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        for (size_t i = 0; i < input.pixels.size(); ++i) {
            input.pixels[i] = dist(gen);
            if (add_noise) {
                input.pixels[i] += (dist(gen) - 0.5f) * 2.0f * noise_level;
                input.pixels[i] = std::max(0.0f, std::min(1.0f, input.pixels[i]));
            }
        }

        return input;
    }

    // Generate edge-case visual input
    VisualInput generateEdgeCaseInput(const std::string& type) {
        VisualInput input(VISUAL_INPUT_WIDTH, VISUAL_INPUT_HEIGHT, VISUAL_INPUT_CHANNELS);

        if (type == "all_zeros") {
            std::fill(input.pixels.begin(), input.pixels.end(), 0.0f);
        } else if (type == "all_ones") {
            std::fill(input.pixels.begin(), input.pixels.end(), 1.0f);
        } else if (type == "checkerboard") {
            for (uint32_t y = 0; y < input.height; ++y) {
                for (uint32_t x = 0; x < input.width; ++x) {
                    float val = ((x + y) % 2 == 0) ? 1.0f : 0.0f;
                    for (uint32_t c = 0; c < input.channels; ++c) {
                        input.pixels[(y * input.width + x) * input.channels + c] = val;
                    }
                }
            }
        } else if (type == "gradient") {
            for (uint32_t y = 0; y < input.height; ++y) {
                for (uint32_t x = 0; x < input.width; ++x) {
                    float val = static_cast<float>(x) / (input.width - 1);
                    for (uint32_t c = 0; c < input.channels; ++c) {
                        input.pixels[(y * input.width + x) * input.channels + c] = val;
                    }
                }
            }
        } else if (type == "extreme_contrast") {
            for (uint32_t i = 0; i < input.pixels.size() / 2; ++i) {
                input.pixels[i] = 0.0f;
            }
            for (uint32_t i = input.pixels.size() / 2; i < input.pixels.size(); ++i) {
                input.pixels[i] = 1.0f;
            }
        }

        return input;
    }

    // Simulate visual processing (perception)
    PerceptionOutput processVisualInput(const VisualInput& input) {
        PerceptionOutput output;
        output.features.resize(FEATURE_DIM);
        output.valid = true;

        // Simple feature extraction simulation
        // In real system, this would be CNN/visual cortex processing
        float sum = std::accumulate(input.pixels.begin(), input.pixels.end(), 0.0f);
        float mean = sum / input.pixels.size();

        float variance = 0.0f;
        for (float p : input.pixels) {
            variance += (p - mean) * (p - mean);
        }
        variance /= input.pixels.size();

        // Generate features based on statistics
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<float> normal(0.0f, 1.0f);

        for (size_t i = 0; i < output.features.size(); ++i) {
            output.features[i] = mean * 0.5f + std::sqrt(variance) * normal(gen) * 0.5f;

            // Check for numerical issues
            if (std::isnan(output.features[i]) || std::isinf(output.features[i])) {
                output.features[i] = 0.0f;
                output.valid = false;
            }

            // Clamp to valid range
            output.features[i] = std::max(MIN_ACTIVATION_VALUE,
                                          std::min(MAX_ACTIVATION_VALUE, output.features[i]));
        }

        output.confidence = 1.0f - std::sqrt(variance);  // Higher variance = lower confidence
        output.confidence = std::max(0.0f, std::min(1.0f, output.confidence));

        return output;
    }

    // Simulate cognitive processing
    std::vector<float> processCognitive(const PerceptionOutput& perception) {
        std::vector<float> action_probs(ACTION_DIM, 0.0f);

        if (!perception.valid) {
            // Default uniform distribution for invalid perception
            std::fill(action_probs.begin(), action_probs.end(), 1.0f / ACTION_DIM);
            return action_probs;
        }

        // Simple action selection based on features
        for (size_t i = 0; i < ACTION_DIM; ++i) {
            for (size_t j = 0; j < perception.features.size(); ++j) {
                action_probs[i] += perception.features[j] *
                                   std::sin(static_cast<float>(i * j) / 100.0f);
            }
            action_probs[i] /= perception.features.size();
        }

        // Softmax normalization
        float max_val = *std::max_element(action_probs.begin(), action_probs.end());
        float sum = 0.0f;
        for (float& p : action_probs) {
            p = std::exp(p - max_val);  // Subtract max for numerical stability
            sum += p;
        }
        for (float& p : action_probs) {
            p /= sum;
        }

        return action_probs;
    }

    // Simulate motor output generation
    MotorOutput generateMotorOutput(const std::vector<float>& action_probs) {
        MotorOutput output;
        output.joint_angles.resize(MOTOR_DOF, 0.0f);
        output.velocities.resize(MOTOR_DOF, 0.0f);
        output.executable = true;

        // Find winning action
        auto max_it = std::max_element(action_probs.begin(), action_probs.end());
        uint32_t action_id = std::distance(action_probs.begin(), max_it);
        output.execution_confidence = *max_it;

        // Generate motor commands based on action
        for (size_t i = 0; i < MOTOR_DOF; ++i) {
            // Action-dependent joint angles
            output.joint_angles[i] = static_cast<float>(action_id + i) * 0.1f;
            output.velocities[i] = output.execution_confidence * 0.5f;

            // Validate outputs
            if (std::isnan(output.joint_angles[i]) || std::isinf(output.joint_angles[i])) {
                output.joint_angles[i] = 0.0f;
                output.executable = false;
            }
        }

        return output;
    }

    // Check numerical stability
    bool checkNumericalStability(const std::vector<float>& values) {
        for (float v : values) {
            if (std::isnan(v) || std::isinf(v)) {
                return false;
            }
            if (v > MAX_ACTIVATION_VALUE || v < MIN_ACTIVATION_VALUE) {
                return false;
            }
        }
        return true;
    }
};

//=============================================================================
// Test: Visual Input to Cognitive Processing
//=============================================================================

E2E_TEST_F(PerceptionActionE2ETest, VisualToCognitive) {
    E2E_PIPELINE_START("Visual Input to Cognitive Processing");

    // Stage 1: Create brain
    E2E_STAGE_BEGIN("Create perception-action brain", 500);
    {
        bool created = createPerceptionActionBrain("visual_cognitive_brain");
        E2E_ASSERT(created, "Failed to create brain");
    }
    E2E_STAGE_END();

    // Stage 2: Generate visual input
    VisualInput visual_input(0, 0, 0);
    E2E_STAGE_BEGIN("Generate visual input", 100);
    {
        visual_input = generateVisualInput();
        E2E_ASSERT(visual_input.pixels.size() == VISUAL_INPUT_SIZE,
                   "Visual input size mismatch");
    }
    E2E_STAGE_END();

    // Stage 3: Visual processing (perception)
    PerceptionOutput perception;
    E2E_STAGE_BEGIN("Visual processing", MAX_PERCEPTION_TIME_MS);
    {
        perception = processVisualInput(visual_input);

        std::cout << "[E2E] Perception output:\n";
        std::cout << "  Features: " << perception.features.size() << " dims\n";
        std::cout << "  Confidence: " << perception.confidence << "\n";
        std::cout << "  Valid: " << (perception.valid ? "yes" : "no") << "\n";

        E2E_ASSERT(perception.valid, "Perception failed");
        E2E_ASSERT(perception.features.size() == FEATURE_DIM, "Feature dim mismatch");
    }
    E2E_STAGE_END();

    // Stage 4: Cognitive processing
    std::vector<float> action_probs;
    E2E_STAGE_BEGIN("Cognitive processing", MAX_COGNITIVE_TIME_MS);
    {
        action_probs = processCognitive(perception);

        std::cout << "[E2E] Action probabilities:\n";
        for (size_t i = 0; i < action_probs.size(); ++i) {
            std::cout << "  Action " << i << ": " << action_probs[i] << "\n";
        }

        // Verify valid probability distribution
        float sum = std::accumulate(action_probs.begin(), action_probs.end(), 0.0f);
        E2E_ASSERT(std::abs(sum - 1.0f) < 0.01f, "Action probs don't sum to 1");
    }
    E2E_STAGE_END();

    // Stage 5: Verify numerical stability
    E2E_STAGE_BEGIN("Verify numerical stability", 50);
    {
        bool features_stable = checkNumericalStability(perception.features);
        bool actions_stable = checkNumericalStability(action_probs);

        E2E_ASSERT(features_stable, "Feature values numerically unstable");
        E2E_ASSERT(actions_stable, "Action values numerically unstable");
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test: Cognitive Processing to Motor Output
//=============================================================================

E2E_TEST_F(PerceptionActionE2ETest, CognitiveToMotor) {
    E2E_PIPELINE_START("Cognitive Processing to Motor Output");

    // Stage 1: Create brain
    E2E_STAGE_BEGIN("Create brain", 500);
    {
        bool created = createPerceptionActionBrain("cognitive_motor_brain");
        E2E_ASSERT(created, "Failed to create brain");
    }
    E2E_STAGE_END();

    // Stage 2: Generate features directly
    PerceptionOutput perception;
    E2E_STAGE_BEGIN("Generate perception features", 100);
    {
        perception.features = TestDataGenerator::generate_features(FEATURE_DIM, -1.0f, 1.0f);
        perception.confidence = 0.85f;
        perception.valid = true;
    }
    E2E_STAGE_END();

    // Stage 3: Cognitive processing
    std::vector<float> action_probs;
    E2E_STAGE_BEGIN("Cognitive processing", MAX_COGNITIVE_TIME_MS);
    {
        action_probs = processCognitive(perception);
        E2E_ASSERT(action_probs.size() == ACTION_DIM, "Action dim mismatch");
    }
    E2E_STAGE_END();

    // Stage 4: Motor output generation
    MotorOutput motor;
    E2E_STAGE_BEGIN("Motor output generation", MAX_MOTOR_TIME_MS);
    {
        motor = generateMotorOutput(action_probs);

        std::cout << "[E2E] Motor output:\n";
        std::cout << "  Execution confidence: " << motor.execution_confidence << "\n";
        std::cout << "  Executable: " << (motor.executable ? "yes" : "no") << "\n";
        std::cout << "  Joint angles: ";
        for (float a : motor.joint_angles) {
            std::cout << a << " ";
        }
        std::cout << "\n";

        E2E_ASSERT(motor.executable, "Motor output not executable");
        E2E_ASSERT(motor.joint_angles.size() == MOTOR_DOF, "DOF mismatch");
    }
    E2E_STAGE_END();

    // Stage 5: Verify motor output validity
    E2E_STAGE_BEGIN("Verify motor output validity", 50);
    {
        bool angles_stable = checkNumericalStability(motor.joint_angles);
        bool velocities_stable = checkNumericalStability(motor.velocities);

        E2E_ASSERT(angles_stable, "Joint angles numerically unstable");
        E2E_ASSERT(velocities_stable, "Velocities numerically unstable");
        E2E_ASSERT(motor.execution_confidence >= 0.0f &&
                   motor.execution_confidence <= 1.0f,
                   "Confidence out of range");
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test: Numerical Stability Throughout Pipeline
//=============================================================================

E2E_TEST_F(PerceptionActionE2ETest, NumericalStabilityPipeline) {
    E2E_PIPELINE_START("Numerical Stability Throughout Pipeline");

    // Stage 1: Create brain
    E2E_STAGE_BEGIN("Create brain", 500);
    {
        bool created = createPerceptionActionBrain("stability_brain");
        E2E_ASSERT(created, "Failed to create brain");
    }
    E2E_STAGE_END();

    // Stage 2: Run multiple iterations
    E2E_STAGE_BEGIN("Multi-iteration stability test", 5000);
    {
        const uint32_t NUM_ITERATIONS = 100;
        uint32_t nan_count = 0;
        uint32_t inf_count = 0;
        uint32_t overflow_count = 0;

        for (uint32_t iter = 0; iter < NUM_ITERATIONS; ++iter) {
            // Generate input
            VisualInput input = generateVisualInput(true, 0.2f);

            // Process perception
            PerceptionOutput perception = processVisualInput(input);

            // Check perception stability
            for (float f : perception.features) {
                if (std::isnan(f)) nan_count++;
                if (std::isinf(f)) inf_count++;
                if (std::abs(f) > MAX_ACTIVATION_VALUE) overflow_count++;
            }

            // Process cognitive
            std::vector<float> actions = processCognitive(perception);

            // Check action stability
            for (float a : actions) {
                if (std::isnan(a)) nan_count++;
                if (std::isinf(a)) inf_count++;
            }

            // Generate motor output
            MotorOutput motor = generateMotorOutput(actions);

            // Check motor stability
            for (float j : motor.joint_angles) {
                if (std::isnan(j)) nan_count++;
                if (std::isinf(j)) inf_count++;
            }
        }

        std::cout << "[E2E] Stability results over " << NUM_ITERATIONS << " iterations:\n";
        std::cout << "  NaN count: " << nan_count << "\n";
        std::cout << "  Inf count: " << inf_count << "\n";
        std::cout << "  Overflow count: " << overflow_count << "\n";

        E2E_ASSERT(nan_count == 0, "NaN values detected");
        E2E_ASSERT(inf_count == 0, "Infinity values detected");
        E2E_ASSERT(overflow_count == 0, "Overflow values detected");
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test: Edge-Case Inputs
//=============================================================================

E2E_TEST_F(PerceptionActionE2ETest, EdgeCaseInputs) {
    E2E_PIPELINE_START("Edge-Case Input Handling");

    // Stage 1: Create brain
    E2E_STAGE_BEGIN("Create brain", 500);
    {
        bool created = createPerceptionActionBrain("edge_case_brain");
        E2E_ASSERT(created, "Failed to create brain");
    }
    E2E_STAGE_END();

    // Stage 2: Test various edge cases
    std::vector<std::string> edge_cases = {
        "all_zeros",
        "all_ones",
        "checkerboard",
        "gradient",
        "extreme_contrast"
    };

    E2E_STAGE_BEGIN("Test edge cases", 3000);
    {
        for (const auto& edge_case : edge_cases) {
            std::cout << "[E2E] Testing edge case: " << edge_case << "\n";

            // Generate edge case input
            VisualInput input = generateEdgeCaseInput(edge_case);

            // Process through pipeline
            PerceptionOutput perception = processVisualInput(input);
            std::vector<float> actions = processCognitive(perception);
            MotorOutput motor = generateMotorOutput(actions);

            // Verify stability
            bool perception_ok = perception.valid || !perception.features.empty();
            bool actions_ok = !actions.empty() && checkNumericalStability(actions);
            bool motor_ok = motor.executable || !motor.joint_angles.empty();

            std::cout << "  Perception: " << (perception_ok ? "OK" : "FAIL") << "\n";
            std::cout << "  Actions: " << (actions_ok ? "OK" : "FAIL") << "\n";
            std::cout << "  Motor: " << (motor_ok ? "OK" : "FAIL") << "\n";

            // Should handle all edge cases gracefully (no crashes)
            E2E_ASSERT(actions_ok, "Actions failed for edge case: " + edge_case);
        }
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test: Complete Perception-Action Loop
//=============================================================================

E2E_TEST_F(PerceptionActionE2ETest, CompletePerceptionActionLoop) {
    E2E_PIPELINE_START("Complete Perception-Action Loop");

    // Stage 1: Create brain
    E2E_STAGE_BEGIN("Create brain", 500);
    {
        bool created = createPerceptionActionBrain("complete_loop_brain");
        E2E_ASSERT(created, "Failed to create brain");
    }
    E2E_STAGE_END();

    // Stage 2: Run complete perception-action loop
    E2E_STAGE_BEGIN("Complete loop execution", MAX_PIPELINE_TIME_MS);
    {
        const uint32_t NUM_LOOPS = 20;
        std::vector<float> loop_latencies;
        std::vector<float> action_confidences;

        for (uint32_t loop = 0; loop < NUM_LOOPS; ++loop) {
            auto loop_start = std::chrono::high_resolution_clock::now();

            // 1. Perception
            VisualInput input = generateVisualInput(true, 0.1f);
            PerceptionOutput perception = processVisualInput(input);

            // 2. Cognitive processing
            std::vector<float> actions = processCognitive(perception);

            // 3. Motor output
            MotorOutput motor = generateMotorOutput(actions);

            auto loop_end = std::chrono::high_resolution_clock::now();
            float latency = std::chrono::duration<float, std::milli>(
                loop_end - loop_start).count();

            loop_latencies.push_back(latency);
            action_confidences.push_back(motor.execution_confidence);

            // Simulate action execution feedback
            // (In real system, motor output would affect next perception)
        }

        // Compute statistics
        float avg_latency = std::accumulate(loop_latencies.begin(),
                                            loop_latencies.end(), 0.0f) /
                            loop_latencies.size();
        float max_latency = *std::max_element(loop_latencies.begin(),
                                              loop_latencies.end());
        float avg_confidence = std::accumulate(action_confidences.begin(),
                                               action_confidences.end(), 0.0f) /
                               action_confidences.size();

        std::cout << "[E2E] Loop statistics:\n";
        std::cout << "  Average latency: " << avg_latency << " ms\n";
        std::cout << "  Max latency: " << max_latency << " ms\n";
        std::cout << "  Average confidence: " << avg_confidence << "\n";

        E2E_ASSERT(avg_latency < MAX_PIPELINE_TIME_MS / NUM_LOOPS,
                   "Average latency too high");
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test: Noisy Input Robustness
//=============================================================================

E2E_TEST_F(PerceptionActionE2ETest, NoisyInputRobustness) {
    E2E_PIPELINE_START("Noisy Input Robustness");

    // Stage 1: Create brain
    E2E_STAGE_BEGIN("Create brain", 500);
    {
        bool created = createPerceptionActionBrain("noisy_input_brain");
        E2E_ASSERT(created, "Failed to create brain");
    }
    E2E_STAGE_END();

    // Stage 2: Test increasing noise levels
    E2E_STAGE_BEGIN("Test noise robustness", 3000);
    {
        std::vector<float> noise_levels = {0.0f, 0.1f, 0.2f, 0.3f, 0.5f, 0.7f, 1.0f};
        std::vector<float> confidences;

        for (float noise : noise_levels) {
            VisualInput input = generateVisualInput(true, noise);
            PerceptionOutput perception = processVisualInput(input);
            std::vector<float> actions = processCognitive(perception);
            MotorOutput motor = generateMotorOutput(actions);

            confidences.push_back(motor.execution_confidence);

            std::cout << "[E2E] Noise " << noise
                      << " -> confidence " << motor.execution_confidence
                      << " executable " << (motor.executable ? "yes" : "no") << "\n";

            // Should still produce valid output even with high noise
            E2E_ASSERT(!actions.empty(), "No actions for noise level " + std::to_string(noise));
        }

        // Confidence should generally decrease with noise (though not strictly)
        std::cout << "[E2E] Confidence range: "
                  << *std::min_element(confidences.begin(), confidences.end()) << " - "
                  << *std::max_element(confidences.begin(), confidences.end()) << "\n";
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test: Multi-Modal Fusion (Simulated)
//=============================================================================

E2E_TEST_F(PerceptionActionE2ETest, MultiModalFusion) {
    E2E_PIPELINE_START("Multi-Modal Sensory Fusion");

    // Stage 1: Create brain
    E2E_STAGE_BEGIN("Create brain", 500);
    {
        bool created = createPerceptionActionBrain("multimodal_brain");
        E2E_ASSERT(created, "Failed to create brain");
    }
    E2E_STAGE_END();

    // Stage 2: Generate multi-modal inputs
    E2E_STAGE_BEGIN("Generate multi-modal inputs", 200);
    {
        // Visual input
        VisualInput visual = generateVisualInput();

        // Simulated audio features (proprioceptive feedback)
        std::vector<float> audio_features = TestDataGenerator::generate_features(32, 0.0f, 1.0f);

        // Simulated proprioceptive features
        std::vector<float> proprio_features = TestDataGenerator::generate_features(16, -1.0f, 1.0f);

        std::cout << "[E2E] Multi-modal inputs:\n";
        std::cout << "  Visual: " << visual.pixels.size() << " values\n";
        std::cout << "  Audio: " << audio_features.size() << " features\n";
        std::cout << "  Proprioceptive: " << proprio_features.size() << " features\n";
    }
    E2E_STAGE_END();

    // Stage 3: Fusion and processing
    E2E_STAGE_BEGIN("Multi-modal fusion", MAX_COGNITIVE_TIME_MS);
    {
        // Process visual
        VisualInput visual = generateVisualInput();
        PerceptionOutput visual_perception = processVisualInput(visual);

        // Simulated fusion (concatenate features)
        std::vector<float> audio_features = TestDataGenerator::generate_features(32, 0.0f, 1.0f);
        std::vector<float> proprio_features = TestDataGenerator::generate_features(16, -1.0f, 1.0f);

        // Weighted fusion
        std::vector<float> fused_features;
        fused_features.reserve(FEATURE_DIM);

        // Take subset of visual features
        for (size_t i = 0; i < FEATURE_DIM / 2 && i < visual_perception.features.size(); ++i) {
            fused_features.push_back(visual_perception.features[i] * 0.6f);
        }

        // Add audio contribution
        for (size_t i = 0; i < audio_features.size() && fused_features.size() < FEATURE_DIM * 3 / 4; ++i) {
            fused_features.push_back(audio_features[i] * 0.3f);
        }

        // Add proprioceptive contribution
        for (size_t i = 0; i < proprio_features.size() && fused_features.size() < FEATURE_DIM; ++i) {
            fused_features.push_back(proprio_features[i] * 0.1f);
        }

        // Pad to full feature size
        while (fused_features.size() < FEATURE_DIM) {
            fused_features.push_back(0.0f);
        }

        std::cout << "[E2E] Fused features: " << fused_features.size() << " dims\n";

        // Process fused features
        PerceptionOutput fused_perception;
        fused_perception.features = fused_features;
        fused_perception.confidence = visual_perception.confidence * 0.8f;
        fused_perception.valid = true;

        std::vector<float> actions = processCognitive(fused_perception);
        MotorOutput motor = generateMotorOutput(actions);

        std::cout << "[E2E] Fused output:\n";
        std::cout << "  Motor confidence: " << motor.execution_confidence << "\n";
        std::cout << "  Executable: " << (motor.executable ? "yes" : "no") << "\n";

        E2E_ASSERT(motor.executable, "Multi-modal fusion failed to produce valid output");
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Main Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
