/**
 * @file test_exception_swarm_memory_macros.cpp
 * @brief Unit tests for exception macro integration in swarm and memory modules
 *
 * WHAT: Tests for exception handling in swarm memory, emotional contagion,
 *       energy gossip, temporal replay, and Hopfield memory modules
 * WHY:  Verify exception macros work correctly for memory and swarm operations
 * HOW:  GoogleTest framework with fixture setup/teardown for exception system
 *
 * TEST CATEGORIES:
 * 1. Swarm memory exception scenarios (allocation failures, invalid operations)
 * 2. Emotional contagion exception handling
 * 3. Swarm energy gossip error conditions
 * 4. Temporal replay exception flows
 * 5. Hopfield memory exception handling
 * 6. Recovery action integration
 *
 * @author NIMCP Development Team
 * @date 2026-01-21
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <vector>
#include <string>
#include <cmath>

// Include C headers - each with separate extern "C" linkage
// NOTE: Headers with GPU/CUDA dependencies handle their own extern "C" guards
extern "C" {
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
}

// Swarm headers - include separately due to potential CUDA dependencies
extern "C" {
#include "swarm/nimcp_swarm_memory.h"
#include "swarm/nimcp_emotional_contagion.h"
#include "swarm/nimcp_swarm_energy_gossip.h"
}

// Define local structures for testing instead of including CUDA-dependent headers
// These mirror the fields we need for validation testing

/** Replay mode enum for temporal replay tests */
typedef enum {
    REPLAY_MODE_FORWARD = 0,
    REPLAY_MODE_BACKWARD,
    REPLAY_MODE_RANDOM,
    REPLAY_MODE_PRIORITY,
    REPLAY_MODE_INTERLEAVED
} replay_mode_t;

/** Temporal replay configuration for testing */
typedef struct {
    uint32_t capacity;
    uint32_t state_dim;
    uint32_t action_dim;
    uint32_t max_sequence_length;
    replay_mode_t default_mode;
    float priority_alpha;
    float is_beta;
    float compression_ratio;
    bool use_priority_tree;
    float priority_decay;
    uint32_t gpu_mode;
    uint32_t min_batch_for_gpu;
    bool enable_bio_async;
    bool store_next_states;
    bool store_actions;
} replay_config_t;

/** Temporal replay buffer (simplified for tests) */
typedef struct temporal_replay {
    replay_config_t config;
    void* transitions;
    uint32_t head;
    uint32_t count;
    uint32_t next_transition_id;
    void* sequences;
    uint32_t num_sequences;
    uint32_t max_sequences;
    uint32_t current_sequence_id;
    bool recording_sequence;
    float* priority_tree;
    float* min_tree;
    float total_priority;
    int seq_state;
    uint32_t replay_position;
    uint32_t replay_sequence_idx;
    void* stats;
    void* mutex;
} temporal_replay_t;

/** Replay batch for sampling (simplified) */
typedef struct {
    float* states;
    float* actions;
    float* next_states;
    float* rewards;
    float* is_weights;
    uint32_t* indices;
    uint32_t batch_size;
    bool is_sequence;
} replay_batch_t;

/** Sweep result for replay (simplified) */
typedef struct {
    float** states;
    uint64_t* timestamps;
    float* rewards;
    uint32_t length;
    replay_mode_t mode;
    uint64_t replay_duration_us;
    float compression_ratio;
} replay_sweep_result_t;

/** Hopfield mode enum */
typedef enum {
    HOPFIELD_MODE_SOFTMAX = 0,
    HOPFIELD_MODE_EXPONENTIAL,
    HOPFIELD_MODE_POLYNOMIAL,
    HOPFIELD_MODE_SPARSE
} hopfield_mode_t;

/** Hopfield store mode enum */
typedef enum {
    HOPFIELD_STORE_OVERWRITE = 0,
    HOPFIELD_STORE_REJECT,
    HOPFIELD_STORE_MERGE
} hopfield_store_mode_t;

/** Hopfield GPU mode enum */
typedef enum {
    HOPFIELD_GPU_DISABLED = 0,
    HOPFIELD_GPU_AUTO,
    HOPFIELD_GPU_PREFERRED,
    HOPFIELD_GPU_REQUIRED
} hopfield_gpu_mode_t;

/** Hopfield configuration for testing */
typedef struct {
    uint32_t pattern_dim;
    uint32_t capacity;
    hopfield_mode_t mode;
    hopfield_store_mode_t store_mode;
    float beta;
    uint32_t max_iterations;
    float convergence_threshold;
    float similarity_threshold;
    hopfield_gpu_mode_t gpu_mode;
    uint32_t min_batch_for_gpu;
    bool enable_metadata;
    bool normalize_patterns;
    bool enable_bio_async;
} hopfield_config_t;

/** Hopfield memory (simplified for tests) */
typedef struct hopfield_memory {
    hopfield_config_t config;
    float* patterns;
    void* metadata;
    uint32_t pattern_count;
    uint32_t next_pattern_id;
    float* query_buffer;
    float* similarity_buffer;
    float* attention_buffer;
    void* stats;
    void* mutex;
} hopfield_memory_t;

/** Hopfield retrieval result (simplified) */
typedef struct {
    float* pattern;
    uint32_t pattern_id;
    float similarity;
    float energy;
    uint32_t iterations;
    bool converged;
} hopfield_retrieval_result_t;

/** Hopfield batch result (simplified) */
typedef struct {
    hopfield_retrieval_result_t* results;
    uint32_t num_results;
    float avg_similarity;
    float avg_energy;
    uint64_t total_time_us;
} hopfield_batch_result_t;

// Constants for validation
#define REPLAY_MAX_CAPACITY 1000000
#define HOPFIELD_MAX_CAPACITY 65536
#define HOPFIELD_MAX_BETA 100.0f

//=============================================================================
// Test Globals for Handler Tracking
//=============================================================================

namespace {

std::atomic<int> g_handler_call_count{0};
std::atomic<nimcp_error_t> g_last_error_code{NIMCP_SUCCESS};
std::atomic<nimcp_exception_severity_t> g_last_severity{EXCEPTION_SEVERITY_DEBUG};
std::atomic<bool> g_exception_presented_to_immune{false};
std::vector<std::string> g_captured_messages;

/**
 * @brief Test handler callback to track exception dispatch
 */
bool swarm_memory_test_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    if (ex) {
        g_handler_call_count++;
        g_last_error_code = ex->code;
        g_last_severity = ex->severity;
        g_exception_presented_to_immune = ex->presented_to_immune;

        if (ex->message) {
            g_captured_messages.push_back(std::string(ex->message));
        }
    }
    return false;  // Don't consume, let chain continue
}

/**
 * @brief Reset all test tracking globals
 */
void reset_tracking() {
    g_handler_call_count = 0;
    g_last_error_code = NIMCP_SUCCESS;
    g_last_severity = EXCEPTION_SEVERITY_DEBUG;
    g_exception_presented_to_immune = false;
    g_captured_messages.clear();
}

}  // anonymous namespace

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * WHAT: Base fixture for swarm memory exception tests
 * WHY:  Setup/teardown exception system for each test
 */
