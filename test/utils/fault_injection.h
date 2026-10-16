/**
 * @file fault_injection.h
 * @brief Fault injection utilities for chaos testing NIMCP components
 *
 * WHAT: Mock allocator, network failure simulation, and chaos testing helpers
 * WHY:  Test system resilience to failures, memory pressure, and adverse conditions
 * HOW:  Provide injectable failure points and configurable failure policies
 *
 * USAGE:
 *   // Memory allocation failures
 *   FaultInjector::get().enable_memory_failures(0.1f);  // 10% failure rate
 *   brain_t brain = brain_create(...);  // May fail due to allocation
 *   FaultInjector::get().disable_memory_failures();
 *
 *   // Deterministic failures at specific call counts
 *   FaultInjector::get().fail_allocation_at(5);  // Fail 5th allocation
 *
 *   // Network failure simulation
 *   FaultInjector::get().simulate_network_partition();
 *   FaultInjector::get().set_network_latency_ms(100);
 *
 * NIMCP STANDARDS:
 * - Thread-safe singleton pattern
 * - RAII-based scope guards for automatic cleanup
 * - No side effects when disabled
 */

#ifndef NIMCP_TEST_FAULT_INJECTION_H
#define NIMCP_TEST_FAULT_INJECTION_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <random>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// C API for Hook Integration
// ============================================================================

/**
 * @brief Check if allocation should fail
 * @param size Requested allocation size
 * @return true if allocation should fail, false otherwise
 */
bool fault_injection_should_fail_allocation(size_t size);

/**
 * @brief Check if network operation should fail
 * @return true if network operation should fail
 */
bool fault_injection_should_fail_network(void);

/**
 * @brief Get simulated network latency in milliseconds
 * @return Latency to add, or 0 if no latency injection
 */
uint32_t fault_injection_get_network_latency_ms(void);

/**
 * @brief Reset all fault injection state
 */
void fault_injection_reset(void);

#ifdef __cplusplus
}

// ============================================================================
// C++ Fault Injector Class
// ============================================================================

namespace nimcp {
namespace test {

/**
 * @brief Thread-safe singleton for fault injection
 *
 * Provides configurable failure injection for:
 * - Memory allocation (malloc, calloc, realloc)
 * - Network operations (connect, send, recv)
 * - File I/O operations
 * - Custom failure points
 */
class FaultInjector {
public:
    // ========================================================================
    // Singleton Access
    // ========================================================================

    /**
     * @brief Get singleton instance
     * @return Reference to FaultInjector instance
     */
    static FaultInjector& get() {
        static FaultInjector instance;
        return instance;
    }

    // Prevent copying
    FaultInjector(const FaultInjector&) = delete;
    FaultInjector& operator=(const FaultInjector&) = delete;

    // ========================================================================
    // Memory Allocation Failures
    // ========================================================================

    /**
     * @brief Enable probabilistic memory allocation failures
     * @param failure_rate Probability of failure (0.0 to 1.0)
     */
    void enable_memory_failures(float failure_rate) {
        std::lock_guard<std::mutex> lock(mutex_);
        memory_failure_rate_ = std::min(1.0f, std::max(0.0f, failure_rate));
        memory_failures_enabled_ = true;
    }

    /**
     * @brief Disable memory allocation failures
     */
    void disable_memory_failures() {
        std::lock_guard<std::mutex> lock(mutex_);
        memory_failures_enabled_ = false;
        memory_failure_rate_ = 0.0f;
    }

    /**
     * @brief Fail allocation at specific call count
     * @param call_number Which allocation to fail (1-indexed)
     */
    void fail_allocation_at(uint32_t call_number) {
        std::lock_guard<std::mutex> lock(mutex_);
        deterministic_failure_at_ = call_number;
    }

    /**
     * @brief Fail allocations above a size threshold
     * @param threshold_bytes Allocations >= this size will fail
     */
    void fail_allocations_above(size_t threshold_bytes) {
        std::lock_guard<std::mutex> lock(mutex_);
        size_failure_threshold_ = threshold_bytes;
    }

