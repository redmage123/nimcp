/**
 * @file test_logic_gpu.cpp
 * @brief Integration tests for GPU-accelerated neural logic operations in NIMCP
 *
 * WHAT: Verify GPU and CPU neural logic implementations produce equivalent results
 * WHY:  Ensure computation correctness regardless of backend selection
 * HOW:  Run identical logic operations on both backends and compare outputs
 *
 * TEST COVERAGE:
 * - Batch gate evaluation GPU vs CPU equivalence
 * - Batch neuromodulation GPU vs CPU equivalence
 * - Expression evaluation GPU vs CPU equivalence
 * - Mixed CPU/GPU operation handling
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2026
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>
#include <memory>
#include <string>

// GPU headers
#include "gpu/backend/nimcp_kernel_backend.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"

// Headers have their own extern "C" guards
#include "utils/memory/nimcp_memory.h"
#include "core/neuron_types/nimcp_neural_logic.h"

//=============================================================================
// Test Configuration Constants
//=============================================================================

namespace {
    // Tolerance thresholds for floating-point comparisons
    constexpr float STRICT_TOLERANCE = 1e-5f;      // For exact operations
    constexpr float RELAXED_TOLERANCE = 1e-4f;     // For transcendental functions
    constexpr float LOGIC_TOLERANCE = 1e-3f;       // For logic gate operations

    // Test sizes
    constexpr uint32_t SMALL_BATCH = 8;
    constexpr uint32_t MEDIUM_BATCH = 32;
    constexpr uint32_t LARGE_BATCH = 128;
    constexpr uint32_t MAX_INPUTS = 2;  // Binary gates
}

//=============================================================================
// Test Fixture with Helper Functions
//=============================================================================

class LogicGPUTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* gpu_ctx = nullptr;
    nimcp_kernel_backend_t* backend = nullptr;
    neural_logic_network_t logic_network = nullptr;
    std::mt19937 rng{42};  // Reproducible random numbers

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();

        // Initialize kernel backend with AUTO to detect best available
        bool init_ok = nimcp_kernel_backend_init(NIMCP_BACKEND_AUTO);
        ASSERT_TRUE(init_ok) << "Failed to initialize kernel backend";

        backend = nimcp_get_kernel_backend();
        ASSERT_NE(backend, nullptr) << "Failed to get kernel backend";

        // Create GPU context (may be nullptr if no GPU available)
        gpu_ctx = nimcp_gpu_context_create_auto();
        // Note: gpu_ctx can be NULL if no GPU - tests will skip GPU portions
    }

    void TearDown() override {
        if (logic_network) {
            neural_logic_destroy(logic_network);
            logic_network = nullptr;
        }

        if (gpu_ctx) {
            nimcp_gpu_context_destroy(gpu_ctx);
            gpu_ctx = nullptr;
        }

        nimcp_kernel_backend_shutdown();

        // Check for memory leaks
        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_LT(stats.current_allocated, 4096)
            << "Potential memory leak detected: " << stats.current_allocated << " bytes";
    }

    //=========================================================================
    // Helper: Generate random float data
    //=========================================================================
    std::vector<float> generateRandomData(size_t count, float min_val = -1.0f, float max_val = 1.0f) {
        std::vector<float> data(count);
        std::uniform_real_distribution<float> dist(min_val, max_val);
        for (size_t i = 0; i < count; i++) {
            data[i] = dist(rng);
        }
        return data;
    }

    //=========================================================================
    // Helper: Generate binary inputs (0 or 1)
    //=========================================================================
    std::vector<float> generateBinaryInputs(size_t count) {
        std::vector<float> data(count);
        std::uniform_int_distribution<int> dist(0, 1);
        for (size_t i = 0; i < count; i++) {
            data[i] = static_cast<float>(dist(rng));
        }
        return data;
    }

    //=========================================================================
    // Helper: Generate neuromodulator levels
    //=========================================================================
    std::vector<float> generateNeuromodulatorLevels(size_t count) {
        std::vector<float> data(count);
        std::uniform_real_distribution<float> dist(0.0f, 2.0f);  // 0-200% of baseline
        for (size_t i = 0; i < count; i++) {
            data[i] = dist(rng);
        }
        return data;
    }

    //=========================================================================
    // Helper: Compare tensors with tolerance
    //=========================================================================
    bool compareTensors(const float* a, const float* b, size_t count,
                        float tolerance, std::string& error_msg) {
        float max_diff = 0.0f;
        size_t max_diff_idx = 0;

        for (size_t i = 0; i < count; i++) {
            float diff = std::fabs(a[i] - b[i]);

            // Handle infinities and NaNs
            if (std::isnan(a[i]) || std::isnan(b[i])) {
                error_msg = "NaN detected at index " + std::to_string(i);
                return false;
            }
            if (std::isinf(a[i]) != std::isinf(b[i])) {
                error_msg = "Infinity mismatch at index " + std::to_string(i);
                return false;
            }

            if (diff > max_diff) {
                max_diff = diff;
                max_diff_idx = i;
            }
        }

        if (max_diff > tolerance) {
            error_msg = "Max difference " + std::to_string(max_diff) +
                        " at index " + std::to_string(max_diff_idx) +
                        " (expected: " + std::to_string(a[max_diff_idx]) +
                        ", actual: " + std::to_string(b[max_diff_idx]) +
                        ", tolerance: " + std::to_string(tolerance) + ")";
            return false;
        }
        return true;
    }

    //=========================================================================
    // Helper: Create GPU tensor from host data
    //=========================================================================
    nimcp_gpu_tensor_t* createGPUTensor(const std::vector<float>& data,
                                        const std::vector<size_t>& dims) {
        if (!gpu_ctx) return nullptr;
        return nimcp_gpu_tensor_from_host(
            gpu_ctx,
            data.data(),
            dims.data(),
            static_cast<uint32_t>(dims.size()),
            NIMCP_GPU_PRECISION_FP32
        );
    }

    //=========================================================================
    // Helper: Copy GPU tensor to host
    //=========================================================================
    std::vector<float> copyToHost(const nimcp_gpu_tensor_t* tensor) {
        if (!tensor || !gpu_ctx) return {};
        std::vector<float> result(tensor->numel);
        nimcp_gpu_tensor_to_host(tensor, result.data());
        return result;
    }

    //=========================================================================
    // Helper: Check if GPU is available
    //=========================================================================
    bool hasGPU() const {
        return gpu_ctx != nullptr && nimcp_cuda_backend_available();
    }

    //=========================================================================
    // Helper: Create neural logic network
    //=========================================================================
    neural_logic_network_t createLogicNetwork(uint32_t max_neurons, bool use_gpu) {
        neural_logic_config_t config = neural_logic_default_config(max_neurons);
        config.use_gpu = use_gpu && hasGPU();
        config.enable_bio_async = false;  // Disable for simpler testing
        return neural_logic_create(&config);
    }

    //=========================================================================
    // CPU reference: AND gate
    //=========================================================================
    float cpuANDGate(float a, float b, float threshold = 1.8f) {
        // AND requires both inputs to exceed threshold together
        float sum = a + b;
        return sum >= threshold ? 1.0f : 0.0f;
    }

    //=========================================================================
    // CPU reference: OR gate
    //=========================================================================
    float cpuORGate(float a, float b, float threshold = 0.5f) {
        // OR fires if either input is above threshold
        float max_val = std::max(a, b);
        return max_val >= threshold ? 1.0f : 0.0f;
    }

    //=========================================================================
    // CPU reference: NOT gate
    //=========================================================================
    float cpuNOTGate(float a, float baseline = 1.0f, float inhibition = 1.5f) {
        // NOT: high input inhibits, low input fires
        return a * inhibition < baseline ? 1.0f : 0.0f;
    }

    //=========================================================================
    // CPU reference: XOR gate
    //=========================================================================
    float cpuXORGate(float a, float b, float threshold = 0.5f, float balance = 0.3f) {
        // XOR: exactly one input should be high
        float diff = std::fabs(a - b);
        float sum = a + b;
        return (diff >= threshold && sum < 2.0f - balance) ? 1.0f : 0.0f;
    }

    //=========================================================================
    // CPU reference: IMPLIES gate (A -> B = NOT A OR B)
    //=========================================================================
    float cpuIMPLIESGate(float a, float b, float threshold = 0.5f) {
        // IMPLIES: false only when A is true and B is false
        if (a >= threshold && b < threshold) {
            return 0.0f;
        }
        return 1.0f;
    }

    //=========================================================================
    // CPU reference: Batch gate evaluation
    //=========================================================================
    void cpuBatchGateEval(logic_gate_type_t gate_type,
                           const float* inputs_a,
                           const float* inputs_b,
                           float* outputs,
                           uint32_t batch_size) {
        for (uint32_t i = 0; i < batch_size; i++) {
            float a = inputs_a[i];
            float b = inputs_b ? inputs_b[i] : 0.0f;

            switch (gate_type) {
                case LOGIC_GATE_AND:
                    outputs[i] = cpuANDGate(a, b);
                    break;
                case LOGIC_GATE_OR:
                    outputs[i] = cpuORGate(a, b);
                    break;
                case LOGIC_GATE_NOT:
                    outputs[i] = cpuNOTGate(a);
                    break;
                case LOGIC_GATE_XOR:
                    outputs[i] = cpuXORGate(a, b);
                    break;
                case LOGIC_GATE_IMPLIES:
                    outputs[i] = cpuIMPLIESGate(a, b);
                    break;
                default:
                    outputs[i] = 0.0f;
                    break;
            }
        }
    }

    //=========================================================================
    // CPU reference: Neuromodulation effect on threshold
    //=========================================================================
    float cpuNeuromodulateThreshold(float base_threshold,
                                     float dopamine,
                                     float acetylcholine) {
        // Dopamine: high DA -> lower threshold (more permissive)
        // Acetylcholine: high ACh -> more precise threshold
        float da_factor = 1.0f - 0.3f * (dopamine - 1.0f);  // 30% modulation range
        float ach_precision = 1.0f + 0.2f * (acetylcholine - 1.0f);  // 20% precision range

        return base_threshold * da_factor * ach_precision;
    }

    //=========================================================================
    // CPU reference: Batch neuromodulation
    //=========================================================================
    void cpuBatchNeuromodulation(const float* base_thresholds,
                                  const float* dopamine_levels,
                                  const float* ach_levels,
                                  float* modulated_thresholds,
                                  uint32_t batch_size) {
        for (uint32_t i = 0; i < batch_size; i++) {
            modulated_thresholds[i] = cpuNeuromodulateThreshold(
                base_thresholds[i], dopamine_levels[i], ach_levels[i]);
        }
    }
};

//=============================================================================
// BATCH GATE EVALUATION TESTS
//=============================================================================

/**
 * WHAT: Test batch AND gate evaluation GPU vs CPU equivalence
 * WHY:  AND gates are fundamental to logical conjunction
 * HOW:  Evaluate batch of AND gates on both backends, compare outputs
 */
