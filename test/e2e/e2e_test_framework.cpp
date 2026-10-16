/**
 * @file e2e_test_framework.cpp
 * @brief Implementation of E2E Test Framework
 *
 * WHAT: Implementation of pipeline tracking, timing, and result aggregation
 * WHY:  Support comprehensive E2E testing with rich diagnostics
 * HOW:  High-resolution timers, memory tracking, statistical aggregation
 */

#include "e2e_test_framework.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <random>

namespace nimcp {
namespace e2e {

//=============================================================================
// PipelineTracker Implementation
//=============================================================================

PipelineTracker::PipelineTracker(const std::string& name)
    : pipeline_name_(name)
    , pipeline_start_(std::chrono::high_resolution_clock::now())
    , current_stage_timeout_us_(0)
    , failed_(false)
{
    std::cout << "\n[E2E Pipeline] Starting: " << pipeline_name_ << "\n";
}

PipelineTracker::~PipelineTracker()
{
    if (!stages_.empty()) {
        print_summary();
    }
}

void PipelineTracker::begin_stage(const std::string& name, uint64_t timeout_ms)
{
    current_stage_name_ = name;
    current_stage_start_ = std::chrono::high_resolution_clock::now();
    current_stage_timeout_us_ = timeout_ms * 1000;

    std::cout << "[E2E Stage] BEGIN: " << name << " (timeout: " << timeout_ms << "ms)\n";
}

void PipelineTracker::end_stage()
{
    if (current_stage_name_.empty()) {
        return; // No stage active
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end - current_stage_start_
    ).count();

    StageResult result(current_stage_name_, duration, current_stage_timeout_us_);

    if (result.timed_out) {
        result.error_message = "Stage exceeded timeout";
        failed_ = true;
        failure_reason_ = "Stage '" + current_stage_name_ + "' timed out";
    }

    stages_.push_back(result);

    std::cout << "[E2E Stage] END: " << current_stage_name_
              << " (" << (duration / 1000.0) << "ms)"
              << (result.timed_out ? " [TIMEOUT]" : " [OK]") << "\n";

    current_stage_name_.clear();
}

void PipelineTracker::fail_stage(const std::string& error)
{
    if (!current_stage_name_.empty()) {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end - current_stage_start_
        ).count();

        StageResult result(current_stage_name_, duration, current_stage_timeout_us_);
        result.error_message = error;
        result.completed = false;
        stages_.push_back(result);

        current_stage_name_.clear();
    }

    failed_ = true;
    failure_reason_ = error;

    std::cout << "[E2E Stage] FAILED: " << error << "\n";
}

bool PipelineTracker::is_successful() const
{
    if (failed_) {
        return false;
    }

    for (const auto& stage : stages_) {
        if (!stage.completed || stage.timed_out) {
            return false;
        }
    }

    return true;
}

std::string PipelineTracker::get_failure_reason() const
{
    if (!failure_reason_.empty()) {
        return failure_reason_;
    }

    for (const auto& stage : stages_) {
        if (!stage.completed) {
            return "Stage '" + stage.name + "' did not complete";
        }
        if (stage.timed_out) {
            return "Stage '" + stage.name + "' timed out";
        }
    }

    return "Unknown failure";
}

uint64_t PipelineTracker::get_total_duration_ms() const
{
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - pipeline_start_
    ).count();
    return static_cast<uint64_t>(duration);
}

void PipelineTracker::print_summary() const
{
    std::cout << "\n[E2E Pipeline] Summary: " << pipeline_name_ << "\n";
    std::cout << "  Total stages: " << stages_.size() << "\n";
    std::cout << "  Status: " << (is_successful() ? "SUCCESS" : "FAILED") << "\n";

    if (!is_successful()) {
        std::cout << "  Failure: " << get_failure_reason() << "\n";
    }

    std::cout << "  Total duration: " << get_total_duration_ms() << "ms\n";

    if (!stages_.empty()) {
        std::cout << "\n  Stage breakdown:\n";
        for (const auto& stage : stages_) {
            std::cout << "    " << std::left << std::setw(30) << stage.name
                      << std::right << std::setw(10) << (stage.duration_us / 1000.0) << "ms"
                      << "  (timeout: " << (stage.timeout_us / 1000) << "ms)"
                      << (stage.timed_out ? " [TIMEOUT]" : "")
                      << (!stage.error_message.empty() ? " [ERROR: " + stage.error_message + "]" : "")
                      << "\n";
        }
    }

    std::cout << "\n";
}