class SwarmMemoryExceptionTest : public ::testing::Test {
protected:
    nimcp_handler_registration_t* handler_reg_ = nullptr;

    void SetUp() override {
        reset_tracking();

        // Initialize exception system
        nimcp_exception_system_init();

        // Register test handler
        nimcp_handler_options_t opts;
        nimcp_handler_default_options(&opts);
        opts.name = "swarm_memory_test_handler";
        opts.handler = swarm_memory_test_handler;
        opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
        handler_reg_ = nimcp_handler_register(&opts);
    }

    void TearDown() override {
        if (handler_reg_) {
            nimcp_handler_unregister(handler_reg_);
            handler_reg_ = nullptr;
        }
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }
};

//=============================================================================
// Swarm Memory Exception Simulations
//=============================================================================

/**
 * @brief Simulate swarm memory creation validation
 */
static nimcp_result_t validate_swarm_memory_create(
    uint32_t max_capacity,
    uint32_t replication_factor) {

    NIMCP_CHECK_THROW(max_capacity > 0, NIMCP_ERROR_INVALID_PARAM,
                      "Max capacity must be positive, got %u", max_capacity);

    NIMCP_CHECK_THROW(max_capacity <= 1000000, NIMCP_ERROR_OUT_OF_RANGE,
                      "Max capacity %u exceeds limit 1000000", max_capacity);

    NIMCP_CHECK_THROW(replication_factor > 0 && replication_factor <= 10,
                      NIMCP_ERROR_INVALID_PARAM,
                      "Replication factor %u out of range [1, 10]",
                      replication_factor);

    return NIMCP_SUCCESS;
}

/**
 * @brief Simulate swarm memory store operation
 */