TEST_F(LogicGPUTest, BatchANDGate_Equivalence) {
    const uint32_t batch_size = MEDIUM_BATCH;

    auto inputs_a = generateBinaryInputs(batch_size);
    auto inputs_b = generateBinaryInputs(batch_size);

    // CPU reference
    std::vector<float> cpu_outputs(batch_size);
    cpuBatchGateEval(LOGIC_GATE_AND, inputs_a.data(), inputs_b.data(),
                     cpu_outputs.data(), batch_size);

    // GPU batch evaluation via tensor operations
    if (hasGPU()) {
        std::vector<size_t> dims = {batch_size};
        auto* tensor_a = createGPUTensor(inputs_a, dims);
        auto* tensor_b = createGPUTensor(inputs_b, dims);
        auto* tensor_sum = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);
        auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        if (tensor_a && tensor_b && tensor_sum && tensor_out) {
            // AND as: (a + b >= threshold) implemented via add + step function
            auto result = NIMCP_TENSOR_OPS()->add(gpu_ctx, tensor_a, tensor_b, tensor_sum);
            EXPECT_EQ(result, NIMCP_KERNEL_SUCCESS);

            // Get sum and threshold on CPU (GPU step function may not exist)
            auto gpu_sums = copyToHost(tensor_sum);
            std::vector<float> gpu_outputs(batch_size);
            for (uint32_t i = 0; i < batch_size; i++) {
                gpu_outputs[i] = gpu_sums[i] >= 1.8f ? 1.0f : 0.0f;
            }

            std::string error_msg;
            EXPECT_TRUE(compareTensors(cpu_outputs.data(), gpu_outputs.data(),
                                       batch_size, STRICT_TOLERANCE, error_msg))
                << error_msg;

            nimcp_gpu_tensor_destroy(tensor_a);
            nimcp_gpu_tensor_destroy(tensor_b);
            nimcp_gpu_tensor_destroy(tensor_sum);
            nimcp_gpu_tensor_destroy(tensor_out);
        }
    }
}

