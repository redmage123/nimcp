/**
 * @file e2e_test_framework.h
 * @brief End-to-End Test Framework for NIMCP
 *
 * WHAT: Comprehensive E2E testing infrastructure for complete pipelines
 * WHY:  Unit tests verify components; E2E tests verify complete workflows
 * HOW:  Pipeline stage tracking, timing assertions, result aggregation
 *
 * FRAMEWORK FEATURES:
 * - Pipeline stage tracking with automatic timing
 * - Configurable timeout assertions per stage
 * - Result aggregation across multi-stage pipelines
 * - Memory leak detection for entire pipelines
 * - Performance regression detection
 *
 * USAGE EXAMPLE:
 * ```cpp
 * E2E_TEST(BrainPipeline, TrainAndInfer) {
 *     E2E_PIPELINE_START("Brain Lifecycle");
 *
 *     E2E_STAGE_BEGIN("Create brain", 10);  // 10ms timeout
 *     nimcp_brain_t brain = nimcp_brain_create("test", NIMCP_BRAIN_SMALL, ...);
 *     E2E_ASSERT_NOT_NULL(brain, "Brain creation failed");
 *     E2E_STAGE_END();
 *
 *     E2E_STAGE_BEGIN("Training", 5000);  // 5s timeout
 *     // ... training code ...
 *     E2E_STAGE_END();
 *
 *     E2E_PIPELINE_END();
 * }
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 * @version 1.0.0
 */

#ifndef E2E_TEST_FRAMEWORK_H
#define E2E_TEST_FRAMEWORK_H

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <string>
#include <memory>
#include <map>
#include <limits>
#include <numeric>

extern "C" {
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
#include "nimcp.h"
}

//=============================================================================
// E2E Test Macros
//=============================================================================

/**
 * @brief Define an E2E test case
 *
 * Similar to TEST() but with E2E-specific setup/teardown
 */
#define E2E_TEST(test_suite, test_name) \
    TEST(test_suite, test_name)

/**
 * @brief Assert condition is true in E2E context
 */
#define E2E_ASSERT(condition, message) \
    ASSERT_TRUE(condition) << "E2E Assertion Failed: " << message

/**
 * @brief Assert pointer is not NULL
 */
#define E2E_ASSERT_NOT_NULL(ptr, message) \
    ASSERT_NE(ptr, nullptr) << "E2E Null Pointer: " << message

/**
 * @brief Assert NIMCP status is success
 */
#define E2E_ASSERT_SUCCESS(status, message) \
    ASSERT_EQ(status, NIMCP_OK) << "E2E Operation Failed: " << message \
        << " (error: " << nimcp_error_to_string(status) << ")"

/**
 * @brief Assert pipeline completes within timeout
 */
#define E2E_ASSERT_PIPELINE_SUCCESS(pipeline_ref) \
    do { \
        const nimcp::e2e::PipelineTracker& _p = (pipeline_ref); \
        ASSERT_TRUE(_p.is_successful()) << "Pipeline failed: " << _p.get_failure_reason(); \
    } while (0)

/**
 * @brief Assert stage completes within timeout
 */
#define E2E_ASSERT_STAGE_TIMEOUT(elapsed_ms, timeout_ms, stage_name) \
    ASSERT_LE(elapsed_ms, timeout_ms) << "Stage '" << stage_name \
        << "' exceeded timeout (" << elapsed_ms << "ms > " << timeout_ms << "ms)"

//=============================================================================
// Pipeline Stage Tracking
//=============================================================================

namespace nimcp {
namespace e2e {

/**
 * @brief Pipeline stage result
 */
struct StageResult {
    std::string name;
    uint64_t duration_us;
    uint64_t timeout_us;
    bool completed;
    bool timed_out;
    std::string error_message;

    StageResult()
        : duration_us(0), timeout_us(0), completed(false), timed_out(false) {}

    StageResult(const std::string& n, uint64_t dur, uint64_t timeout)
        : name(n), duration_us(dur), timeout_us(timeout),
          completed(true), timed_out(dur > timeout) {}
};

/**
 * @brief Pipeline execution tracker
 *
 * Tracks execution of multi-stage pipelines with timing and result aggregation
 */
class PipelineTracker {
public:
    explicit PipelineTracker(const std::string& name);
    ~PipelineTracker();

    // Stage management
    void begin_stage(const std::string& name, uint64_t timeout_ms);
    void end_stage();
    void fail_stage(const std::string& error);

    // Status queries
    bool is_successful() const;
    std::string get_failure_reason() const;
    const std::vector<StageResult>& get_stages() const { return stages_; }