    /**
     * @brief Check if allocation should fail
     * @param size Requested allocation size
     * @return true if allocation should fail
     */
    bool should_fail_allocation(size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);

        allocation_count_++;

        // Deterministic failure at specific call
        if (deterministic_failure_at_ > 0 &&
            allocation_count_ == deterministic_failure_at_) {
            return true;
        }

        // Size-based failure
        if (size_failure_threshold_ > 0 && size >= size_failure_threshold_) {
            return true;
        }

        // Probabilistic failure
        if (memory_failures_enabled_ && memory_failure_rate_ > 0.0f) {
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            return dist(rng_) < memory_failure_rate_;
        }

        return false;
    }

    // ========================================================================
    // Network Failure Simulation
    // ========================================================================

    /**
     * @brief Simulate network partition (all network ops fail)
     */
    void simulate_network_partition() {
        std::lock_guard<std::mutex> lock(mutex_);
        network_partitioned_ = true;
    }

    /**
     * @brief End network partition simulation
     */
    void heal_network_partition() {
        std::lock_guard<std::mutex> lock(mutex_);
        network_partitioned_ = false;
    }

    /**
     * @brief Set network failure probability
     * @param failure_rate Probability of network operation failure (0.0 to 1.0)
     */
    void set_network_failure_rate(float failure_rate) {
        std::lock_guard<std::mutex> lock(mutex_);
        network_failure_rate_ = std::min(1.0f, std::max(0.0f, failure_rate));
    }

    /**
     * @brief Set simulated network latency
     * @param latency_ms Latency to add to network operations (milliseconds)
     */
    void set_network_latency_ms(uint32_t latency_ms) {
        std::lock_guard<std::mutex> lock(mutex_);
        network_latency_ms_ = latency_ms;
    }

    /**
     * @brief Set simulated packet loss rate
     * @param loss_rate Probability of packet loss (0.0 to 1.0)
     */
    void set_packet_loss_rate(float loss_rate) {
        std::lock_guard<std::mutex> lock(mutex_);
        packet_loss_rate_ = std::min(1.0f, std::max(0.0f, loss_rate));
    }

    /**
     * @brief Check if network operation should fail
     * @return true if network operation should fail
     */
    bool should_fail_network() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (network_partitioned_) {
            return true;
        }

        if (network_failure_rate_ > 0.0f) {
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            return dist(rng_) < network_failure_rate_;
        }

        return false;
    }

    /**
     * @brief Get current network latency setting
     * @return Latency in milliseconds
     */
    uint32_t get_network_latency_ms() {
        std::lock_guard<std::mutex> lock(mutex_);
        return network_latency_ms_;
    }

    /**
     * @brief Check if packet should be dropped
     * @return true if packet should be dropped
     */
    bool should_drop_packet() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (packet_loss_rate_ > 0.0f) {
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            return dist(rng_) < packet_loss_rate_;
        }

        return false;
    }

    // ========================================================================
    // Custom Failure Points
    // ========================================================================

    /**
     * @brief Register a custom failure callback
     * @param name Failure point name
     * @param callback Function that returns true if operation should fail
     */
    void register_failure_point(const std::string& name,
                                std::function<bool()> callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        failure_points_[name] = callback;
    }

    /**
     * @brief Check custom failure point
     * @param name Failure point name
     * @return true if operation should fail, false if point not found or passes
     */
    bool check_failure_point(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = failure_points_.find(name);
        if (it != failure_points_.end()) {
            return it->second();
        }
        return false;
    }

    // ========================================================================
    // Statistics
    // ========================================================================

    /**
     * @brief Get total allocation count since last reset
     * @return Number of allocation calls
     */
    uint32_t get_allocation_count() const {
        return allocation_count_.load();
    }

    /**
     * @brief Get number of injected failures
     * @return Count of injected failures
     */
    uint32_t get_injected_failure_count() const {
        return injected_failures_.load();
    }

    // ========================================================================
    // Reset
    // ========================================================================

    /**
     * @brief Reset all fault injection state
     */
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);

        memory_failures_enabled_ = false;
        memory_failure_rate_ = 0.0f;
        deterministic_failure_at_ = 0;
        size_failure_threshold_ = 0;
        allocation_count_ = 0;

        network_partitioned_ = false;
        network_failure_rate_ = 0.0f;
        network_latency_ms_ = 0;
        packet_loss_rate_ = 0.0f;

        failure_points_.clear();
        injected_failures_ = 0;
    }