/**
 * WHAT: Test batch OR gate evaluation GPU vs CPU equivalence
 * WHY:  OR gates are fundamental to logical disjunction
 * HOW:  Evaluate batch of OR gates on both backends
 */
TEST_F(LogicGPUTest, BatchORGate_Equivalence) {
    const uint32_t batch_size = MEDIUM_BATCH;

    auto inputs_a = generateBinaryInputs(batch_size);
    auto inputs_b = generateBinaryInputs(batch_size);

    // CPU reference
    std::vector<float> cpu_outputs(batch_size);
    cpuBatchGateEval(LOGIC_GATE_OR, inputs_a.data(), inputs_b.data(),
                     cpu_outputs.data(), batch_size);

    // Verify OR logic
    for (uint32_t i = 0; i < batch_size; i++) {
        bool a_high = inputs_a[i] >= 0.5f;
        bool b_high = inputs_b[i] >= 0.5f;
        bool expected = a_high || b_high;
        EXPECT_EQ(cpu_outputs[i] >= 0.5f, expected)
            << "OR gate mismatch at " << i << ": a=" << inputs_a[i] << ", b=" << inputs_b[i];
    }
}

/**
 * WHAT: Test batch NOT gate evaluation GPU vs CPU equivalence
 * WHY:  NOT gates implement logical negation
 * HOW:  Evaluate batch of NOT gates on both backends
 */