//=============================================================================
// Timer Implementation
//=============================================================================

Timer::Timer() : running_(false) {}

void Timer::start()
{
    start_time_ = std::chrono::high_resolution_clock::now();
    running_ = true;
}

void Timer::stop()
{
    if (running_) {
        end_time_ = std::chrono::high_resolution_clock::now();
        running_ = false;
    }
}

void Timer::reset()
{
    running_ = false;
}

uint64_t Timer::elapsed_us() const
{
    auto end = running_ ? std::chrono::high_resolution_clock::now() : end_time_;
    return std::chrono::duration_cast<std::chrono::microseconds>(
        end - start_time_
    ).count();
}

uint64_t Timer::elapsed_ms() const
{
    return elapsed_us() / 1000;
}

double Timer::elapsed_sec() const
{
    return elapsed_us() / 1000000.0;
}

//=============================================================================
// MemoryLeakDetector Implementation
//=============================================================================

MemoryLeakDetector::MemoryLeakDetector()
    : checkpoint_taken_(false)
{
    // Initialize memory system if not already done
    nimcp_memory_init();

    // Capture initial state
    nimcp_memory_get_stats(&initial_stats_);
}

MemoryLeakDetector::~MemoryLeakDetector()
{
    if (checkpoint_taken_) {
        print_leak_report();
    }
}

void MemoryLeakDetector::checkpoint()
{
    nimcp_memory_get_stats(&final_stats_);
    checkpoint_taken_ = true;
}

bool MemoryLeakDetector::has_leaks() const
{
    if (!checkpoint_taken_) {
        return false;
    }

    return final_stats_.current_allocated > initial_stats_.current_allocated;
}

size_t MemoryLeakDetector::get_leaked_bytes() const
{
    if (!checkpoint_taken_ || !has_leaks()) {
        return 0;
    }

    return final_stats_.current_allocated - initial_stats_.current_allocated;
}

void MemoryLeakDetector::print_leak_report() const
{
    if (!checkpoint_taken_) {
        return;
    }

    std::cout << "\n[E2E Memory] Leak Detection Report:\n";
    std::cout << "  Initial allocated: " << initial_stats_.current_allocated << " bytes\n";
    std::cout << "  Final allocated:   " << final_stats_.current_allocated << " bytes\n";

    if (has_leaks()) {
        std::cout << "  LEAK DETECTED: " << get_leaked_bytes() << " bytes leaked\n";
    } else {
        std::cout << "  Status: No leaks detected\n";
    }

    std::cout << "\n";
}

//=============================================================================
// TestDataGenerator Implementation
//=============================================================================

std::vector<float> TestDataGenerator::generate_features(size_t dim, float min, float max)
{
    std::vector<float> features(dim);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(min, max);

    for (size_t i = 0; i < dim; ++i) {
        features[i] = dist(gen);
    }

    return features;
}

std::vector<float> TestDataGenerator::generate_one_hot(size_t num_classes, size_t label_idx)
{
    std::vector<float> one_hot(num_classes, 0.0f);
    if (label_idx < num_classes) {
        one_hot[label_idx] = 1.0f;
    }
    return one_hot;
}