    // Statistics
    uint64_t get_total_duration_ms() const;
    size_t get_stage_count() const { return stages_.size(); }
    void print_summary() const;

private:
    std::string pipeline_name_;
    std::vector<StageResult> stages_;
    std::string current_stage_name_;
    std::chrono::high_resolution_clock::time_point current_stage_start_;
    uint64_t current_stage_timeout_us_;
    std::chrono::high_resolution_clock::time_point pipeline_start_;
    bool failed_;
    std::string failure_reason_;
};

/**
 * @brief RAII wrapper for pipeline stages
 *
 * Automatically calls end_stage() on scope exit
 */
class StageGuard {
public:
    StageGuard(PipelineTracker& tracker, const std::string& name, uint64_t timeout_ms)
        : tracker_(tracker) {
        tracker_.begin_stage(name, timeout_ms);
    }

    ~StageGuard() {
        tracker_.end_stage();
    }

    StageGuard(const StageGuard&) = delete;
    StageGuard& operator=(const StageGuard&) = delete;

private:
    PipelineTracker& tracker_;
};

//=============================================================================
// Timing Utilities
//=============================================================================

/**
 * @brief High-precision timer for performance measurements
 */
class Timer {
public:
    Timer();

    void start();
    void stop();
    void reset();

    uint64_t elapsed_us() const;
    uint64_t elapsed_ms() const;
    double elapsed_sec() const;

private:
    std::chrono::high_resolution_clock::time_point start_time_;
    std::chrono::high_resolution_clock::time_point end_time_;
    bool running_;
};

//=============================================================================
// Memory Tracking for E2E Tests
//=============================================================================

/**
 * @brief Memory leak detector for E2E pipelines
 *
 * Captures memory stats before/after pipeline to detect leaks
 */
class MemoryLeakDetector {
public:
    MemoryLeakDetector();
    ~MemoryLeakDetector();

    void checkpoint();
    bool has_leaks() const;
    size_t get_leaked_bytes() const;
    void print_leak_report() const;

private:
    nimcp_memory_stats_t initial_stats_;
    nimcp_memory_stats_t final_stats_;
    bool checkpoint_taken_;
};

//=============================================================================
// Test Data Helpers
//=============================================================================

/**
 * @brief Generate synthetic test data for brain training/inference
 */
class TestDataGenerator {
public:
    // Generate random feature vector
    static std::vector<float> generate_features(size_t dim, float min = -1.0f, float max = 1.0f);

    // Generate one-hot encoded label
    static std::vector<float> generate_one_hot(size_t num_classes, size_t label_idx);

    // Generate synthetic training batch
    static void generate_training_batch(
        size_t batch_size,
        size_t input_dim,
        size_t output_dim,
        std::vector<float>& features,
        std::vector<float>& labels
    );

    // Generate XOR dataset (classic test case)
    static void generate_xor_dataset(
        std::vector<float>& features,
        std::vector<float>& labels
    );
};

//=============================================================================
// Result Aggregation
//=============================================================================

/**
 * @brief Aggregate results across multiple pipeline runs
 *
 * Used for performance regression testing and benchmarking
 */
class ResultAggregator {
public:
    void add_sample(const std::string& metric_name, double value);
    void add_duration(const std::string& operation, uint64_t duration_us);

    double get_mean(const std::string& metric_name) const;
    double get_stddev(const std::string& metric_name) const;
    double get_min(const std::string& metric_name) const;
    double get_max(const std::string& metric_name) const;

    void print_summary() const;

private:
    struct MetricStats {
        std::vector<double> samples;
        double sum = 0.0;
        double sum_sq = 0.0;
        double min_val = std::numeric_limits<double>::max();
        double max_val = std::numeric_limits<double>::lowest();
    };

    std::map<std::string, MetricStats> metrics_;
};

//=============================================================================
// Pipeline Macros (for convenience in test code)
//=============================================================================

} // namespace e2e
} // namespace nimcp

// Global pipeline tracker for current test
extern std::unique_ptr<nimcp::e2e::PipelineTracker> g_current_pipeline;

/**
 * @brief Start a new pipeline
 */
#define E2E_PIPELINE_START(name) \
    g_current_pipeline = std::make_unique<nimcp::e2e::PipelineTracker>(name)

/**
 * @brief End pipeline and assert success
 */
#define E2E_PIPELINE_END() \
    do { \
        if (g_current_pipeline) { \
            g_current_pipeline->print_summary(); \
            E2E_ASSERT_PIPELINE_SUCCESS(*g_current_pipeline); \
            g_current_pipeline.reset(); \
        } \
    } while (0)

/**
 * @brief Begin a pipeline stage with timeout (milliseconds)
 */
#define E2E_STAGE_BEGIN(name, timeout_ms) \
    do { \
        if (g_current_pipeline) { \
            g_current_pipeline->begin_stage(name, timeout_ms); \
        } \
    } while (0)

/**
 * @brief End current pipeline stage
 */
#define E2E_STAGE_END() \
    do { \
        if (g_current_pipeline) { \
            g_current_pipeline->end_stage(); \
        } \
    } while (0)

/**
 * @brief Fail current stage with error message
 */
#define E2E_STAGE_FAIL(message) \
    do { \
        if (g_current_pipeline) { \
            g_current_pipeline->fail_stage(message); \
        } \
    } while (0)

#endif // E2E_TEST_FRAMEWORK_H