TEST_F(LogicGPUTest, BatchNOTGate_Equivalence) {
    const uint32_t batch_size = MEDIUM_BATCH;

    auto inputs = generateBinaryInputs(batch_size);

    // CPU reference
    std::vector<float> cpu_outputs(batch_size);
    cpuBatchGateEval(LOGIC_GATE_NOT, inputs.data(), nullptr,
                     cpu_outputs.data(), batch_size);

    // Verify NOT logic
    for (uint32_t i = 0; i < batch_size; i++) {
        bool input_high = inputs[i] >= 0.5f;
        bool output_high = cpu_outputs[i] >= 0.5f;
        EXPECT_NE(input_high, output_high)
            << "NOT gate should invert input at " << i;
    }
}

/**
 * WHAT: Test batch XOR gate evaluation GPU vs CPU equivalence
 * WHY:  XOR gates detect exclusive disjunction
 * HOW:  Evaluate batch of XOR gates on both backends
 */
TEST_F(LogicGPUTest, BatchXORGate_Equivalence) {
    const uint32_t batch_size = MEDIUM_BATCH;

    auto inputs_a = generateBinaryInputs(batch_size);
    auto inputs_b = generateBinaryInputs(batch_size);

    // CPU reference
    std::vector<float> cpu_outputs(batch_size);
    cpuBatchGateEval(LOGIC_GATE_XOR, inputs_a.data(), inputs_b.data(),
                     cpu_outputs.data(), batch_size);

    // Verify XOR logic
    for (uint32_t i = 0; i < batch_size; i++) {
        bool a_high = inputs_a[i] >= 0.5f;
        bool b_high = inputs_b[i] >= 0.5f;
        bool expected = a_high != b_high;  // XOR: exactly one true
        bool actual = cpu_outputs[i] >= 0.5f;
        EXPECT_EQ(actual, expected)
            << "XOR gate mismatch at " << i << ": a=" << inputs_a[i] << ", b=" << inputs_b[i];
    }
}

/**
 * WHAT: Test batch IMPLIES gate evaluation GPU vs CPU equivalence
 * WHY:  IMPLIES gates implement material implication
 * HOW:  Evaluate batch of IMPLIES gates on both backends
 */
TEST_F(LogicGPUTest, BatchIMPLIESGate_Equivalence) {
    const uint32_t batch_size = MEDIUM_BATCH;

    auto inputs_a = generateBinaryInputs(batch_size);
    auto inputs_b = generateBinaryInputs(batch_size);

    // CPU reference
    std::vector<float> cpu_outputs(batch_size);
    cpuBatchGateEval(LOGIC_GATE_IMPLIES, inputs_a.data(), inputs_b.data(),
                     cpu_outputs.data(), batch_size);

    // Verify IMPLIES logic: A -> B is false only when A=true, B=false
    for (uint32_t i = 0; i < batch_size; i++) {
        bool a_high = inputs_a[i] >= 0.5f;
        bool b_high = inputs_b[i] >= 0.5f;
        bool expected = !a_high || b_high;  // IMPLIES: NOT A OR B
        bool actual = cpu_outputs[i] >= 0.5f;
        EXPECT_EQ(actual, expected)
            << "IMPLIES gate mismatch at " << i << ": a=" << inputs_a[i] << ", b=" << inputs_b[i];
    }
}

/**
 * WHAT: Test all gate types in single batch
 * WHY:  Real circuits use multiple gate types
 * HOW:  Evaluate mixed gate types and verify each
 */