private:
    FaultInjector()
        : memory_failures_enabled_(false)
        , memory_failure_rate_(0.0f)
        , deterministic_failure_at_(0)
        , size_failure_threshold_(0)
        , allocation_count_(0)
        , network_partitioned_(false)
        , network_failure_rate_(0.0f)
        , network_latency_ms_(0)
        , packet_loss_rate_(0.0f)
        , injected_failures_(0)
        , rng_(std::random_device{}()) {}

    std::mutex mutex_;

    // Memory failure settings
    bool memory_failures_enabled_;
    float memory_failure_rate_;
    uint32_t deterministic_failure_at_;
    size_t size_failure_threshold_;
    std::atomic<uint32_t> allocation_count_;

    // Network failure settings
    bool network_partitioned_;
    float network_failure_rate_;
    uint32_t network_latency_ms_;
    float packet_loss_rate_;

    // Custom failure points
    std::unordered_map<std::string, std::function<bool()>> failure_points_;

    // Statistics
    std::atomic<uint32_t> injected_failures_;

    // Random number generator
    std::mt19937 rng_;
};

// ============================================================================
// RAII Scope Guards
// ============================================================================

/**
 * @brief RAII guard for memory failure injection
 *
 * USAGE:
 *   {
 *       MemoryFailureGuard guard(0.1f);  // 10% failure rate
 *       // ... code that may fail allocations ...
 *   }  // Automatically disabled
 */
class MemoryFailureGuard {
public:
    explicit MemoryFailureGuard(float failure_rate) {
        FaultInjector::get().enable_memory_failures(failure_rate);
    }

    ~MemoryFailureGuard() {
        FaultInjector::get().disable_memory_failures();
    }

    // Non-copyable, non-movable
    MemoryFailureGuard(const MemoryFailureGuard&) = delete;
    MemoryFailureGuard& operator=(const MemoryFailureGuard&) = delete;
};

/**
 * @brief RAII guard for network partition simulation
 *
 * USAGE:
 *   {
 *       NetworkPartitionGuard guard;
 *       // ... network operations will fail ...
 *   }  // Partition automatically healed
 */
class NetworkPartitionGuard {
public:
    NetworkPartitionGuard() {
        FaultInjector::get().simulate_network_partition();
    }

    ~NetworkPartitionGuard() {
        FaultInjector::get().heal_network_partition();
    }

    // Non-copyable, non-movable
    NetworkPartitionGuard(const NetworkPartitionGuard&) = delete;
    NetworkPartitionGuard& operator=(const NetworkPartitionGuard&) = delete;
};

/**
 * @brief RAII guard for network latency simulation
 */
class NetworkLatencyGuard {
public:
    explicit NetworkLatencyGuard(uint32_t latency_ms) {
        FaultInjector::get().set_network_latency_ms(latency_ms);
    }

    ~NetworkLatencyGuard() {
        FaultInjector::get().set_network_latency_ms(0);
    }

    // Non-copyable, non-movable
    NetworkLatencyGuard(const NetworkLatencyGuard&) = delete;
    NetworkLatencyGuard& operator=(const NetworkLatencyGuard&) = delete;
};

}  // namespace test
}  // namespace nimcp

#endif  // __cplusplus

#endif  // NIMCP_TEST_FAULT_INJECTION_H