void TestDataGenerator::generate_training_batch(
    size_t batch_size,
    size_t input_dim,
    size_t output_dim,
    std::vector<float>& features,
    std::vector<float>& labels)
{
    features.resize(batch_size * input_dim);
    labels.resize(batch_size * output_dim);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> feature_dist(-1.0f, 1.0f);
    std::uniform_int_distribution<size_t> label_dist(0, output_dim - 1);

    for (size_t i = 0; i < batch_size; ++i) {
        // Generate features
        for (size_t j = 0; j < input_dim; ++j) {
            features[i * input_dim + j] = feature_dist(gen);
        }

        // Generate one-hot label
        size_t label_idx = label_dist(gen);
        for (size_t j = 0; j < output_dim; ++j) {
            labels[i * output_dim + j] = (j == label_idx) ? 1.0f : 0.0f;
        }
    }
}

void TestDataGenerator::generate_xor_dataset(
    std::vector<float>& features,
    std::vector<float>& labels)
{
    // XOR dataset: 4 samples, 2 inputs, 1 output
    features = {
        0.0f, 0.0f,  // Input: [0, 0]
        0.0f, 1.0f,  // Input: [0, 1]
        1.0f, 0.0f,  // Input: [1, 0]
        1.0f, 1.0f   // Input: [1, 1]
    };

    labels = {
        0.0f,  // Output: 0
        1.0f,  // Output: 1
        1.0f,  // Output: 1
        0.0f   // Output: 0
    };
}

//=============================================================================
// ResultAggregator Implementation
//=============================================================================

void ResultAggregator::add_sample(const std::string& metric_name, double value)
{
    auto& stats = metrics_[metric_name];
    stats.samples.push_back(value);
    stats.sum += value;
    stats.sum_sq += value * value;
    stats.min_val = std::min(stats.min_val, value);
    stats.max_val = std::max(stats.max_val, value);
}

void ResultAggregator::add_duration(const std::string& operation, uint64_t duration_us)
{
    add_sample(operation + "_us", static_cast<double>(duration_us));
}

double ResultAggregator::get_mean(const std::string& metric_name) const
{
    auto it = metrics_.find(metric_name);
    if (it == metrics_.end() || it->second.samples.empty()) {
        return 0.0;
    }

    return it->second.sum / it->second.samples.size();
}

double ResultAggregator::get_stddev(const std::string& metric_name) const
{
    auto it = metrics_.find(metric_name);
    if (it == metrics_.end() || it->second.samples.size() < 2) {
        return 0.0;
    }

    size_t n = it->second.samples.size();
    double mean = it->second.sum / n;
    double variance = (it->second.sum_sq / n) - (mean * mean);
    return std::sqrt(std::max(0.0, variance));
}

double ResultAggregator::get_min(const std::string& metric_name) const
{
    auto it = metrics_.find(metric_name);
    if (it == metrics_.end()) {
        return 0.0;
    }
    return it->second.min_val;
}

double ResultAggregator::get_max(const std::string& metric_name) const
{
    auto it = metrics_.find(metric_name);
    if (it == metrics_.end()) {
        return 0.0;
    }
    return it->second.max_val;
}

void ResultAggregator::print_summary() const
{
    std::cout << "\n[E2E Results] Aggregated Statistics:\n";

    for (const auto& [metric_name, stats] : metrics_) {
        if (stats.samples.empty()) {
            continue;
        }

        double mean = stats.sum / stats.samples.size();
        double stddev = 0.0;

        if (stats.samples.size() >= 2) {
            double variance = (stats.sum_sq / stats.samples.size()) - (mean * mean);
            stddev = std::sqrt(std::max(0.0, variance));
        }

        std::cout << "  " << metric_name << ":\n";
        std::cout << "    Samples: " << stats.samples.size() << "\n";
        std::cout << "    Mean:    " << mean << "\n";
        std::cout << "    StdDev:  " << stddev << "\n";
        std::cout << "    Min:     " << stats.min_val << "\n";
        std::cout << "    Max:     " << stats.max_val << "\n";
    }

    std::cout << "\n";
}

} // namespace e2e
} // namespace nimcp

//=============================================================================
// Global Pipeline Tracker
//=============================================================================

std::unique_ptr<nimcp::e2e::PipelineTracker> g_current_pipeline;