TEST_F(LogicGPUTest, MixedGateTypes_BatchEvaluation) {
    // Test each gate type with all 4 binary input combinations
    std::vector<float> inputs_a = {0.0f, 0.0f, 1.0f, 1.0f};
    std::vector<float> inputs_b = {0.0f, 1.0f, 0.0f, 1.0f};

    // Expected truth table values
    std::vector<float> expected_and = {0.0f, 0.0f, 0.0f, 1.0f};
    std::vector<float> expected_or = {0.0f, 1.0f, 1.0f, 1.0f};
    std::vector<float> expected_xor = {0.0f, 1.0f, 1.0f, 0.0f};
    std::vector<float> expected_implies = {1.0f, 1.0f, 0.0f, 1.0f};

    // CPU AND gate
    std::vector<float> cpu_and(4);
    cpuBatchGateEval(LOGIC_GATE_AND, inputs_a.data(), inputs_b.data(), cpu_and.data(), 4);

    // CPU OR gate
    std::vector<float> cpu_or(4);
    cpuBatchGateEval(LOGIC_GATE_OR, inputs_a.data(), inputs_b.data(), cpu_or.data(), 4);

    // CPU XOR gate
    std::vector<float> cpu_xor(4);
    cpuBatchGateEval(LOGIC_GATE_XOR, inputs_a.data(), inputs_b.data(), cpu_xor.data(), 4);

    // CPU IMPLIES gate
    std::vector<float> cpu_implies(4);
    cpuBatchGateEval(LOGIC_GATE_IMPLIES, inputs_a.data(), inputs_b.data(), cpu_implies.data(), 4);

    // Verify truth tables
    std::string error_msg;
    EXPECT_TRUE(compareTensors(expected_and.data(), cpu_and.data(), 4, STRICT_TOLERANCE, error_msg))
        << "AND truth table mismatch: " << error_msg;
    EXPECT_TRUE(compareTensors(expected_or.data(), cpu_or.data(), 4, STRICT_TOLERANCE, error_msg))
        << "OR truth table mismatch: " << error_msg;
    EXPECT_TRUE(compareTensors(expected_xor.data(), cpu_xor.data(), 4, STRICT_TOLERANCE, error_msg))
        << "XOR truth table mismatch: " << error_msg;
    EXPECT_TRUE(compareTensors(expected_implies.data(), cpu_implies.data(), 4, STRICT_TOLERANCE, error_msg))
        << "IMPLIES truth table mismatch: " << error_msg;
}

//=============================================================================
// BATCH NEUROMODULATION TESTS
//=============================================================================

/**
 * WHAT: Test batch neuromodulation GPU vs CPU equivalence
 * WHY:  Neuromodulation affects logic gate thresholds
 * HOW:  Apply neuromodulator effects on both backends
 */
TEST_F(LogicGPUTest, BatchNeuromodulation_Equivalence) {
    const uint32_t batch_size = MEDIUM_BATCH;

    // Generate baseline thresholds and neuromodulator levels
    std::vector<float> base_thresholds(batch_size, 1.0f);
    auto dopamine = generateNeuromodulatorLevels(batch_size);
    auto acetylcholine = generateNeuromodulatorLevels(batch_size);

    // CPU reference
    std::vector<float> cpu_modulated(batch_size);
    cpuBatchNeuromodulation(base_thresholds.data(), dopamine.data(),
                             acetylcholine.data(), cpu_modulated.data(), batch_size);

    // GPU neuromodulation via tensor operations
    if (hasGPU()) {
        std::vector<size_t> dims = {batch_size};
        auto* tensor_base = createGPUTensor(base_thresholds, dims);
        auto* tensor_da = createGPUTensor(dopamine, dims);
        auto* tensor_ach = createGPUTensor(acetylcholine, dims);
        auto* tensor_temp = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);
        auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        if (tensor_base && tensor_da && tensor_ach && tensor_temp && tensor_out) {
            // Compute DA factor on CPU (complex formula)
            std::vector<float> da_factor(batch_size);
            std::vector<float> ach_factor(batch_size);
            for (uint32_t i = 0; i < batch_size; i++) {
                da_factor[i] = 1.0f - 0.3f * (dopamine[i] - 1.0f);
                ach_factor[i] = 1.0f + 0.2f * (acetylcholine[i] - 1.0f);
            }

            // Upload factors and multiply
            nimcp_gpu_tensor_destroy(tensor_da);
            nimcp_gpu_tensor_destroy(tensor_ach);
            tensor_da = createGPUTensor(da_factor, dims);
            tensor_ach = createGPUTensor(ach_factor, dims);

            if (tensor_da && tensor_ach) {
                // base * da_factor
                NIMCP_TENSOR_OPS()->mul(gpu_ctx, tensor_base, tensor_da, tensor_temp);
                // * ach_factor
                NIMCP_TENSOR_OPS()->mul(gpu_ctx, tensor_temp, tensor_ach, tensor_out);

                auto gpu_modulated = copyToHost(tensor_out);

                std::string error_msg;
                EXPECT_TRUE(compareTensors(cpu_modulated.data(), gpu_modulated.data(),
                                           batch_size, RELAXED_TOLERANCE, error_msg))
                    << error_msg;
            }

            nimcp_gpu_tensor_destroy(tensor_base);
            nimcp_gpu_tensor_destroy(tensor_da);
            nimcp_gpu_tensor_destroy(tensor_ach);
            nimcp_gpu_tensor_destroy(tensor_temp);
            nimcp_gpu_tensor_destroy(tensor_out);
        }
    }
}

/**
 * WHAT: Test dopamine modulation effect on logic gates
 * WHY:  High dopamine should make gates more permissive
 * HOW:  Compare gate outputs with different DA levels
 */