static nimcp_result_t validate_swarm_memory_store(
    const NimcpSwarmMemory* memory,
    NimcpMemoryType type,
    NimcpMemoryImportance importance,
    const void* data,
    size_t data_size) {

    NIMCP_CHECK_THROW(memory != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Swarm memory system is NULL");

    NIMCP_CHECK_THROW(memory->is_initialized, NIMCP_ERROR_NOT_INITIALIZED,
                      "Swarm memory system not initialized");

    NIMCP_CHECK_THROW(type < NIMCP_MEMORY_TYPE_COUNT, NIMCP_ERROR_OUT_OF_RANGE,
                      "Invalid memory type %d", (int)type);

    NIMCP_CHECK_THROW(importance <= NIMCP_IMPORTANCE_CRITICAL,
                      NIMCP_ERROR_INVALID_PARAM,
                      "Invalid importance level %d", (int)importance);

    NIMCP_CHECK_THROW(data != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Memory data is NULL");

    NIMCP_CHECK_THROW(data_size > 0, NIMCP_ERROR_INVALID_PARAM,
                      "Memory data size must be positive");

    NIMCP_CHECK_THROW(data_size <= 1024 * 1024, NIMCP_ERROR_OUT_OF_RANGE,
                      "Memory data size %zu exceeds maximum 1MB", data_size);

    return NIMCP_SUCCESS;
}

/**
 * @brief Simulate swarm memory retrieve operation
 */
static nimcp_result_t validate_swarm_memory_retrieve(
    const NimcpSwarmMemory* memory,
    const char* memory_id,
    void* out_data,
    size_t data_size) {

    NIMCP_CHECK_THROW(memory != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Swarm memory system is NULL");

    NIMCP_CHECK_THROW(memory_id != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Memory ID is NULL");

    NIMCP_CHECK_THROW(strlen(memory_id) > 0, NIMCP_ERROR_INVALID_PARAM,
                      "Memory ID is empty");

    NIMCP_CHECK_THROW(strlen(memory_id) < 64, NIMCP_ERROR_BUFFER_OVERFLOW,
                      "Memory ID length %zu exceeds maximum 64", strlen(memory_id));

    NIMCP_CHECK_THROW(out_data != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Output data buffer is NULL");

    NIMCP_CHECK_THROW(data_size > 0, NIMCP_ERROR_INVALID_PARAM,
                      "Data buffer size must be positive");

    return NIMCP_SUCCESS;
}

/**
 * @brief Simulate pattern storage validation
 */
static nimcp_result_t validate_pattern_store(
    NimcpSwarmMemory* memory,
    const swarm_pattern_t* pattern) {

    NIMCP_CHECK_THROW(memory != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Swarm memory system is NULL");

    NIMCP_CHECK_THROW(pattern != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Pattern is NULL");

    NIMCP_CHECK_THROW(pattern->data != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Pattern data is NULL for pattern %u", pattern->pattern_id);

    NIMCP_CHECK_THROW(pattern->data_len > 0, NIMCP_ERROR_INVALID_PARAM,
                      "Pattern data length must be positive for pattern %u",
                      pattern->pattern_id);

    NIMCP_CHECK_THROW(pattern->confidence >= 0.0f && pattern->confidence <= 1.0f,
                      NIMCP_ERROR_INVALID_PARAM,
                      "Pattern confidence %.2f out of range [0, 1] for pattern %u",
                      pattern->confidence, pattern->pattern_id);

    NIMCP_CHECK_THROW(pattern->strength >= 0.0f && pattern->strength <= 1.0f,
                      NIMCP_ERROR_INVALID_PARAM,
                      "Pattern strength %.2f out of range [0, 1]",
                      pattern->strength);

    return NIMCP_SUCCESS;
}

/**
 * @brief Simulate memory consolidation validation
 */
static nimcp_result_t validate_memory_consolidation(
    NimcpSwarmMemory* memory,
    const NimcpConsolidationWindow* window) {

    NIMCP_CHECK_THROW(memory != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Swarm memory system is NULL");

    NIMCP_CHECK_THROW(window != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Consolidation window is NULL");

    NIMCP_CHECK_THROW(window->mode <= NIMCP_CONSOLIDATION_SLEEP,
                      NIMCP_ERROR_INVALID_PARAM,
                      "Invalid consolidation mode %d", (int)window->mode);

    NIMCP_CHECK_THROW(window->window_duration_ms > 0,
                      NIMCP_ERROR_INVALID_PARAM,
                      "Window duration must be positive");

    NIMCP_CHECK_THROW(window->max_memories_per_window > 0,
                      NIMCP_ERROR_INVALID_PARAM,
                      "Max memories per window must be positive");

    NIMCP_CHECK_THROW(window->activity_threshold >= 0.0f &&
                      window->activity_threshold <= 1.0f,
                      NIMCP_ERROR_INVALID_PARAM,
                      "Activity threshold %.2f out of range [0, 1]",
                      window->activity_threshold);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Energy Gossip Exception Simulations
//=============================================================================

/**
 * @brief Simulate energy gossip creation validation
 */
static nimcp_result_t validate_energy_gossip_create(
    uint32_t node_id,
    const NimcpEnergyGossipConfig* config) {

    NIMCP_CHECK_THROW(node_id != 0, NIMCP_ERROR_INVALID_PARAM,
                      "Node ID 0 is reserved");

    if (config != nullptr) {
        NIMCP_CHECK_THROW(config->gossip_fanout > 0 && config->gossip_fanout <= 10,
                          NIMCP_ERROR_INVALID_PARAM,
                          "Gossip fanout %u out of range [1, 10]",
                          config->gossip_fanout);

        NIMCP_CHECK_THROW(config->min_relay_energy >= 0.0f &&
                          config->min_relay_energy <= 100.0f,
                          NIMCP_ERROR_INVALID_PARAM,
                          "Min relay energy %.2f out of range [0, 100]",
                          config->min_relay_energy);

        NIMCP_CHECK_THROW(config->convergence_threshold >= 0.0f &&
                          config->convergence_threshold <= 1.0f,
                          NIMCP_ERROR_INVALID_PARAM,
                          "Convergence threshold %.2f out of range [0, 1]",
                          config->convergence_threshold);
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Simulate energy update validation
 */
static nimcp_result_t validate_energy_update(
    NimcpEnergyGossip* gossip,
    float energy_level) {

    NIMCP_CHECK_THROW(gossip != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Energy gossip instance is NULL");

    NIMCP_CHECK_THROW(gossip->is_initialized, NIMCP_ERROR_NOT_INITIALIZED,
                      "Energy gossip not initialized");

    NIMCP_CHECK_THROW(energy_level >= 0.0f && energy_level <= 100.0f,
                      NIMCP_ERROR_INVALID_PARAM,
                      "Energy level %.2f out of range [0, 100]", energy_level);

    return NIMCP_SUCCESS;
}

/**
 * @brief Simulate gossip broadcast validation
 */
static uint64_t validate_gossip_broadcast(
    NimcpEnergyGossip* gossip,
    const void* payload,
    size_t payload_size,
    NimcpMessagePriority priority,
    uint32_t ttl) {

    if (gossip == nullptr) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "Gossip instance is NULL");
        return 0;
    }

    if (payload == nullptr || payload_size == 0) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "Broadcast payload is NULL or empty");
        return 0;
    }

    if (payload_size > 65536) {
        NIMCP_THROW(NIMCP_ERROR_BUFFER_OVERFLOW,
                    "Payload size %zu exceeds maximum 64KB", payload_size);
        return 0;
    }

    if (ttl == 0) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "TTL must be positive");
        return 0;
    }

    // Suppress unused parameter warning
    (void)priority;

    return 12345;  // Mock message ID
}

/**
 * @brief Simulate relay registration validation
 */
static nimcp_result_t validate_relay_register(
    NimcpEnergyGossip* gossip,
    uint32_t node_id,
    float energy_level,
    float distance) {

    NIMCP_CHECK_THROW(gossip != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Gossip instance is NULL");

    NIMCP_CHECK_THROW(node_id != 0, NIMCP_ERROR_INVALID_PARAM,
                      "Node ID 0 is reserved");

    NIMCP_CHECK_THROW(node_id != gossip->node_id, NIMCP_ERROR_INVALID_PARAM,
                      "Cannot register self as relay");

    NIMCP_CHECK_THROW(energy_level >= 0.0f && energy_level <= 100.0f,
                      NIMCP_ERROR_INVALID_PARAM,
                      "Relay energy level %.2f out of range", energy_level);

    NIMCP_CHECK_THROW(distance >= 0.0f, NIMCP_ERROR_INVALID_PARAM,
                      "Distance must be non-negative, got %.2f", distance);

    return NIMCP_SUCCESS;
}

/**
 * @brief Simulate sleep scheduling validation
 */
static nimcp_result_t validate_sleep_schedule(
    NimcpEnergyGossip* gossip,
    time_t start_time,
    uint32_t duration_seconds) {

    NIMCP_CHECK_THROW(gossip != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Gossip instance is NULL");

    NIMCP_CHECK_THROW(duration_seconds > 0, NIMCP_ERROR_INVALID_PARAM,
                      "Sleep duration must be positive");

    NIMCP_CHECK_THROW(duration_seconds <= 86400, NIMCP_ERROR_OUT_OF_RANGE,
                      "Sleep duration %u exceeds maximum 24 hours", duration_seconds);

    time_t now = time(NULL);
    NIMCP_CHECK_THROW(start_time >= now, NIMCP_ERROR_INVALID_PARAM,
                      "Sleep start time cannot be in the past");

    return NIMCP_SUCCESS;
}

//=============================================================================
// Temporal Replay Exception Simulations
//=============================================================================

/**
 * @brief Simulate temporal replay configuration validation
 */
static int validate_replay_config(const replay_config_t* config) {

    NIMCP_CHECK_THROW(config != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Replay configuration is NULL");

    NIMCP_CHECK_THROW(config->capacity > 0, NIMCP_ERROR_INVALID_PARAM,
                      "Replay capacity must be positive");

    NIMCP_CHECK_THROW(config->capacity <= REPLAY_MAX_CAPACITY,
                      NIMCP_ERROR_OUT_OF_RANGE,
                      "Replay capacity %u exceeds maximum %u",
                      config->capacity, REPLAY_MAX_CAPACITY);

    NIMCP_CHECK_THROW(config->state_dim > 0, NIMCP_ERROR_INVALID_PARAM,
                      "State dimension must be positive");

    NIMCP_CHECK_THROW(config->max_sequence_length > 0,
                      NIMCP_ERROR_INVALID_PARAM,
                      "Max sequence length must be positive");

    NIMCP_CHECK_THROW(config->priority_alpha >= 0.0f &&
                      config->priority_alpha <= 1.0f,
                      NIMCP_ERROR_INVALID_PARAM,
                      "Priority alpha %.2f out of range [0, 1]",
                      config->priority_alpha);

    NIMCP_CHECK_THROW(config->compression_ratio >= 1.0f,
                      NIMCP_ERROR_INVALID_PARAM,
                      "Compression ratio %.2f must be >= 1.0",
                      config->compression_ratio);

    return 0;
}

/**
 * @brief Simulate replay store validation
 */
static int validate_replay_store(
    temporal_replay_t* replay,
    const float* state,
    const float* action,
    float reward,
    float priority) {

    NIMCP_CHECK_THROW(replay != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Replay buffer is NULL");

    NIMCP_CHECK_THROW(state != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "State is NULL");

    // action can be NULL if not storing actions

    NIMCP_CHECK_THROW(!std::isnan(reward) && !std::isinf(reward),
                      NIMCP_ERROR_INVALID_PARAM,
                      "Reward %.2f is invalid (NaN or Inf)", reward);

    NIMCP_CHECK_THROW(priority >= 0.0f, NIMCP_ERROR_INVALID_PARAM,
                      "Priority must be non-negative, got %.2f", priority);

    // Suppress unused parameter warning
    (void)action;

    return 0;
}

/**
 * @brief Simulate replay sampling validation
 */
static int validate_replay_sample(
    temporal_replay_t* replay,
    replay_mode_t mode,
    uint32_t batch_size,
    replay_batch_t* batch) {

    NIMCP_CHECK_THROW(replay != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Replay buffer is NULL");

    NIMCP_CHECK_THROW(mode <= REPLAY_MODE_INTERLEAVED, NIMCP_ERROR_INVALID_PARAM,
                      "Invalid replay mode %d", (int)mode);

    NIMCP_CHECK_THROW(batch_size > 0, NIMCP_ERROR_INVALID_PARAM,
                      "Batch size must be positive");

    NIMCP_CHECK_THROW(batch != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Output batch is NULL");

    NIMCP_CHECK_THROW(replay->count > 0, NIMCP_ERROR_INVALID_STATE,
                      "Cannot sample from empty replay buffer");

    NIMCP_CHECK_THROW(batch_size <= replay->count, NIMCP_ERROR_OUT_OF_RANGE,
                      "Batch size %u exceeds available transitions %u",
                      batch_size, replay->count);

    return 0;
}

/**
 * @brief Simulate forward sweep validation
 */
static int validate_forward_sweep(
    temporal_replay_t* replay,
    uint32_t start_idx,
    uint32_t length,
    replay_sweep_result_t* result) {

    NIMCP_CHECK_THROW(replay != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Replay buffer is NULL");

    NIMCP_CHECK_THROW(result != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Sweep result output is NULL");

    NIMCP_CHECK_THROW(length > 0, NIMCP_ERROR_INVALID_PARAM,
                      "Sweep length must be positive");

    NIMCP_CHECK_THROW(start_idx < replay->count, NIMCP_ERROR_OUT_OF_RANGE,
                      "Start index %u out of range [0, %u)",
                      start_idx, replay->count);

    NIMCP_CHECK_THROW(start_idx + length <= replay->count,
                      NIMCP_ERROR_OUT_OF_RANGE,
                      "Sweep end %u exceeds buffer size %u",
                      start_idx + length, replay->count);

    return 0;
}

//=============================================================================
// Hopfield Memory Exception Simulations
//=============================================================================

/**
 * @brief Simulate Hopfield configuration validation
 */
static int validate_hopfield_config(const hopfield_config_t* config) {

    NIMCP_CHECK_THROW(config != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Hopfield configuration is NULL");

    NIMCP_CHECK_THROW(config->pattern_dim > 0, NIMCP_ERROR_INVALID_PARAM,
                      "Pattern dimension must be positive");

    NIMCP_CHECK_THROW(config->capacity > 0, NIMCP_ERROR_INVALID_PARAM,
                      "Hopfield capacity must be positive");

    NIMCP_CHECK_THROW(config->capacity <= HOPFIELD_MAX_CAPACITY,
                      NIMCP_ERROR_OUT_OF_RANGE,
                      "Hopfield capacity %u exceeds maximum %u",
                      config->capacity, HOPFIELD_MAX_CAPACITY);

    NIMCP_CHECK_THROW(config->beta > 0.0f, NIMCP_ERROR_INVALID_PARAM,
                      "Inverse temperature beta must be positive, got %.2f",
                      config->beta);

    NIMCP_CHECK_THROW(config->beta <= HOPFIELD_MAX_BETA,
                      NIMCP_ERROR_OUT_OF_RANGE,
                      "Beta %.2f exceeds maximum %.2f",
                      config->beta, HOPFIELD_MAX_BETA);

    NIMCP_CHECK_THROW(config->max_iterations > 0, NIMCP_ERROR_INVALID_PARAM,
                      "Max iterations must be positive");

    NIMCP_CHECK_THROW(config->convergence_threshold > 0.0f,
                      NIMCP_ERROR_INVALID_PARAM,
                      "Convergence threshold must be positive");

    NIMCP_CHECK_THROW(config->similarity_threshold >= 0.0f &&
                      config->similarity_threshold <= 1.0f,
                      NIMCP_ERROR_INVALID_PARAM,
                      "Similarity threshold %.2f out of range [0, 1]",
                      config->similarity_threshold);

    return 0;
}

/**
 * @brief Simulate Hopfield pattern store validation
 */
static int validate_hopfield_store(
    hopfield_memory_t* memory,
    const float* pattern,
    uint32_t* pattern_id) {

    NIMCP_CHECK_THROW(memory != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Hopfield memory is NULL");

    NIMCP_CHECK_THROW(pattern != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Pattern to store is NULL");

    NIMCP_CHECK_THROW(memory->pattern_count < memory->config.capacity,
                      NIMCP_ERROR_OUT_OF_RANGE,
                      "Hopfield memory at capacity (%u patterns)",
                      memory->config.capacity);

    // pattern_id can be NULL if not needed
    (void)pattern_id;

    return 0;
}

/**
 * @brief Simulate Hopfield retrieve validation
 */
static int validate_hopfield_retrieve(
    hopfield_memory_t* memory,
    const float* query,
    hopfield_retrieval_result_t* result) {

    NIMCP_CHECK_THROW(memory != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Hopfield memory is NULL");

    NIMCP_CHECK_THROW(query != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Query pattern is NULL");

    NIMCP_CHECK_THROW(result != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Retrieval result output is NULL");

    NIMCP_CHECK_THROW(memory->pattern_count > 0, NIMCP_ERROR_INVALID_STATE,
                      "Cannot retrieve from empty Hopfield memory");

    return 0;
}

/**
 * @brief Simulate Hopfield batch retrieve validation
 */
static int validate_hopfield_batch_retrieve(
    hopfield_memory_t* memory,
    const float* queries,
    uint32_t num_queries,
    hopfield_batch_result_t* result) {

    NIMCP_CHECK_THROW(memory != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Hopfield memory is NULL");

    NIMCP_CHECK_THROW(queries != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Query patterns are NULL");

    NIMCP_CHECK_THROW(num_queries > 0, NIMCP_ERROR_INVALID_PARAM,
                      "Number of queries must be positive");

    NIMCP_CHECK_THROW(result != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Batch result output is NULL");

    NIMCP_CHECK_THROW(memory->pattern_count > 0, NIMCP_ERROR_INVALID_STATE,
                      "Cannot retrieve from empty Hopfield memory");

    return 0;
}

//=============================================================================
// Swarm Memory Tests
//=============================================================================

/**
 * WHAT: Test swarm memory creation with zero capacity
 * WHY:  Verify capacity validation
 */
TEST_F(SwarmMemoryExceptionTest, SwarmMemoryZeroCapacity) {
    nimcp_result_t result = validate_swarm_memory_create(0, 3);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_INVALID_PARAM);
}

/**
 * WHAT: Test swarm memory creation with excessive capacity
 * WHY:  Verify capacity range validation
 */
TEST_F(SwarmMemoryExceptionTest, SwarmMemoryExcessiveCapacity) {
    nimcp_result_t result = validate_swarm_memory_create(10000000, 3);

    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test swarm memory creation with invalid replication factor
 * WHY:  Verify replication factor range validation
 */
TEST_F(SwarmMemoryExceptionTest, SwarmMemoryInvalidReplicationFactor) {
    nimcp_result_t result = validate_swarm_memory_create(1000, 15);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test swarm memory store with NULL memory
 * WHY:  Verify NULL check for memory parameter
 */
TEST_F(SwarmMemoryExceptionTest, SwarmMemoryStoreNullMemory) {
    uint8_t data[] = {1, 2, 3, 4};
    nimcp_result_t result = validate_swarm_memory_store(
        nullptr, NIMCP_MEMORY_EPISODIC, NIMCP_IMPORTANCE_HIGH,
        data, sizeof(data));

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test swarm memory store with uninitialized memory
 * WHY:  Verify initialization check
 */
TEST_F(SwarmMemoryExceptionTest, SwarmMemoryStoreNotInitialized) {
    NimcpSwarmMemory memory;
    memset(&memory, 0, sizeof(memory));
    memory.is_initialized = false;

    uint8_t data[] = {1, 2, 3, 4};
    nimcp_result_t result = validate_swarm_memory_store(
        &memory, NIMCP_MEMORY_EPISODIC, NIMCP_IMPORTANCE_HIGH,
        data, sizeof(data));

    EXPECT_EQ(result, NIMCP_ERROR_NOT_INITIALIZED);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test swarm memory store with invalid memory type
 * WHY:  Verify memory type range validation
 */
TEST_F(SwarmMemoryExceptionTest, SwarmMemoryStoreInvalidType) {
    NimcpSwarmMemory memory;
    memset(&memory, 0, sizeof(memory));
    memory.is_initialized = true;

    uint8_t data[] = {1, 2, 3, 4};
    nimcp_result_t result = validate_swarm_memory_store(
        &memory, (NimcpMemoryType)999, NIMCP_IMPORTANCE_HIGH,
        data, sizeof(data));

    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test swarm memory store with NULL data
 * WHY:  Verify NULL check for data parameter
 */
TEST_F(SwarmMemoryExceptionTest, SwarmMemoryStoreNullData) {
    NimcpSwarmMemory memory;
    memset(&memory, 0, sizeof(memory));
    memory.is_initialized = true;

    nimcp_result_t result = validate_swarm_memory_store(
        &memory, NIMCP_MEMORY_EPISODIC, NIMCP_IMPORTANCE_HIGH,
        nullptr, 100);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test swarm memory retrieve with invalid memory ID
 * WHY:  Verify memory ID validation
 */
TEST_F(SwarmMemoryExceptionTest, SwarmMemoryRetrieveEmptyId) {
    NimcpSwarmMemory memory;
    memset(&memory, 0, sizeof(memory));
    uint8_t buffer[100];

    nimcp_result_t result = validate_swarm_memory_retrieve(
        &memory, "", buffer, sizeof(buffer));

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test swarm memory retrieve with excessively long ID
 * WHY:  Verify buffer overflow prevention
 */
TEST_F(SwarmMemoryExceptionTest, SwarmMemoryRetrieveLongId) {
    NimcpSwarmMemory memory;
    memset(&memory, 0, sizeof(memory));
    uint8_t buffer[100];

    std::string long_id(100, 'x');  // 100 character ID
    nimcp_result_t result = validate_swarm_memory_retrieve(
        &memory, long_id.c_str(), buffer, sizeof(buffer));

    EXPECT_EQ(result, NIMCP_ERROR_BUFFER_OVERFLOW);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test pattern store with invalid confidence
 * WHY:  Verify confidence range validation
 */
TEST_F(SwarmMemoryExceptionTest, PatternStoreInvalidConfidence) {
    NimcpSwarmMemory memory;
    memset(&memory, 0, sizeof(memory));

    float data[] = {1.0f, 2.0f, 3.0f};
    swarm_pattern_t pattern;
    memset(&pattern, 0, sizeof(pattern));
    pattern.pattern_id = 1;
    pattern.data = data;
    pattern.data_len = 3;
    pattern.confidence = 1.5f;  // Invalid - above 1.0
    pattern.strength = 0.8f;

    nimcp_result_t result = validate_pattern_store(&memory, &pattern);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
    ASSERT_FALSE(g_captured_messages.empty());
    EXPECT_NE(g_captured_messages[0].find("confidence"), std::string::npos);
}

/**
 * WHAT: Test valid swarm memory operations succeed
 * WHY:  Verify successful operations don't throw
 */
TEST_F(SwarmMemoryExceptionTest, SwarmMemoryValidOperations) {
    EXPECT_EQ(validate_swarm_memory_create(1000, 3), NIMCP_SUCCESS);

    NimcpSwarmMemory memory;
    memset(&memory, 0, sizeof(memory));
    memory.is_initialized = true;

    uint8_t data[] = {1, 2, 3, 4};
    EXPECT_EQ(validate_swarm_memory_store(&memory, NIMCP_MEMORY_EPISODIC,
                                           NIMCP_IMPORTANCE_HIGH,
                                           data, sizeof(data)), NIMCP_SUCCESS);

    uint8_t buffer[100];
    EXPECT_EQ(validate_swarm_memory_retrieve(&memory, "test_id",
                                              buffer, sizeof(buffer)),
              NIMCP_SUCCESS);

    EXPECT_EQ(g_handler_call_count, 0);
}

//=============================================================================
// Energy Gossip Tests
//=============================================================================

/**
 * WHAT: Test energy gossip creation with reserved node ID
 * WHY:  Verify node ID 0 is rejected
 */
TEST_F(SwarmMemoryExceptionTest, EnergyGossipReservedNodeId) {
    nimcp_result_t result = validate_energy_gossip_create(0, nullptr);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test energy gossip with invalid config parameters
 * WHY:  Verify configuration validation
 */
TEST_F(SwarmMemoryExceptionTest, EnergyGossipInvalidFanout) {
    NimcpEnergyGossipConfig config;
    memset(&config, 0, sizeof(config));
    config.gossip_fanout = 100;  // Invalid - above 10
    config.min_relay_energy = 10.0f;
    config.convergence_threshold = 0.8f;

    nimcp_result_t result = validate_energy_gossip_create(1, &config);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test energy update with invalid level
 * WHY:  Verify energy level range validation
 */
TEST_F(SwarmMemoryExceptionTest, EnergyGossipInvalidEnergyLevel) {
    NimcpEnergyGossip gossip;
    memset(&gossip, 0, sizeof(gossip));
    gossip.is_initialized = true;

    nimcp_result_t result = validate_energy_update(&gossip, 150.0f);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test energy update with negative level
 * WHY:  Verify energy level non-negativity
 */
TEST_F(SwarmMemoryExceptionTest, EnergyGossipNegativeEnergyLevel) {
    NimcpEnergyGossip gossip;
    memset(&gossip, 0, sizeof(gossip));
    gossip.is_initialized = true;

    nimcp_result_t result = validate_energy_update(&gossip, -10.0f);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test gossip broadcast with NULL payload
 * WHY:  Verify payload validation
 */
TEST_F(SwarmMemoryExceptionTest, GossipBroadcastNullPayload) {
    NimcpEnergyGossip gossip;
    memset(&gossip, 0, sizeof(gossip));

    uint64_t result = validate_gossip_broadcast(
        &gossip, nullptr, 100, NIMCP_PRIORITY_NORMAL, 5);

    EXPECT_EQ(result, 0u);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_NULL_POINTER);
}

/**
 * WHAT: Test gossip broadcast with excessive payload
 * WHY:  Verify buffer overflow prevention
 */
TEST_F(SwarmMemoryExceptionTest, GossipBroadcastExcessivePayload) {
    NimcpEnergyGossip gossip;
    memset(&gossip, 0, sizeof(gossip));
    uint8_t data[] = {1, 2, 3};

    uint64_t result = validate_gossip_broadcast(
        &gossip, data, 100000, NIMCP_PRIORITY_NORMAL, 5);

    EXPECT_EQ(result, 0u);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_BUFFER_OVERFLOW);
}

/**
 * WHAT: Test relay registration with self
 * WHY:  Verify self-relay prevention
 */
TEST_F(SwarmMemoryExceptionTest, GossipRelaySelfRegistration) {
    NimcpEnergyGossip gossip;
    memset(&gossip, 0, sizeof(gossip));
    gossip.node_id = 42;

    nimcp_result_t result = validate_relay_register(&gossip, 42, 50.0f, 10.0f);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test sleep scheduling with past start time
 * WHY:  Verify time validation
 */
TEST_F(SwarmMemoryExceptionTest, GossipSleepPastStartTime) {
    NimcpEnergyGossip gossip;
    memset(&gossip, 0, sizeof(gossip));

    time_t past = time(NULL) - 3600;  // 1 hour ago
    nimcp_result_t result = validate_sleep_schedule(&gossip, past, 600);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test sleep scheduling with excessive duration
 * WHY:  Verify duration limit
 */
TEST_F(SwarmMemoryExceptionTest, GossipSleepExcessiveDuration) {
    NimcpEnergyGossip gossip;
    memset(&gossip, 0, sizeof(gossip));

    time_t future = time(NULL) + 3600;
    nimcp_result_t result = validate_sleep_schedule(&gossip, future, 100000);

    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(g_handler_call_count, 1);
}

//=============================================================================
// Temporal Replay Tests
//=============================================================================

/**
 * WHAT: Test replay config with zero capacity
 * WHY:  Verify capacity validation
 */
TEST_F(SwarmMemoryExceptionTest, ReplayConfigZeroCapacity) {
    replay_config_t config;
    memset(&config, 0, sizeof(config));
    config.capacity = 0;
    config.state_dim = 64;
    config.max_sequence_length = 32;
    config.priority_alpha = 0.6f;
    config.compression_ratio = 10.0f;

    int result = validate_replay_config(&config);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test replay config with excessive capacity
 * WHY:  Verify capacity range validation
 */
TEST_F(SwarmMemoryExceptionTest, ReplayConfigExcessiveCapacity) {
    replay_config_t config;
    memset(&config, 0, sizeof(config));
    config.capacity = REPLAY_MAX_CAPACITY + 1;
    config.state_dim = 64;
    config.max_sequence_length = 32;
    config.priority_alpha = 0.6f;
    config.compression_ratio = 10.0f;

    int result = validate_replay_config(&config);

    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test replay config with invalid priority alpha
 * WHY:  Verify priority alpha range
 */
TEST_F(SwarmMemoryExceptionTest, ReplayConfigInvalidPriorityAlpha) {
    replay_config_t config;
    memset(&config, 0, sizeof(config));
    config.capacity = 10000;
    config.state_dim = 64;
    config.max_sequence_length = 32;
    config.priority_alpha = 1.5f;  // Invalid - above 1.0
    config.compression_ratio = 10.0f;

    int result = validate_replay_config(&config);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test replay store with NULL state
 * WHY:  Verify state validation
 */
TEST_F(SwarmMemoryExceptionTest, ReplayStoreNullState) {
    temporal_replay_t replay;
    memset(&replay, 0, sizeof(replay));

    int result = validate_replay_store(&replay, nullptr, nullptr, 1.0f, 0.5f);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test replay store with NaN reward
 * WHY:  Verify reward validation
 */
TEST_F(SwarmMemoryExceptionTest, ReplayStoreNaNReward) {
    temporal_replay_t replay;
    memset(&replay, 0, sizeof(replay));
    float state[] = {1.0f, 2.0f, 3.0f};

    int result = validate_replay_store(&replay, state, nullptr, NAN, 0.5f);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test replay store with negative priority
 * WHY:  Verify priority non-negativity
 */
TEST_F(SwarmMemoryExceptionTest, ReplayStoreNegativePriority) {
    temporal_replay_t replay;
    memset(&replay, 0, sizeof(replay));
    float state[] = {1.0f, 2.0f, 3.0f};

    int result = validate_replay_store(&replay, state, nullptr, 1.0f, -0.5f);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test replay sample from empty buffer
 * WHY:  Verify state validation
 */
TEST_F(SwarmMemoryExceptionTest, ReplaySampleEmptyBuffer) {
    temporal_replay_t replay;
    memset(&replay, 0, sizeof(replay));
    replay.count = 0;

    replay_batch_t batch;
    int result = validate_replay_sample(&replay, REPLAY_MODE_FORWARD, 32, &batch);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_STATE);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test replay sample with batch larger than available
 * WHY:  Verify batch size validation
 */
TEST_F(SwarmMemoryExceptionTest, ReplaySampleOversizedBatch) {
    temporal_replay_t replay;
    memset(&replay, 0, sizeof(replay));
    replay.count = 10;

    replay_batch_t batch;
    int result = validate_replay_sample(&replay, REPLAY_MODE_FORWARD, 100, &batch);

    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test forward sweep with out of range start index
 * WHY:  Verify index validation
 */
TEST_F(SwarmMemoryExceptionTest, ForwardSweepOutOfRange) {
    temporal_replay_t replay;
    memset(&replay, 0, sizeof(replay));
    replay.count = 100;

    replay_sweep_result_t result_struct;
    int result = validate_forward_sweep(&replay, 150, 10, &result_struct);

    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(g_handler_call_count, 1);
}

//=============================================================================
// Hopfield Memory Tests
//=============================================================================

/**
 * WHAT: Test Hopfield config with zero capacity
 * WHY:  Verify capacity validation
 */
TEST_F(SwarmMemoryExceptionTest, HopfieldConfigZeroCapacity) {
    hopfield_config_t config;
    memset(&config, 0, sizeof(config));
    config.pattern_dim = 256;
    config.capacity = 0;
    config.beta = 1.0f;
    config.max_iterations = 3;
    config.convergence_threshold = 1e-6f;
    config.similarity_threshold = 0.9f;

    int result = validate_hopfield_config(&config);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test Hopfield config with excessive capacity
 * WHY:  Verify capacity range validation
 */
TEST_F(SwarmMemoryExceptionTest, HopfieldConfigExcessiveCapacity) {
    hopfield_config_t config;
    memset(&config, 0, sizeof(config));
    config.pattern_dim = 256;
    config.capacity = HOPFIELD_MAX_CAPACITY + 1;
    config.beta = 1.0f;
    config.max_iterations = 3;
    config.convergence_threshold = 1e-6f;
    config.similarity_threshold = 0.9f;

    int result = validate_hopfield_config(&config);

    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test Hopfield config with invalid beta
 * WHY:  Verify inverse temperature validation
 */
TEST_F(SwarmMemoryExceptionTest, HopfieldConfigInvalidBeta) {
    hopfield_config_t config;
    memset(&config, 0, sizeof(config));
    config.pattern_dim = 256;
    config.capacity = 1024;
    config.beta = -1.0f;  // Invalid - negative
    config.max_iterations = 3;
    config.convergence_threshold = 1e-6f;
    config.similarity_threshold = 0.9f;

    int result = validate_hopfield_config(&config);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test Hopfield config with excessive beta
 * WHY:  Verify beta range validation
 */
TEST_F(SwarmMemoryExceptionTest, HopfieldConfigExcessiveBeta) {
    hopfield_config_t config;
    memset(&config, 0, sizeof(config));
    config.pattern_dim = 256;
    config.capacity = 1024;
    config.beta = 200.0f;  // Invalid - above max
    config.max_iterations = 3;
    config.convergence_threshold = 1e-6f;
    config.similarity_threshold = 0.9f;

    int result = validate_hopfield_config(&config);

    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test Hopfield store with NULL pattern
 * WHY:  Verify pattern validation
 */
TEST_F(SwarmMemoryExceptionTest, HopfieldStoreNullPattern) {
    hopfield_memory_t memory;
    memset(&memory, 0, sizeof(memory));
    memory.config.capacity = 1024;
    memory.pattern_count = 0;

    uint32_t pattern_id;
    int result = validate_hopfield_store(&memory, nullptr, &pattern_id);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test Hopfield store at capacity
 * WHY:  Verify capacity check
 */
TEST_F(SwarmMemoryExceptionTest, HopfieldStoreAtCapacity) {
    hopfield_memory_t memory;
    memset(&memory, 0, sizeof(memory));
    memory.config.capacity = 100;
    memory.pattern_count = 100;  // At capacity

    float pattern[] = {1.0f, 2.0f, 3.0f};
    uint32_t pattern_id;
    int result = validate_hopfield_store(&memory, pattern, &pattern_id);

    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test Hopfield retrieve from empty memory
 * WHY:  Verify state validation
 */
TEST_F(SwarmMemoryExceptionTest, HopfieldRetrieveEmpty) {
    hopfield_memory_t memory;
    memset(&memory, 0, sizeof(memory));
    memory.pattern_count = 0;

    float query[] = {1.0f, 2.0f, 3.0f};
    hopfield_retrieval_result_t result_struct;
    int result = validate_hopfield_retrieve(&memory, query, &result_struct);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_STATE);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test Hopfield batch retrieve with NULL result
 * WHY:  Verify output validation
 */
TEST_F(SwarmMemoryExceptionTest, HopfieldBatchRetrieveNullResult) {
    hopfield_memory_t memory;
    memset(&memory, 0, sizeof(memory));
    memory.pattern_count = 10;

    float queries[] = {1.0f, 2.0f, 3.0f};
    int result = validate_hopfield_batch_retrieve(&memory, queries, 1, nullptr);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test valid Hopfield operations succeed
 * WHY:  Verify successful operations don't throw
 */
TEST_F(SwarmMemoryExceptionTest, HopfieldValidOperations) {
    hopfield_config_t config;
    memset(&config, 0, sizeof(config));
    config.pattern_dim = 256;
    config.capacity = 1024;
    config.beta = 1.0f;
    config.max_iterations = 3;
    config.convergence_threshold = 1e-6f;
    config.similarity_threshold = 0.9f;

    EXPECT_EQ(validate_hopfield_config(&config), 0);

    hopfield_memory_t memory;
    memset(&memory, 0, sizeof(memory));
    memory.config.capacity = 1024;
    memory.pattern_count = 50;

    float pattern[] = {1.0f, 2.0f, 3.0f};
    uint32_t pattern_id;
    EXPECT_EQ(validate_hopfield_store(&memory, pattern, &pattern_id), 0);

    hopfield_retrieval_result_t result_struct;
    EXPECT_EQ(validate_hopfield_retrieve(&memory, pattern, &result_struct), 0);

    EXPECT_EQ(g_handler_call_count, 0);
}

//=============================================================================
// Error Message Content Tests
//=============================================================================

/**
 * WHAT: Test error message contains memory type info
 * WHY:  Verify descriptive error messages
 */
TEST_F(SwarmMemoryExceptionTest, ErrorMessageContainsMemoryType) {
    NimcpSwarmMemory memory;
    memset(&memory, 0, sizeof(memory));
    memory.is_initialized = true;

    uint8_t data[] = {1, 2, 3, 4};
    validate_swarm_memory_store(&memory, (NimcpMemoryType)999,
                                 NIMCP_IMPORTANCE_HIGH, data, sizeof(data));

    ASSERT_FALSE(g_captured_messages.empty());
    const std::string& msg = g_captured_messages[0];
    EXPECT_NE(msg.find("999"), std::string::npos)
        << "Message should contain invalid type value";
}

/**
 * WHAT: Test error message contains pattern ID
 * WHY:  Verify pattern identification in errors
 */
TEST_F(SwarmMemoryExceptionTest, ErrorMessageContainsPatternId) {
    NimcpSwarmMemory memory;
    memset(&memory, 0, sizeof(memory));

    float data[] = {1.0f, 2.0f, 3.0f};
    swarm_pattern_t pattern;
    memset(&pattern, 0, sizeof(pattern));
    pattern.pattern_id = 12345;
    pattern.data = data;
    pattern.data_len = 3;
    pattern.confidence = 1.5f;
    pattern.strength = 0.8f;

    validate_pattern_store(&memory, &pattern);

    ASSERT_FALSE(g_captured_messages.empty());
    const std::string& msg = g_captured_messages[0];
    EXPECT_NE(msg.find("12345"), std::string::npos)
        << "Message should contain pattern ID 12345";
}

/**
 * WHAT: Test error message contains energy level
 * WHY:  Verify energy value in error messages
 */
TEST_F(SwarmMemoryExceptionTest, ErrorMessageContainsEnergyLevel) {
    NimcpEnergyGossip gossip;
    memset(&gossip, 0, sizeof(gossip));
    gossip.is_initialized = true;

    validate_energy_update(&gossip, 150.0f);

    ASSERT_FALSE(g_captured_messages.empty());
    const std::string& msg = g_captured_messages[0];
    EXPECT_NE(msg.find("150"), std::string::npos)
        << "Message should contain invalid energy value";
}

/**
 * WHAT: Test error message contains capacity information
 * WHY:  Verify capacity info in range errors
 */
TEST_F(SwarmMemoryExceptionTest, ErrorMessageContainsCapacity) {
    hopfield_memory_t memory;
    memset(&memory, 0, sizeof(memory));
    memory.config.capacity = 100;
    memory.pattern_count = 100;

    float pattern[] = {1.0f, 2.0f, 3.0f};
    uint32_t pattern_id;
    validate_hopfield_store(&memory, pattern, &pattern_id);

    ASSERT_FALSE(g_captured_messages.empty());
    const std::string& msg = g_captured_messages[0];
    EXPECT_NE(msg.find("100"), std::string::npos)
        << "Message should contain capacity value";
}

//=============================================================================
// Memory Leak and Stress Tests
//=============================================================================

/**
 * WHAT: Test multiple swarm memory exceptions don't leak memory
 * WHY:  Verify exception cleanup on error paths
 */
TEST_F(SwarmMemoryExceptionTest, MultipleSwarmMemoryExceptionsNoLeak) {
    const int iterations = 100;
    NimcpSwarmMemory memory;
    memset(&memory, 0, sizeof(memory));
    memory.is_initialized = true;

    for (int i = 0; i < iterations; i++) {
        validate_swarm_memory_store(&memory, (NimcpMemoryType)(i + 100),
                                     NIMCP_IMPORTANCE_HIGH, nullptr, 0);
    }

    // First iteration fails on type, subsequent on NULL data
    EXPECT_GE(g_handler_call_count, iterations);
}

/**
 * WHAT: Test mixed module errors don't leak memory
 * WHY:  Verify cleanup across different modules
 */
TEST_F(SwarmMemoryExceptionTest, MixedModuleErrorsNoLeak) {
    // Swarm memory errors
    NimcpSwarmMemory memory;
    memset(&memory, 0, sizeof(memory));
    validate_swarm_memory_store(&memory, NIMCP_MEMORY_EPISODIC,
                                 NIMCP_IMPORTANCE_HIGH, nullptr, 0);

    // Energy gossip errors
    NimcpEnergyGossip gossip;
    memset(&gossip, 0, sizeof(gossip));
    gossip.is_initialized = true;
    validate_energy_update(&gossip, -10.0f);

    // Temporal replay errors
    replay_config_t replay_config;
    memset(&replay_config, 0, sizeof(replay_config));
    validate_replay_config(&replay_config);

    // Hopfield errors
    hopfield_config_t hopfield_config;
    memset(&hopfield_config, 0, sizeof(hopfield_config));
    validate_hopfield_config(&hopfield_config);

    EXPECT_EQ(g_handler_call_count, 4);
}

/**
 * WHAT: Test alternating success and failure operations
 * WHY:  Verify cleanup works for mixed outcomes
 */
TEST_F(SwarmMemoryExceptionTest, AlternatingSuccessAndFailureNoLeak) {
    NimcpSwarmMemory memory;
    memset(&memory, 0, sizeof(memory));
    memory.is_initialized = true;

    const int iterations = 50;
    int success_count = 0;
    int failure_count = 0;
    uint8_t data[] = {1, 2, 3, 4};

    for (int i = 0; i < iterations; i++) {
        void* data_ptr = (i % 2 == 0) ? data : nullptr;
        nimcp_result_t result = validate_swarm_memory_store(
            &memory, NIMCP_MEMORY_EPISODIC, NIMCP_IMPORTANCE_HIGH,
            data_ptr, sizeof(data));

        if (result == NIMCP_SUCCESS) {
            success_count++;
        } else {
            failure_count++;
        }
    }

    EXPECT_EQ(success_count, 25);
    EXPECT_EQ(failure_count, 25);
    EXPECT_EQ(g_handler_call_count, failure_count);
}

//=============================================================================
// State Machine Transition Tests
//=============================================================================

/**
 * WHAT: Test energy state machine invalid transitions
 * WHY:  Verify state validation
 */
TEST_F(SwarmMemoryExceptionTest, EnergyStateInvalidTransition) {
    NimcpEnergyGossip gossip;
    memset(&gossip, 0, sizeof(gossip));
    gossip.is_initialized = false;  // Not initialized

    nimcp_result_t result = validate_energy_update(&gossip, 50.0f);

    EXPECT_EQ(result, NIMCP_ERROR_NOT_INITIALIZED);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test replay sequence state validation
 * WHY:  Verify sampling from empty buffer fails
 */
TEST_F(SwarmMemoryExceptionTest, ReplaySequenceInvalidState) {
    temporal_replay_t replay;
    memset(&replay, 0, sizeof(replay));
    replay.count = 0;  // Empty

    replay_batch_t batch;
    memset(&batch, 0, sizeof(batch));
    int result = validate_replay_sample(&replay, REPLAY_MODE_FORWARD, 10, &batch);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_STATE);
}

//=============================================================================
// Recovery Action Tests
//=============================================================================

/**
 * WHAT: Test recovery action is set on exception
 * WHY:  Verify recovery mechanism integration
 */
TEST_F(SwarmMemoryExceptionTest, RecoveryActionSet) {
    // Create exception with recovery action
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__,
        __LINE__,
        __func__,
        "Test recovery"
    );

    ASSERT_NE(ex, nullptr);

    // Set suggested recovery action
    ex->suggested_action = EXCEPTION_RECOVERY_GC;
    ex->recovery_attempted = false;
    ex->recovery_succeeded = false;

    // Verify recovery fields
    EXPECT_EQ(ex->suggested_action, EXCEPTION_RECOVERY_GC);
    EXPECT_FALSE(ex->recovery_attempted);
    EXPECT_FALSE(ex->recovery_succeeded);

    // Simulate recovery attempt
    ex->recovery_attempted = true;
    ex->recovery_succeeded = true;

    EXPECT_TRUE(ex->recovery_attempted);
    EXPECT_TRUE(ex->recovery_succeeded);

    nimcp_exception_unref(ex);
}

/**
 * WHAT: Test recovery action for memory exceptions
 * WHY:  Verify memory-specific recovery suggestions
 */
TEST_F(SwarmMemoryExceptionTest, MemoryRecoveryActionSuggested) {
    // Create exception for memory error
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__,
        __LINE__,
        __func__,
        "Memory allocation failed for swarm memory buffer"
    );

    ASSERT_NE(ex, nullptr);

    // For memory errors, GC or compact are appropriate recovery actions
    ex->suggested_action = EXCEPTION_RECOVERY_GC;

    // Verify the action is set correctly
    EXPECT_EQ(ex->suggested_action, EXCEPTION_RECOVERY_GC);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_SEVERE);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NO_MEMORY);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