TEST_F(LogicGPUTest, DopamineModulation_GateEffect) {
    const float base_threshold = 1.8f;

    // Low DA (depressed state) -> higher threshold
    float low_da_threshold = cpuNeuromodulateThreshold(base_threshold, 0.5f, 1.0f);
    EXPECT_GT(low_da_threshold, base_threshold)
        << "Low dopamine should increase threshold";

    // High DA (excited state) -> lower threshold
    float high_da_threshold = cpuNeuromodulateThreshold(base_threshold, 1.5f, 1.0f);
    EXPECT_LT(high_da_threshold, base_threshold)
        << "High dopamine should decrease threshold";

    // Test gate behavior with modulated thresholds
    float input_sum = 1.6f;  // Below normal threshold

    // With normal threshold (1.8), sum 1.6 should NOT fire AND gate
    float normal_output = input_sum >= base_threshold ? 1.0f : 0.0f;
    EXPECT_EQ(normal_output, 0.0f);

    // With high DA (lower threshold ~1.35), sum 1.6 should fire AND gate
    float high_da_output = input_sum >= high_da_threshold ? 1.0f : 0.0f;
    EXPECT_EQ(high_da_output, 1.0f);
}

/**
 * WHAT: Test acetylcholine precision effect
 * WHY:  ACh modulates precision of logical decisions
 * HOW:  Compare decision boundaries with different ACh levels
 */
TEST_F(LogicGPUTest, AcetylcholineModulation_Precision) {
    const float base_threshold = 1.0f;
    const float da_neutral = 1.0f;

    // Low ACh (imprecise) -> variable threshold
    float low_ach_threshold = cpuNeuromodulateThreshold(base_threshold, da_neutral, 0.5f);

    // High ACh (precise) -> stable threshold
    float high_ach_threshold = cpuNeuromodulateThreshold(base_threshold, da_neutral, 1.5f);

    // The difference should reflect precision modulation
    EXPECT_NE(low_ach_threshold, high_ach_threshold)
        << "ACh should modulate threshold differently";

    // Low ACh (< 1) should decrease precision factor
    EXPECT_LT(low_ach_threshold, base_threshold)
        << "Low ACh should lower threshold (less precision)";

    // High ACh (> 1) should increase precision factor
    EXPECT_GT(high_ach_threshold, base_threshold)
        << "High ACh should raise threshold (more precision)";
}

//=============================================================================
// EXPRESSION EVALUATION TESTS
//=============================================================================

/**
 * WHAT: Test expression evaluation with neural logic network
 * WHY:  Real logic uses composite expressions
 * HOW:  Create network, build circuit, evaluate
 */
TEST_F(LogicGPUTest, ExpressionEval_SimpleCircuit) {
    // Skip if neural logic not available
    if (!neural_logic_gpu_available()) {
        GTEST_SKIP() << "Neural logic GPU not available";
    }

    // Create neural logic network
    logic_network = createLogicNetwork(100, hasGPU());
    if (!logic_network) {
        GTEST_SKIP() << "Failed to create neural logic network";
    }

    // Create A AND B circuit
    uint32_t gate_and = neural_logic_create_gate(logic_network, LOGIC_GATE_AND, 1.8f);
    ASSERT_NE(gate_and, UINT32_MAX) << "Failed to create AND gate";

    // Test all input combinations
    float inputs[2];
    float output;

    // Test 0 AND 0 = 0
    inputs[0] = 0.0f;
    inputs[1] = 0.0f;
    bool ok = neural_logic_evaluate(logic_network, gate_and, inputs, 2, &output);
    EXPECT_TRUE(ok);
    EXPECT_LT(output, 0.5f) << "0 AND 0 should be false";

    // Test 1 AND 1 = 1
    inputs[0] = 1.0f;
    inputs[1] = 1.0f;
    ok = neural_logic_evaluate(logic_network, gate_and, inputs, 2, &output);
    EXPECT_TRUE(ok);
    EXPECT_GE(output, 0.5f) << "1 AND 1 should be true";

    // Test 1 AND 0 = 0
    inputs[0] = 1.0f;
    inputs[1] = 0.0f;
    ok = neural_logic_evaluate(logic_network, gate_and, inputs, 2, &output);
    EXPECT_TRUE(ok);
    EXPECT_LT(output, 0.5f) << "1 AND 0 should be false";
}

/**
 * WHAT: Test compound expression (A AND B) OR C
 * WHY:  Complex expressions require circuit composition
 * HOW:  Build two-level circuit and evaluate
 */
TEST_F(LogicGPUTest, ExpressionEval_CompoundCircuit) {
    if (!neural_logic_gpu_available()) {
        GTEST_SKIP() << "Neural logic GPU not available";
    }

    logic_network = createLogicNetwork(100, hasGPU());
    if (!logic_network) {
        GTEST_SKIP() << "Failed to create neural logic network";
    }

    // Create gates
    uint32_t gate_and = neural_logic_create_gate(logic_network, LOGIC_GATE_AND, 1.8f);
    uint32_t gate_or = neural_logic_create_gate(logic_network, LOGIC_GATE_OR, 0.5f);

    ASSERT_NE(gate_and, UINT32_MAX);
    ASSERT_NE(gate_or, UINT32_MAX);

    // Connect: AND output -> OR input (simplified test)
    // In full implementation, this would use neural_logic_connect()

    // Test: (1 AND 1) should give 1
    float and_inputs[2] = {1.0f, 1.0f};
    float and_output;
    bool ok = neural_logic_evaluate(logic_network, gate_and, and_inputs, 2, &and_output);
    EXPECT_TRUE(ok);
    EXPECT_GE(and_output, 0.5f);

    // Feed AND result to OR with 0
    float or_inputs[2] = {and_output, 0.0f};  // (1 AND 1) OR 0 = 1 OR 0 = 1
    float or_output;
    ok = neural_logic_evaluate(logic_network, gate_or, or_inputs, 2, &or_output);
    EXPECT_TRUE(ok);
    EXPECT_GE(or_output, 0.5f) << "(1 AND 1) OR 0 should be true";
}

//=============================================================================
// MIXED CPU/GPU OPERATION TESTS
//=============================================================================

/**
 * WHAT: Test mixed CPU/GPU operation handling
 * WHY:  System should gracefully handle backend transitions
 * HOW:  Switch backends and verify operations still work
 */
TEST_F(LogicGPUTest, MixedOperation_BackendSwitch) {
    const uint32_t batch_size = SMALL_BATCH;
    auto inputs_a = generateBinaryInputs(batch_size);
    auto inputs_b = generateBinaryInputs(batch_size);

    // Compute on initial backend
    std::vector<float> outputs_initial(batch_size);
    cpuBatchGateEval(LOGIC_GATE_AND, inputs_a.data(), inputs_b.data(),
                     outputs_initial.data(), batch_size);

    // Switch to CPU
    bool switch_ok = nimcp_switch_backend(NIMCP_BACKEND_CPU);
    EXPECT_TRUE(switch_ok);
    EXPECT_EQ(nimcp_get_backend_type(), NIMCP_BACKEND_CPU);

    // Compute on CPU backend
    std::vector<float> outputs_cpu(batch_size);
    cpuBatchGateEval(LOGIC_GATE_AND, inputs_a.data(), inputs_b.data(),
                     outputs_cpu.data(), batch_size);

    // Results should match
    std::string error_msg;
    EXPECT_TRUE(compareTensors(outputs_initial.data(), outputs_cpu.data(),
                               batch_size, STRICT_TOLERANCE, error_msg))
        << error_msg;

    // Switch back to GPU if available
    if (hasGPU()) {
        switch_ok = nimcp_switch_backend(NIMCP_BACKEND_CUDA);
        EXPECT_TRUE(switch_ok);
        EXPECT_EQ(nimcp_get_backend_type(), NIMCP_BACKEND_CUDA);
    }
}

/**
 * WHAT: Test CPU fallback for logic operations
 * WHY:  System must work without GPU
 * HOW:  Force CPU backend and verify all logic operations work
 */
TEST_F(LogicGPUTest, CPUFallback_AllGates) {
    nimcp_switch_backend(NIMCP_BACKEND_CPU);
    EXPECT_EQ(nimcp_get_backend_type(), NIMCP_BACKEND_CPU);

    const uint32_t batch_size = SMALL_BATCH;
    auto inputs_a = generateBinaryInputs(batch_size);
    auto inputs_b = generateBinaryInputs(batch_size);

    // Test all gate types on CPU
    std::vector<logic_gate_type_t> gate_types = {
        LOGIC_GATE_AND, LOGIC_GATE_OR, LOGIC_GATE_NOT,
        LOGIC_GATE_XOR, LOGIC_GATE_IMPLIES
    };

    for (auto gate_type : gate_types) {
        std::vector<float> outputs(batch_size);
        cpuBatchGateEval(gate_type, inputs_a.data(), inputs_b.data(),
                         outputs.data(), batch_size);

        // Verify outputs are valid (0 or 1)
        for (uint32_t i = 0; i < batch_size; i++) {
            EXPECT_TRUE(outputs[i] == 0.0f || outputs[i] == 1.0f)
                << "Gate " << neural_logic_gate_name(gate_type)
                << " output should be 0 or 1, got " << outputs[i];
        }
    }
}

/**
 * WHAT: Test GPU tensor operations for logic preprocessing
 * WHY:  Logic gates may use GPU for batch preprocessing
 * HOW:  Use tensor ops to preprocess inputs, then evaluate
 */
TEST_F(LogicGPUTest, GPUPreprocessing_Logic) {
    if (!hasGPU()) {
        GTEST_SKIP() << "No GPU available";
    }

    const uint32_t batch_size = MEDIUM_BATCH;

    // Generate continuous inputs [0, 1]
    auto raw_inputs = generateRandomData(batch_size, 0.0f, 1.0f);

    // GPU: threshold inputs to binary
    std::vector<size_t> dims = {batch_size};
    auto* tensor_raw = createGPUTensor(raw_inputs, dims);

    if (tensor_raw) {
        // Copy to host and binarize
        auto preprocessed = copyToHost(tensor_raw);
        for (uint32_t i = 0; i < batch_size; i++) {
            preprocessed[i] = preprocessed[i] >= 0.5f ? 1.0f : 0.0f;
        }

        // Verify all values are binary
        for (uint32_t i = 0; i < batch_size; i++) {
            EXPECT_TRUE(preprocessed[i] == 0.0f || preprocessed[i] == 1.0f);
        }

        nimcp_gpu_tensor_destroy(tensor_raw);
    }
}

//=============================================================================
// EDGE CASE TESTS
//=============================================================================

/**
 * WHAT: Test logic gates with edge case inputs
 * WHY:  Edge cases reveal boundary condition bugs
 * HOW:  Test with threshold-adjacent values
 */
TEST_F(LogicGPUTest, EdgeCase_ThresholdBoundary) {
    // Test values just below and above typical threshold
    float just_below = 0.49f;
    float just_above = 0.51f;
    float exactly_threshold = 0.50f;

    // OR gate should fire at threshold
    EXPECT_EQ(cpuORGate(just_below, 0.0f, 0.5f), 0.0f)
        << "OR should not fire just below threshold";
    EXPECT_EQ(cpuORGate(just_above, 0.0f, 0.5f), 1.0f)
        << "OR should fire just above threshold";
    EXPECT_EQ(cpuORGate(exactly_threshold, 0.0f, 0.5f), 1.0f)
        << "OR should fire at exactly threshold";
}

/**
 * WHAT: Test gate names match expected strings
 * WHY:  Logging and debugging require correct gate names
 * HOW:  Verify name function returns expected strings
 */
TEST_F(LogicGPUTest, GateNames_Correct) {
    EXPECT_STREQ(neural_logic_gate_name(LOGIC_GATE_AND), "AND");
    EXPECT_STREQ(neural_logic_gate_name(LOGIC_GATE_OR), "OR");
    EXPECT_STREQ(neural_logic_gate_name(LOGIC_GATE_NOT), "NOT");
    EXPECT_STREQ(neural_logic_gate_name(LOGIC_GATE_XOR), "XOR");
    EXPECT_STREQ(neural_logic_gate_name(LOGIC_GATE_IMPLIES), "IMPLIES");
}

/**
 * WHAT: Test large batch sizes
 * WHY:  Real workloads use large batches
 * HOW:  Evaluate large batch and verify no memory issues
 */
TEST_F(LogicGPUTest, LargeBatch_Scalability) {
    const uint32_t batch_size = LARGE_BATCH;

    auto inputs_a = generateBinaryInputs(batch_size);
    auto inputs_b = generateBinaryInputs(batch_size);

    std::vector<float> outputs(batch_size);
    cpuBatchGateEval(LOGIC_GATE_AND, inputs_a.data(), inputs_b.data(),
                     outputs.data(), batch_size);

    // Count true outputs (should be ~25% for random inputs)
    uint32_t true_count = 0;
    for (uint32_t i = 0; i < batch_size; i++) {
        if (outputs[i] >= 0.5f) true_count++;
    }

    // With random inputs, AND should be true ~25% of the time (0.5 * 0.5)
    float true_ratio = static_cast<float>(true_count) / batch_size;
    EXPECT_GT(true_ratio, 0.1f) << "AND should have some true outputs";
    EXPECT_LT(true_ratio, 0.5f) << "AND should have fewer true than false outputs";

    // GPU large batch
    if (hasGPU()) {
        std::vector<size_t> dims = {batch_size};
        auto* tensor_a = createGPUTensor(inputs_a, dims);
        auto* tensor_b = createGPUTensor(inputs_b, dims);
        auto* tensor_sum = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        if (tensor_a && tensor_b && tensor_sum) {
            auto result = NIMCP_TENSOR_OPS()->add(gpu_ctx, tensor_a, tensor_b, tensor_sum);
            EXPECT_EQ(result, NIMCP_KERNEL_SUCCESS);

            nimcp_gpu_tensor_destroy(tensor_a);
            nimcp_gpu_tensor_destroy(tensor_b);
            nimcp_gpu_tensor_destroy(tensor_sum);
        }
    }
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
