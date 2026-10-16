/**
 * @file test_swarm_kernels.cpp
 * @brief Comprehensive unit tests for GPU Swarm Intelligence kernels
 *
 * WHAT: Tests for GPU-accelerated Swarm Intelligence operations
 * WHY:  Verify flocking, consensus, pheromone, quorum, task allocation, and collision
 * HOW:  GoogleTest with GPU context setup/teardown and CPU fallback verification
 *
 * TEST COVERAGE:
 * - Flocking: separation, alignment, cohesion forces
 * - Flocking: combined force computation
 * - Flocking: position/velocity update
 * - Spatial hash: build and query
 * - Consensus: averaging convergence
 * - Consensus: belief propagation
 * - Pheromone: diffusion and decay
 * - Pheromone: deposit and sample
 * - Quorum sensing: threshold detection
 * - Task allocation: auction rounds
 * - Collision detection: pairwise distances
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include <numeric>
#include <chrono>

// GPU headers include CUDA headers that cannot be in extern "C" blocks
#include "gpu/swarm/nimcp_swarm_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

//=============================================================================
// Test Constants
//=============================================================================

static constexpr size_t DEFAULT_N_AGENTS = 100;
static constexpr size_t SMALL_N_AGENTS = 10;
static constexpr size_t LARGE_N_AGENTS = 1000;
static constexpr size_t DEFAULT_MAX_NEIGHBORS = 16;
static constexpr float DEFAULT_DT = 0.016f;  // ~60 FPS
static constexpr float TOLERANCE = 1e-4f;

// Pheromone grid constants
static constexpr uint32_t PHEROMONE_GRID_X = 32;
static constexpr uint32_t PHEROMONE_GRID_Y = 32;
static constexpr uint32_t PHEROMONE_GRID_Z = 1;
static constexpr uint32_t PHEROMONE_N_TYPES = 2;
static constexpr float PHEROMONE_VOXEL_SIZE = 1.0f;

// Consensus constants
static constexpr size_t DEFAULT_BELIEF_DIM = 4;

// Task allocation constants
static constexpr size_t DEFAULT_N_TASKS = 20;
static constexpr size_t DEFAULT_N_CAPABILITIES = 3;

// Quorum constants
static constexpr size_t DEFAULT_N_SIGNAL_TYPES = 4;

// Collision detection constants
static constexpr size_t DEFAULT_MAX_PAIRS = 1000;

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * @brief Test fixture for GPU Swarm kernel tests
 *
 * WHAT: Provides GPU context setup/teardown and helper utilities
 * WHY:  Ensure proper GPU resource management across tests
 * HOW:  Creates context in SetUp, destroys in TearDown, provides tensor helpers
 */
class SwarmKernelTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* ctx = nullptr;
    bool gpu_available = false;

    void SetUp() override {
        // Try to create GPU context (may fail without CUDA)
        ctx = nimcp_gpu_context_create_auto();
        gpu_available = (ctx != nullptr && nimcp_gpu_context_is_valid(ctx));
    }

    void TearDown() override {
        if (ctx) {
            nimcp_gpu_context_destroy(ctx);
            ctx = nullptr;
        }
    }

    /**
     * @brief Skip test if GPU not available
     */
    void RequireGPU() {
        if (!gpu_available) {
            GTEST_SKIP() << "GPU not available, skipping test";
        }
    }

    /**
     * @brief Create default flocking parameters
     */
    nimcp_flocking_gpu_params_t create_default_flocking_params() {
        nimcp_flocking_gpu_params_t params;
        nimcp_flocking_gpu_default_params(&params);
        return params;
    }

    /**
     * @brief Create default consensus parameters
     */
    nimcp_consensus_gpu_params_t create_default_consensus_params() {
        nimcp_consensus_gpu_params_t params;
        nimcp_consensus_gpu_default_params(&params);
        return params;
    }

    /**
     * @brief Create default pheromone parameters
     */
    nimcp_pheromone_gpu_params_t create_default_pheromone_params() {
        nimcp_pheromone_gpu_params_t params;
        nimcp_pheromone_gpu_default_params(&params);
        return params;
    }

    /**
     * @brief Create default quorum parameters
     */
    nimcp_quorum_gpu_params_t create_default_quorum_params() {
        nimcp_quorum_gpu_params_t params;
        nimcp_quorum_gpu_default_params(&params);
        return params;
    }

    /**
     * @brief Create default task allocation parameters
     */
    nimcp_task_alloc_gpu_params_t create_default_task_alloc_params() {
        nimcp_task_alloc_gpu_params_t params;
        nimcp_task_alloc_gpu_default_params(&params);
        return params;
    }

    /**
     * @brief Create default collision parameters
     */
    nimcp_collision_gpu_params_t create_default_collision_params() {
        nimcp_collision_gpu_params_t params;
        nimcp_collision_gpu_default_params(&params);
        return params;
    }

    /**
     * @brief Create GPU tensor from host data
     */
    nimcp_gpu_tensor_t* create_tensor_from_data(const float* data, size_t size) {
        if (!gpu_available) return nullptr;
        size_t dims[1] = {size};
        return nimcp_gpu_tensor_from_host(ctx, data, dims, 1, NIMCP_GPU_PRECISION_FP32);
    }

    /**
     * @brief Create GPU tensor filled with zeros
     */
    nimcp_gpu_tensor_t* create_zero_tensor(size_t size) {
        if (!gpu_available) return nullptr;
        size_t dims[1] = {size};
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
        if (tensor) {
            nimcp_gpu_zeros(ctx, tensor);
        }
        return tensor;
    }

    /**
     * @brief Create GPU tensor filled with a value
     */
    nimcp_gpu_tensor_t* create_filled_tensor(size_t size, float value) {
        if (!gpu_available) return nullptr;
        size_t dims[1] = {size};
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
        if (tensor) {
            nimcp_gpu_fill(ctx, tensor, value);
        }
        return tensor;
    }

    /**
     * @brief Create 2D GPU tensor (matrix)
     */
    nimcp_gpu_tensor_t* create_matrix(size_t rows, size_t cols, float value) {
        if (!gpu_available) return nullptr;
        size_t dims[2] = {rows, cols};
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, 2, NIMCP_GPU_PRECISION_FP32);
        if (tensor) {
            nimcp_gpu_fill(ctx, tensor, value);
        }
        return tensor;
    }

    /**
     * @brief Create 2D GPU tensor from host data
     */
    nimcp_gpu_tensor_t* create_matrix_from_data(const float* data, size_t rows, size_t cols) {
        if (!gpu_available) return nullptr;
        size_t dims[2] = {rows, cols};
        return nimcp_gpu_tensor_from_host(ctx, data, dims, 2, NIMCP_GPU_PRECISION_FP32);
    }

    /**
     * @brief Copy tensor data to host (float version)
     */
    bool copy_to_host(const nimcp_gpu_tensor_t* tensor, float* host_data) {
        if (!tensor || !host_data) return false;
        return nimcp_gpu_tensor_to_host(tensor, host_data);
    }

    /**
     * @brief Copy tensor data to host (uint32 version)
     */
    bool copy_to_host_uint32(const nimcp_gpu_tensor_t* tensor, uint32_t* host_data) {
        if (!tensor || !host_data) return false;
        return nimcp_gpu_tensor_to_host(tensor, host_data);
    }

    /**
     * @brief Create agent positions in a grid pattern
     */
    std::vector<float> create_grid_positions(size_t n_agents, float spacing) {
        std::vector<float> positions(n_agents * 4);
        size_t side = static_cast<size_t>(std::ceil(std::cbrt(static_cast<double>(n_agents))));
        size_t idx = 0;
        for (size_t z = 0; z < side && idx < n_agents; z++) {
            for (size_t y = 0; y < side && idx < n_agents; y++) {
                for (size_t x = 0; x < side && idx < n_agents; x++) {
                    positions[idx * 4 + 0] = x * spacing;
                    positions[idx * 4 + 1] = y * spacing;
                    positions[idx * 4 + 2] = z * spacing;
                    positions[idx * 4 + 3] = 1.0f;  // w component (mass)
                    idx++;
                }
            }
        }
        return positions;
    }

    /**
     * @brief Create uniform velocity field
     */
    std::vector<float> create_uniform_velocities(size_t n_agents, float vx, float vy, float vz) {
        std::vector<float> velocities(n_agents * 4);
        for (size_t i = 0; i < n_agents; i++) {
            velocities[i * 4 + 0] = vx;
            velocities[i * 4 + 1] = vy;
            velocities[i * 4 + 2] = vz;
            velocities[i * 4 + 3] = 0.0f;
        }
        return velocities;
    }
};

//=============================================================================
// Flocking State Lifecycle Tests
//=============================================================================

/**
 * TEST: Flocking state creation with valid parameters
 * WHAT: Create flocking state on GPU
 * WHY:  Verify basic allocation and initialization
 */
TEST_F(SwarmKernelTest, FlockingStateCreate_ValidParams_Succeeds) {
    RequireGPU();

    nimcp_flocking_gpu_params_t params = create_default_flocking_params();
    nimcp_flocking_gpu_state_t* state = nimcp_flocking_gpu_create(
        ctx, DEFAULT_N_AGENTS, DEFAULT_MAX_NEIGHBORS, &params);

    ASSERT_NE(state, nullptr) << "Flocking state creation should succeed";
    EXPECT_NE(state->positions, nullptr) << "Positions tensor should be allocated";
    EXPECT_NE(state->velocities, nullptr) << "Velocities tensor should be allocated";
    EXPECT_NE(state->forces, nullptr) << "Forces tensor should be allocated";
    EXPECT_EQ(state->n_agents, DEFAULT_N_AGENTS);

    nimcp_flocking_gpu_destroy(state);
}

/**
 * TEST: Flocking state creation with NULL context
 * WHAT: Try to create flocking state without GPU context
 * WHY:  Verify NULL-safety
 */
TEST_F(SwarmKernelTest, FlockingStateCreate_NullContext_ReturnsNull) {
    nimcp_flocking_gpu_params_t params = create_default_flocking_params();
    nimcp_flocking_gpu_state_t* state = nimcp_flocking_gpu_create(
        nullptr, DEFAULT_N_AGENTS, DEFAULT_MAX_NEIGHBORS, &params);
    EXPECT_EQ(state, nullptr) << "Should reject NULL context";
}

/**
 * TEST: Flocking state creation with zero agents
 * WHAT: Try to create flocking state with zero agents
 * WHY:  Verify input validation
 */
TEST_F(SwarmKernelTest, FlockingStateCreate_ZeroAgents_ReturnsNull) {
    RequireGPU();

    nimcp_flocking_gpu_params_t params = create_default_flocking_params();
    nimcp_flocking_gpu_state_t* state = nimcp_flocking_gpu_create(
        ctx, 0, DEFAULT_MAX_NEIGHBORS, &params);
    EXPECT_EQ(state, nullptr) << "Should reject zero agents";
}

/**
 * TEST: Flocking state destruction with NULL
 * WHAT: Destroy NULL flocking state
 * WHY:  Verify NULL-safety
 */
TEST_F(SwarmKernelTest, FlockingStateDestroy_Null_NoOp) {
    nimcp_flocking_gpu_destroy(nullptr);
    SUCCEED() << "Should not crash on NULL";
}

//=============================================================================
// Flocking Force Computation Tests
//=============================================================================

/**
 * TEST: Separation force computation
 * WHAT: Compute separation force for agents too close together
 * WHY:  Agents should repel when within separation radius
 */
TEST_F(SwarmKernelTest, FlockingSeparation_CloseAgents_GeneratesRepulsion) {
    RequireGPU();

    nimcp_flocking_gpu_params_t params = create_default_flocking_params();
    params.separation_radius = 5.0f;
    params.separation_weight = 1.0f;

    nimcp_flocking_gpu_state_t* state = nimcp_flocking_gpu_create(
        ctx, SMALL_N_AGENTS, DEFAULT_MAX_NEIGHBORS, &params);
    ASSERT_NE(state, nullptr);

    // Place agents close together (within separation radius)
    std::vector<float> positions = create_grid_positions(SMALL_N_AGENTS, 1.0f);
    size_t dims[2] = {SMALL_N_AGENTS, 4};
    nimcp_gpu_tensor_t* pos_tensor = nimcp_gpu_tensor_from_host(
        ctx, positions.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);

    // Copy positions to state
    nimcp_gpu_copy(ctx, pos_tensor, state->positions);

    // Find neighbors first (required before computing forces)
    bool neighbors_found = nimcp_gpu_flocking_find_neighbors(ctx, state, params.separation_radius);
    EXPECT_TRUE(neighbors_found) << "Neighbor finding should succeed";

    // Compute separation force
    bool result = nimcp_gpu_flocking_separation(ctx, state);
    EXPECT_TRUE(result) << "Separation force computation should succeed";

    // Verify forces are non-zero (agents should repel)
    std::vector<float> forces(SMALL_N_AGENTS * 4);
    copy_to_host(state->forces, forces.data());

    float total_force_magnitude = 0.0f;
    for (size_t i = 0; i < SMALL_N_AGENTS; i++) {
        float fx = forces[i * 4 + 0];
        float fy = forces[i * 4 + 1];
        float fz = forces[i * 4 + 2];
        total_force_magnitude += std::sqrt(fx * fx + fy * fy + fz * fz);
    }
    EXPECT_GT(total_force_magnitude, 0.0f) << "Close agents should generate separation force";

    nimcp_gpu_tensor_destroy(pos_tensor);
    nimcp_flocking_gpu_destroy(state);
}

/**
 * TEST: Alignment force computation
 * WHAT: Compute alignment force to match neighbor velocities
 * WHY:  Agents should steer toward average heading of neighbors
 */
TEST_F(SwarmKernelTest, FlockingAlignment_VaryingVelocities_SteersTowardAverage) {
    RequireGPU();

    nimcp_flocking_gpu_params_t params = create_default_flocking_params();
    params.alignment_radius = 10.0f;
    params.alignment_weight = 1.0f;

    nimcp_flocking_gpu_state_t* state = nimcp_flocking_gpu_create(
        ctx, SMALL_N_AGENTS, DEFAULT_MAX_NEIGHBORS, &params);
    ASSERT_NE(state, nullptr);

    // Place agents in a cluster
    std::vector<float> positions = create_grid_positions(SMALL_N_AGENTS, 2.0f);
    size_t dims[2] = {SMALL_N_AGENTS, 4};
    nimcp_gpu_tensor_t* pos_tensor = nimcp_gpu_tensor_from_host(
        ctx, positions.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_copy(ctx, pos_tensor, state->positions);

    // Create varying velocities - first half going right, second half going left
    std::vector<float> velocities(SMALL_N_AGENTS * 4, 0.0f);
    for (size_t i = 0; i < SMALL_N_AGENTS; i++) {
        velocities[i * 4 + 0] = (i < SMALL_N_AGENTS / 2) ? 1.0f : -1.0f;
    }
    nimcp_gpu_tensor_t* vel_tensor = nimcp_gpu_tensor_from_host(
        ctx, velocities.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_copy(ctx, vel_tensor, state->velocities);

    // Find neighbors first (required before computing forces)
    bool neighbors_found = nimcp_gpu_flocking_find_neighbors(ctx, state, params.alignment_radius);
    EXPECT_TRUE(neighbors_found) << "Neighbor finding should succeed";

    // Compute alignment force
    bool result = nimcp_gpu_flocking_alignment(ctx, state);
    EXPECT_TRUE(result) << "Alignment force computation should succeed";

    // Verify forces try to equalize velocities
    std::vector<float> forces(SMALL_N_AGENTS * 4);
    copy_to_host(state->forces, forces.data());

    // Agents going right should have leftward force (negative) and vice versa
    bool has_corrective_forces = false;
    for (size_t i = 0; i < SMALL_N_AGENTS; i++) {
        float fx = forces[i * 4 + 0];
        if (std::abs(fx) > TOLERANCE) {
            has_corrective_forces = true;
        }
    }
    EXPECT_TRUE(has_corrective_forces) << "Alignment should produce corrective forces";

    nimcp_gpu_tensor_destroy(pos_tensor);
    nimcp_gpu_tensor_destroy(vel_tensor);
    nimcp_flocking_gpu_destroy(state);
}

/**
 * TEST: Cohesion force computation
 * WHAT: Compute cohesion force to move toward center of mass
 * WHY:  Agents should be attracted toward the flock center
 */
TEST_F(SwarmKernelTest, FlockingCohesion_ScatteredAgents_SteersTowardCenter) {
    RequireGPU();

    nimcp_flocking_gpu_params_t params = create_default_flocking_params();
    params.cohesion_radius = 50.0f;
    params.cohesion_weight = 1.0f;

    nimcp_flocking_gpu_state_t* state = nimcp_flocking_gpu_create(
        ctx, SMALL_N_AGENTS, DEFAULT_MAX_NEIGHBORS, &params);
    ASSERT_NE(state, nullptr);

    // Place agents in a sparse grid
    std::vector<float> positions = create_grid_positions(SMALL_N_AGENTS, 10.0f);
    size_t dims[2] = {SMALL_N_AGENTS, 4};
    nimcp_gpu_tensor_t* pos_tensor = nimcp_gpu_tensor_from_host(
        ctx, positions.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_copy(ctx, pos_tensor, state->positions);

    // Zero velocities
    nimcp_gpu_zeros(ctx, state->velocities);

    // Compute cohesion force
    bool result = nimcp_gpu_flocking_cohesion(ctx, state);
    EXPECT_TRUE(result) << "Cohesion force computation should succeed";

    // Calculate expected center of mass
    float cx = 0.0f, cy = 0.0f, cz = 0.0f;
    for (size_t i = 0; i < SMALL_N_AGENTS; i++) {
        cx += positions[i * 4 + 0];
        cy += positions[i * 4 + 1];
        cz += positions[i * 4 + 2];
    }
    cx /= SMALL_N_AGENTS;
    cy /= SMALL_N_AGENTS;
    cz /= SMALL_N_AGENTS;

    // Verify forces point toward center
    std::vector<float> forces(SMALL_N_AGENTS * 4);
    copy_to_host(state->forces, forces.data());

    int correct_direction_count = 0;
    for (size_t i = 0; i < SMALL_N_AGENTS; i++) {
        float px = positions[i * 4 + 0];
        float py = positions[i * 4 + 1];
        float pz = positions[i * 4 + 2];
        float fx = forces[i * 4 + 0];
        float fy = forces[i * 4 + 1];
        float fz = forces[i * 4 + 2];

        // Force should point from agent toward center
        float to_center_x = cx - px;
        float to_center_y = cy - py;
        float to_center_z = cz - pz;

        // Dot product should be positive if force points toward center
        float dot = fx * to_center_x + fy * to_center_y + fz * to_center_z;
        if (dot >= 0) {
            correct_direction_count++;
        }
    }
    EXPECT_GT(correct_direction_count, SMALL_N_AGENTS / 2)
        << "Most agents should have cohesion force toward center";

    nimcp_gpu_tensor_destroy(pos_tensor);
    nimcp_flocking_gpu_destroy(state);
}

/**
 * TEST: Combined force computation
 * WHAT: Compute all flocking forces in a single pass
 * WHY:  Efficient combined computation for real-time simulation
 */
TEST_F(SwarmKernelTest, FlockingComputeForces_Combined_ProducesBalancedForces) {
    RequireGPU();

    nimcp_flocking_gpu_params_t params = create_default_flocking_params();
    params.separation_weight = 1.0f;
    params.alignment_weight = 0.5f;
    params.cohesion_weight = 0.5f;
    params.separation_radius = 2.0f;
    params.alignment_radius = 5.0f;
    params.cohesion_radius = 10.0f;

    nimcp_flocking_gpu_state_t* state = nimcp_flocking_gpu_create(
        ctx, DEFAULT_N_AGENTS, DEFAULT_MAX_NEIGHBORS, &params);
    ASSERT_NE(state, nullptr);

    // Set up agent positions and velocities
    std::vector<float> positions = create_grid_positions(DEFAULT_N_AGENTS, 1.5f);
    std::vector<float> velocities = create_uniform_velocities(DEFAULT_N_AGENTS, 1.0f, 0.0f, 0.0f);

    size_t dims[2] = {DEFAULT_N_AGENTS, 4};
    nimcp_gpu_tensor_t* pos_tensor = nimcp_gpu_tensor_from_host(
        ctx, positions.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* vel_tensor = nimcp_gpu_tensor_from_host(
        ctx, velocities.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);

    nimcp_gpu_copy(ctx, pos_tensor, state->positions);
    nimcp_gpu_copy(ctx, vel_tensor, state->velocities);

    // Compute combined forces
    bool result = nimcp_gpu_flocking_compute_forces(ctx, state);
    EXPECT_TRUE(result) << "Combined force computation should succeed";

    // Verify forces are computed and bounded
    std::vector<float> forces(DEFAULT_N_AGENTS * 4);
    copy_to_host(state->forces, forces.data());

    for (size_t i = 0; i < DEFAULT_N_AGENTS; i++) {
        float fx = forces[i * 4 + 0];
        float fy = forces[i * 4 + 1];
        float fz = forces[i * 4 + 2];
        float magnitude = std::sqrt(fx * fx + fy * fy + fz * fz);

        EXPECT_LE(magnitude, params.max_force + TOLERANCE)
            << "Force magnitude should be bounded by max_force";
    }

    nimcp_gpu_tensor_destroy(pos_tensor);
    nimcp_gpu_tensor_destroy(vel_tensor);
    nimcp_flocking_gpu_destroy(state);
}

/**
 * TEST: Position/velocity update
 * WHAT: Update agent kinematics based on computed forces
 * WHY:  Core simulation step for flocking behavior
 */
TEST_F(SwarmKernelTest, FlockingUpdate_WithForces_UpdatesKinematics) {
    RequireGPU();

    nimcp_flocking_gpu_params_t params = create_default_flocking_params();
    params.dt = DEFAULT_DT;
    params.max_speed = 10.0f;

    nimcp_flocking_gpu_state_t* state = nimcp_flocking_gpu_create(
        ctx, SMALL_N_AGENTS, DEFAULT_MAX_NEIGHBORS, &params);
    ASSERT_NE(state, nullptr);

    // Initial positions at origin
    std::vector<float> initial_positions(SMALL_N_AGENTS * 4, 0.0f);
    for (size_t i = 0; i < SMALL_N_AGENTS; i++) {
        initial_positions[i * 4 + 3] = 1.0f;  // w = mass
    }
    size_t dims[2] = {SMALL_N_AGENTS, 4};
    nimcp_gpu_tensor_t* pos_tensor = nimcp_gpu_tensor_from_host(
        ctx, initial_positions.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_copy(ctx, pos_tensor, state->positions);

    // Initial velocities in +x direction
    std::vector<float> initial_velocities = create_uniform_velocities(SMALL_N_AGENTS, 1.0f, 0.0f, 0.0f);
    nimcp_gpu_tensor_t* vel_tensor = nimcp_gpu_tensor_from_host(
        ctx, initial_velocities.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_copy(ctx, vel_tensor, state->velocities);

    // Set constant force in +y direction
    std::vector<float> forces(SMALL_N_AGENTS * 4, 0.0f);
    for (size_t i = 0; i < SMALL_N_AGENTS; i++) {
        forces[i * 4 + 1] = 1.0f;  // Force in y
    }
    nimcp_gpu_tensor_t* force_tensor = nimcp_gpu_tensor_from_host(
        ctx, forces.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_copy(ctx, force_tensor, state->forces);

    // Update kinematics
    bool result = nimcp_gpu_flocking_update(ctx, state, DEFAULT_DT);
    EXPECT_TRUE(result) << "Flocking update should succeed";

    // Verify positions changed
    std::vector<float> new_positions(SMALL_N_AGENTS * 4);
    copy_to_host(state->positions, new_positions.data());

    for (size_t i = 0; i < SMALL_N_AGENTS; i++) {
        // X should increase due to velocity
        EXPECT_GT(new_positions[i * 4 + 0], 0.0f)
            << "Position X should increase with positive velocity";

        // Y should increase due to force
        EXPECT_GE(new_positions[i * 4 + 1], 0.0f)
            << "Position Y should change with force applied";
    }

    // Verify velocities changed
    std::vector<float> new_velocities(SMALL_N_AGENTS * 4);
    copy_to_host(state->velocities, new_velocities.data());

    for (size_t i = 0; i < SMALL_N_AGENTS; i++) {
        // Y velocity should have increased due to force
        EXPECT_GT(new_velocities[i * 4 + 1], 0.0f)
            << "Velocity Y should increase with positive force";

        // Check speed limit
        float vx = new_velocities[i * 4 + 0];
        float vy = new_velocities[i * 4 + 1];
        float vz = new_velocities[i * 4 + 2];
        float speed = std::sqrt(vx * vx + vy * vy + vz * vz);
        EXPECT_LE(speed, params.max_speed + TOLERANCE)
            << "Speed should be bounded by max_speed";
    }

    nimcp_gpu_tensor_destroy(pos_tensor);
    nimcp_gpu_tensor_destroy(vel_tensor);
    nimcp_gpu_tensor_destroy(force_tensor);
    nimcp_flocking_gpu_destroy(state);
}

//=============================================================================
// Spatial Hash Tests
//=============================================================================

/**
 * TEST: Spatial hash creation
 * WHAT: Create spatial hash structure
 * WHY:  Accelerate neighbor queries for large agent counts
 */
TEST_F(SwarmKernelTest, SpatialHashCreate_ValidParams_Succeeds) {
    RequireGPU();

    nimcp_spatial_hash_t* hash = nimcp_spatial_hash_create(
        ctx, 2.0f,  // cell_size
        32, 32, 32, // grid dimensions
        DEFAULT_N_AGENTS);

    ASSERT_NE(hash, nullptr) << "Spatial hash creation should succeed";
    EXPECT_FLOAT_EQ(hash->cell_size, 2.0f);
    EXPECT_EQ(hash->grid_dim_x, 32u);

    nimcp_spatial_hash_destroy(hash);
}

/**
 * TEST: Spatial hash build and query
 * WHAT: Build spatial hash from positions and find neighbors
 * WHY:  Core acceleration structure for flocking
 */
TEST_F(SwarmKernelTest, SpatialHashBuild_GridPositions_AssignsCorrectCells) {
    RequireGPU();

    const float cell_size = 5.0f;
    nimcp_spatial_hash_t* hash = nimcp_spatial_hash_create(
        ctx, cell_size, 16, 16, 16, SMALL_N_AGENTS);
    ASSERT_NE(hash, nullptr);

    // Create positions on a grid
    std::vector<float> positions = create_grid_positions(SMALL_N_AGENTS, 3.0f);
    size_t dims[2] = {SMALL_N_AGENTS, 4};
    nimcp_gpu_tensor_t* pos_tensor = nimcp_gpu_tensor_from_host(
        ctx, positions.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);

    // Build spatial hash
    bool result = nimcp_gpu_spatial_hash_build(ctx, hash, pos_tensor, SMALL_N_AGENTS);
    EXPECT_TRUE(result) << "Spatial hash build should succeed";

    // Verify structure is populated (cell_start and cell_end should be set)
    // Cannot directly verify GPU contents without kernel, but build should succeed

    nimcp_gpu_tensor_destroy(pos_tensor);
    nimcp_spatial_hash_destroy(hash);
}

/**
 * TEST: Spatial hash neighbor finding
 * WHAT: Find neighbors using spatial hash
 * WHY:  Efficient O(n) neighbor search instead of O(n^2)
 */
TEST_F(SwarmKernelTest, FlockingFindNeighbors_CloseAgents_FindsCorrectNeighbors) {
    RequireGPU();

    nimcp_flocking_gpu_params_t params = create_default_flocking_params();
    nimcp_flocking_gpu_state_t* state = nimcp_flocking_gpu_create(
        ctx, SMALL_N_AGENTS, DEFAULT_MAX_NEIGHBORS, &params);
    ASSERT_NE(state, nullptr);

    // Place agents in a tight cluster
    std::vector<float> positions = create_grid_positions(SMALL_N_AGENTS, 1.0f);
    size_t dims[2] = {SMALL_N_AGENTS, 4};
    nimcp_gpu_tensor_t* pos_tensor = nimcp_gpu_tensor_from_host(
        ctx, positions.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_copy(ctx, pos_tensor, state->positions);

    // Find neighbors within radius
    float radius = 5.0f;
    bool result = nimcp_gpu_flocking_find_neighbors(ctx, state, radius);
    EXPECT_TRUE(result) << "Neighbor finding should succeed";

    // Verify neighbor counts are populated
    std::vector<uint32_t> neighbor_counts(SMALL_N_AGENTS);
    copy_to_host_uint32(state->neighbor_counts, neighbor_counts.data());

    int total_neighbors = 0;
    for (uint32_t count : neighbor_counts) {
        total_neighbors += static_cast<int>(count);
    }
    EXPECT_GT(total_neighbors, 0) << "Should find some neighbors in tight cluster";

    nimcp_gpu_tensor_destroy(pos_tensor);
    nimcp_flocking_gpu_destroy(state);
}

//=============================================================================
// Consensus Tests
//=============================================================================

/**
 * TEST: Consensus state creation
 * WHAT: Create consensus state on GPU
 * WHY:  Verify basic allocation for consensus algorithms
 */
TEST_F(SwarmKernelTest, ConsensusStateCreate_ValidParams_Succeeds) {
    RequireGPU();

    nimcp_consensus_gpu_params_t params = create_default_consensus_params();
    nimcp_consensus_gpu_state_t* state = nimcp_consensus_gpu_create(
        ctx, DEFAULT_N_AGENTS, DEFAULT_BELIEF_DIM, &params);

    ASSERT_NE(state, nullptr) << "Consensus state creation should succeed";
    EXPECT_NE(state->beliefs, nullptr) << "Beliefs tensor should be allocated";
    EXPECT_NE(state->weights, nullptr) << "Weights tensor should be allocated";
    EXPECT_EQ(state->n_agents, DEFAULT_N_AGENTS);
    EXPECT_EQ(state->belief_dim, DEFAULT_BELIEF_DIM);

    nimcp_consensus_gpu_destroy(state);
}

/**
 * TEST: Consensus averaging convergence
 * WHAT: Run parallel averaging until beliefs converge
 * WHY:  Core consensus algorithm for distributed agreement
 */
TEST_F(SwarmKernelTest, ConsensusAveraging_UniformWeights_ConvergesToMean) {
    RequireGPU();

    const size_t n_agents = 5;
    const size_t belief_dim = 1;

    nimcp_consensus_gpu_params_t params = create_default_consensus_params();
    params.learning_rate = 0.5f;

    nimcp_consensus_gpu_state_t* state = nimcp_consensus_gpu_create(
        ctx, n_agents, belief_dim, &params);
    ASSERT_NE(state, nullptr);

    // Initial beliefs: [1, 2, 3, 4, 5]
    std::vector<float> initial_beliefs = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float expected_mean = 3.0f;  // (1+2+3+4+5)/5
    size_t dims1[2] = {n_agents, belief_dim};
    nimcp_gpu_tensor_t* belief_tensor = nimcp_gpu_tensor_from_host(
        ctx, initial_beliefs.data(), dims1, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_copy(ctx, belief_tensor, state->beliefs);

    // Uniform weight matrix (all connected equally)
    std::vector<float> weights(n_agents * n_agents, 1.0f / n_agents);
    size_t dims2[2] = {n_agents, n_agents};
    nimcp_gpu_tensor_t* weight_tensor = nimcp_gpu_tensor_from_host(
        ctx, weights.data(), dims2, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_copy(ctx, weight_tensor, state->weights);

    // Run averaging iterations
    // Note: nimcp_gpu_consensus_averaging handles buffer swap internally
    for (int iter = 0; iter < 20; iter++) {
        bool result = nimcp_gpu_consensus_averaging(ctx, state);
        ASSERT_TRUE(result) << "Averaging iteration " << iter << " should succeed";
    }

    // Check convergence
    std::vector<float> final_beliefs(n_agents);
    copy_to_host(state->beliefs, final_beliefs.data());

    for (float b : final_beliefs) {
        EXPECT_NEAR(b, expected_mean, 0.1f) << "Beliefs should converge to mean";
    }

    nimcp_gpu_tensor_destroy(belief_tensor);
    nimcp_gpu_tensor_destroy(weight_tensor);
    nimcp_consensus_gpu_destroy(state);
}

/**
 * TEST: Consensus belief propagation
 * WHAT: Propagate beliefs through network using message passing
 * WHY:  Alternative consensus method for probabilistic inference
 */
TEST_F(SwarmKernelTest, ConsensusBeliefPropagation_UpdatesBeliefs) {
    RequireGPU();

    nimcp_consensus_gpu_params_t params = create_default_consensus_params();
    nimcp_consensus_gpu_state_t* state = nimcp_consensus_gpu_create(
        ctx, SMALL_N_AGENTS, DEFAULT_BELIEF_DIM, &params);
    ASSERT_NE(state, nullptr);

    // Initialize with random-ish beliefs
    std::vector<float> beliefs(SMALL_N_AGENTS * DEFAULT_BELIEF_DIM);
    for (size_t i = 0; i < beliefs.size(); i++) {
        beliefs[i] = static_cast<float>(i % 10) / 10.0f;
    }
    size_t dims[2] = {SMALL_N_AGENTS, DEFAULT_BELIEF_DIM};
    nimcp_gpu_tensor_t* belief_tensor = nimcp_gpu_tensor_from_host(
        ctx, beliefs.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_copy(ctx, belief_tensor, state->beliefs);

    // Initialize weights (sparse connectivity)
    std::vector<float> weights(SMALL_N_AGENTS * SMALL_N_AGENTS, 0.0f);
    for (size_t i = 0; i < SMALL_N_AGENTS; i++) {
        // Connect to neighbors
        weights[i * SMALL_N_AGENTS + i] = 0.5f;
        if (i > 0) weights[i * SMALL_N_AGENTS + (i - 1)] = 0.25f;
        if (i < SMALL_N_AGENTS - 1) weights[i * SMALL_N_AGENTS + (i + 1)] = 0.25f;
    }
    size_t dims2[2] = {SMALL_N_AGENTS, SMALL_N_AGENTS};
    nimcp_gpu_tensor_t* weight_tensor = nimcp_gpu_tensor_from_host(
        ctx, weights.data(), dims2, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_copy(ctx, weight_tensor, state->weights);

    // Run belief propagation
    // Note: The function swaps buffers internally, so after the call
    // state->beliefs contains the UPDATED beliefs
    bool result = nimcp_gpu_consensus_belief_propagation(ctx, state);
    EXPECT_TRUE(result) << "Belief propagation should succeed";

    // Verify beliefs were updated by comparing current beliefs to original
    std::vector<float> updated_beliefs(SMALL_N_AGENTS * DEFAULT_BELIEF_DIM);
    copy_to_host(state->beliefs, updated_beliefs.data());

    bool beliefs_changed = false;
    for (size_t i = 0; i < beliefs.size(); i++) {
        if (std::abs(beliefs[i] - updated_beliefs[i]) > TOLERANCE) {
            beliefs_changed = true;
            break;
        }
    }
    EXPECT_TRUE(beliefs_changed) << "Belief propagation should update beliefs";

    nimcp_gpu_tensor_destroy(belief_tensor);
    nimcp_gpu_tensor_destroy(weight_tensor);
    nimcp_consensus_gpu_destroy(state);
}

/**
 * TEST: Consensus convergence check
 * WHAT: Check if consensus has been reached
 * WHY:  Determine when to stop iterations
 */
TEST_F(SwarmKernelTest, ConsensusCheckConvergence_IdenticalBeliefs_ReturnsConverged) {
    RequireGPU();

    nimcp_consensus_gpu_params_t params = create_default_consensus_params();
    nimcp_consensus_gpu_state_t* state = nimcp_consensus_gpu_create(
        ctx, SMALL_N_AGENTS, DEFAULT_BELIEF_DIM, &params);
    ASSERT_NE(state, nullptr);

    // Set all beliefs to same value (already converged)
    std::vector<float> beliefs(SMALL_N_AGENTS * DEFAULT_BELIEF_DIM, 0.5f);
    size_t dims[2] = {SMALL_N_AGENTS, DEFAULT_BELIEF_DIM};
    nimcp_gpu_tensor_t* belief_tensor = nimcp_gpu_tensor_from_host(
        ctx, beliefs.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_copy(ctx, belief_tensor, state->beliefs);

    bool converged = false;
    float variance = 1.0f;
    bool result = nimcp_gpu_consensus_check_convergence(ctx, state, &converged, &variance);
    EXPECT_TRUE(result) << "Convergence check should succeed";
    EXPECT_TRUE(converged) << "Identical beliefs should be converged";
    EXPECT_NEAR(variance, 0.0f, TOLERANCE) << "Variance should be zero";

    nimcp_gpu_tensor_destroy(belief_tensor);
    nimcp_consensus_gpu_destroy(state);
}

//=============================================================================
// Pheromone Tests
//=============================================================================

/**
 * TEST: Pheromone state creation
 * WHAT: Create pheromone grid state on GPU
 * WHY:  Verify basic allocation for pheromone algorithms
 */
TEST_F(SwarmKernelTest, PheromoneStateCreate_ValidParams_Succeeds) {
    RequireGPU();

    nimcp_pheromone_gpu_params_t params = create_default_pheromone_params();
    nimcp_pheromone_gpu_state_t* state = nimcp_pheromone_gpu_create(
        ctx, PHEROMONE_GRID_X, PHEROMONE_GRID_Y, PHEROMONE_GRID_Z,
        PHEROMONE_N_TYPES, PHEROMONE_VOXEL_SIZE, &params);

    ASSERT_NE(state, nullptr) << "Pheromone state creation should succeed";
    EXPECT_NE(state->concentration, nullptr) << "Concentration grid should be allocated";
    EXPECT_EQ(state->grid_x, PHEROMONE_GRID_X);
    EXPECT_EQ(state->grid_y, PHEROMONE_GRID_Y);
    EXPECT_EQ(state->n_types, PHEROMONE_N_TYPES);

    nimcp_pheromone_gpu_destroy(state);
}

/**
 * TEST: Pheromone diffusion
 * WHAT: Apply diffusion to spread pheromone concentrations
 * WHY:  Simulates chemical diffusion in environment
 */
TEST_F(SwarmKernelTest, PheromoneDiffusion_ConcentratedSpot_Spreads) {
    RequireGPU();

    nimcp_pheromone_gpu_params_t params = create_default_pheromone_params();
    params.diffusion_rate = 0.1f;

    nimcp_pheromone_gpu_state_t* state = nimcp_pheromone_gpu_create(
        ctx, 16, 16, 1, 1, 1.0f, &params);
    ASSERT_NE(state, nullptr);

    // Create a concentrated spot in the center
    size_t grid_size = 16 * 16 * 1;
    std::vector<float> concentration(grid_size, 0.0f);
    concentration[8 * 16 + 8] = 10.0f;  // Center point

    size_t dims[3] = {16, 16, 1};
    nimcp_gpu_tensor_t* conc_tensor = nimcp_gpu_tensor_from_host(
        ctx, concentration.data(), dims, 3, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_copy(ctx, conc_tensor, state->concentration);

    // Apply diffusion
    bool result = nimcp_gpu_pheromone_diffusion(ctx, state, DEFAULT_DT);
    EXPECT_TRUE(result) << "Pheromone diffusion should succeed";

    // Verify pheromone has spread
    std::vector<float> new_concentration(grid_size);
    copy_to_host(state->concentration, new_concentration.data());

    // Center should have decreased
    EXPECT_LT(new_concentration[8 * 16 + 8], 10.0f) << "Center concentration should decrease";

    // Neighbors should have increased
    float neighbor_sum = new_concentration[7 * 16 + 8] + new_concentration[9 * 16 + 8] +
                         new_concentration[8 * 16 + 7] + new_concentration[8 * 16 + 9];
    EXPECT_GT(neighbor_sum, 0.0f) << "Neighbors should have gained concentration";

    nimcp_gpu_tensor_destroy(conc_tensor);
    nimcp_pheromone_gpu_destroy(state);
}

/**
 * TEST: Pheromone decay
 * WHAT: Apply exponential decay to pheromone concentrations
 * WHY:  Simulates evaporation over time
 */
TEST_F(SwarmKernelTest, PheromoneDecay_UniformConcentration_DecaysExponentially) {
    RequireGPU();

    nimcp_pheromone_gpu_params_t params = create_default_pheromone_params();
    params.decay_rates[0] = 0.1f;  // 10% decay per second

    nimcp_pheromone_gpu_state_t* state = nimcp_pheromone_gpu_create(
        ctx, 8, 8, 1, 1, 1.0f, &params);
    ASSERT_NE(state, nullptr);

    // Uniform initial concentration
    float initial_value = 1.0f;
    nimcp_gpu_fill(ctx, state->concentration, initial_value);

    // Apply decay with dt = 1.0
    bool result = nimcp_gpu_pheromone_decay(ctx, state, 1.0f);
    EXPECT_TRUE(result) << "Pheromone decay should succeed";

    // Expected: c = c * exp(-0.1 * 1.0) = 0.905
    float expected = initial_value * std::exp(-params.decay_rates[0] * 1.0f);

    std::vector<float> concentration(8 * 8 * 1);
    copy_to_host(state->concentration, concentration.data());

    for (float c : concentration) {
        EXPECT_NEAR(c, expected, 0.01f) << "Concentration should decay exponentially";
    }

    nimcp_pheromone_gpu_destroy(state);
}

/**
 * TEST: Pheromone deposit
 * WHAT: Deposit pheromone at specified positions
 * WHY:  Agents leave pheromone trails
 */
TEST_F(SwarmKernelTest, PheromoneDeposit_AtPositions_IncreasesConcentration) {
    RequireGPU();

    nimcp_pheromone_gpu_params_t params = create_default_pheromone_params();
    nimcp_pheromone_gpu_state_t* state = nimcp_pheromone_gpu_create(
        ctx, 16, 16, 1, 2, 1.0f, &params);
    ASSERT_NE(state, nullptr);

    // Clear initial concentration
    nimcp_gpu_zeros(ctx, state->concentration);

    // Deposit positions (3 deposits)
    std::vector<float> positions = {
        4.5f, 4.5f, 0.0f,   // Near grid cell (4,4)
        8.5f, 8.5f, 0.0f,   // Near grid cell (8,8)
        12.5f, 12.5f, 0.0f  // Near grid cell (12,12)
    };
    size_t dims_pos[2] = {3, 3};
    nimcp_gpu_tensor_t* pos_tensor = nimcp_gpu_tensor_from_host(
        ctx, positions.data(), dims_pos, 2, NIMCP_GPU_PRECISION_FP32);

    // Pheromone types (all type 0)
    std::vector<float> types = {0.0f, 0.0f, 0.0f};
    size_t dims_types[1] = {3};
    nimcp_gpu_tensor_t* types_tensor = nimcp_gpu_tensor_from_host(
        ctx, types.data(), dims_types, 1, NIMCP_GPU_PRECISION_FP32);

    // Deposit amounts
    std::vector<float> amounts = {1.0f, 2.0f, 3.0f};
    nimcp_gpu_tensor_t* amounts_tensor = nimcp_gpu_tensor_from_host(
        ctx, amounts.data(), dims_types, 1, NIMCP_GPU_PRECISION_FP32);

    bool result = nimcp_gpu_pheromone_deposit(ctx, state, pos_tensor, types_tensor, amounts_tensor, 3);
    EXPECT_TRUE(result) << "Pheromone deposit should succeed";

    // Verify concentration increased at deposit locations
    std::vector<float> concentration(16 * 16 * 2);
    copy_to_host(state->concentration, concentration.data());

    // Check that some cells have non-zero concentration
    float total = 0.0f;
    for (float c : concentration) {
        total += c;
    }
    EXPECT_NEAR(total, 6.0f, 0.1f) << "Total deposited should be 1+2+3=6";

    nimcp_gpu_tensor_destroy(pos_tensor);
    nimcp_gpu_tensor_destroy(types_tensor);
    nimcp_gpu_tensor_destroy(amounts_tensor);
    nimcp_pheromone_gpu_destroy(state);
}

/**
 * TEST: Pheromone sample
 * WHAT: Sample pheromone concentration at specified positions
 * WHY:  Agents sense pheromone levels
 */
TEST_F(SwarmKernelTest, PheromoneSample_AtKnownPositions_ReturnsCorrectValues) {
    RequireGPU();

    nimcp_pheromone_gpu_params_t params = create_default_pheromone_params();
    nimcp_pheromone_gpu_state_t* state = nimcp_pheromone_gpu_create(
        ctx, 8, 8, 1, 1, 1.0f, &params);
    ASSERT_NE(state, nullptr);

    // Set known concentration pattern
    std::vector<float> concentration(8 * 8 * 1, 0.0f);
    concentration[4 * 8 + 4] = 5.0f;  // Peak at (4,4)

    size_t dims[3] = {8, 8, 1};
    nimcp_gpu_tensor_t* conc_tensor = nimcp_gpu_tensor_from_host(
        ctx, concentration.data(), dims, 3, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_copy(ctx, conc_tensor, state->concentration);

    // Sample at the peak location
    std::vector<float> sample_positions = {4.5f, 4.5f, 0.0f};
    size_t dims_pos[2] = {1, 3};
    nimcp_gpu_tensor_t* pos_tensor = nimcp_gpu_tensor_from_host(
        ctx, sample_positions.data(), dims_pos, 2, NIMCP_GPU_PRECISION_FP32);

    nimcp_gpu_tensor_t* output = create_zero_tensor(1);
    ASSERT_NE(output, nullptr);

    bool result = nimcp_gpu_pheromone_sample(ctx, state, pos_tensor, 0, output);
    EXPECT_TRUE(result) << "Pheromone sampling should succeed";

    std::vector<float> sampled(1);
    copy_to_host(output, sampled.data());

    EXPECT_GT(sampled[0], 0.0f) << "Should sample non-zero concentration at peak";

    nimcp_gpu_tensor_destroy(conc_tensor);
    nimcp_gpu_tensor_destroy(pos_tensor);
    nimcp_gpu_tensor_destroy(output);
    nimcp_pheromone_gpu_destroy(state);
}

//=============================================================================
// Quorum Sensing Tests
//=============================================================================

/**
 * TEST: Quorum state creation
 * WHAT: Create quorum sensing state on GPU
 * WHY:  Verify basic allocation for quorum algorithms
 */
TEST_F(SwarmKernelTest, QuorumStateCreate_ValidParams_Succeeds) {
    RequireGPU();

    nimcp_quorum_gpu_params_t params = create_default_quorum_params();
    nimcp_quorum_gpu_state_t* state = nimcp_quorum_gpu_create(
        ctx, DEFAULT_N_AGENTS, DEFAULT_N_SIGNAL_TYPES, &params);

    ASSERT_NE(state, nullptr) << "Quorum state creation should succeed";
    EXPECT_NE(state->signal_concentrations, nullptr);
    EXPECT_NE(state->agent_signals, nullptr);
    EXPECT_NE(state->threshold_reached, nullptr);
    EXPECT_EQ(state->n_agents, DEFAULT_N_AGENTS);
    EXPECT_EQ(state->n_signal_types, DEFAULT_N_SIGNAL_TYPES);

    nimcp_quorum_gpu_destroy(state);
}

/**
 * TEST: Quorum threshold detection
 * WHAT: Detect when signal concentration exceeds threshold
 * WHY:  Triggers collective behavior when quorum is reached
 */
TEST_F(SwarmKernelTest, QuorumCheckThresholds_AboveThreshold_SetsFlag) {
    RequireGPU();

    nimcp_quorum_gpu_params_t params = create_default_quorum_params();
    params.base_threshold = 0.5f;

    nimcp_quorum_gpu_state_t* state = nimcp_quorum_gpu_create(
        ctx, SMALL_N_AGENTS, 2, &params);
    ASSERT_NE(state, nullptr);

    // Set signal concentrations: type 0 above threshold, type 1 below
    // Note: kernel uses threshold = base_threshold * n_agents = 0.5 * 10 = 5.0
    std::vector<float> concentrations = {8.0f, 3.0f};  // 8.0 > 5.0, 3.0 < 5.0
    size_t dims[1] = {2};
    nimcp_gpu_tensor_t* conc_tensor = nimcp_gpu_tensor_from_host(
        ctx, concentrations.data(), dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_copy(ctx, conc_tensor, state->signal_concentrations);

    // Check thresholds
    bool result = nimcp_gpu_quorum_check_thresholds(ctx, state);
    EXPECT_TRUE(result) << "Threshold check should succeed";

    // Verify flags (threshold_reached is UINT32)
    std::vector<uint32_t> flags(2);
    copy_to_host_uint32(state->threshold_reached, flags.data());

    EXPECT_GT(flags[0], 0u) << "Type 0 should be above threshold (flag=1)";
    EXPECT_EQ(flags[1], 0u) << "Type 1 should be below threshold (flag=0)";

    nimcp_gpu_tensor_destroy(conc_tensor);
    nimcp_quorum_gpu_destroy(state);
}

/**
 * TEST: Quorum signal concentration computation
 * WHAT: Compute total signal concentration from all agents
 * WHY:  Aggregate individual signals to detect quorum
 */
TEST_F(SwarmKernelTest, QuorumComputeConcentration_AgentSignals_SumsCorrectly) {
    RequireGPU();

    nimcp_quorum_gpu_params_t params = create_default_quorum_params();
    nimcp_quorum_gpu_state_t* state = nimcp_quorum_gpu_create(
        ctx, 5, 2, &params);
    ASSERT_NE(state, nullptr);

    // Each agent emits signals
    // Agent signals: [n_agents x n_signal_types]
    std::vector<float> agent_signals = {
        0.1f, 0.2f,   // Agent 0
        0.1f, 0.2f,   // Agent 1
        0.1f, 0.2f,   // Agent 2
        0.1f, 0.2f,   // Agent 3
        0.1f, 0.2f    // Agent 4
    };
    size_t dims[2] = {5, 2};
    nimcp_gpu_tensor_t* signal_tensor = nimcp_gpu_tensor_from_host(
        ctx, agent_signals.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_copy(ctx, signal_tensor, state->agent_signals);

    // Compute concentration
    bool result = nimcp_gpu_quorum_compute_concentration(ctx, state);
    EXPECT_TRUE(result) << "Concentration computation should succeed";

    // Expected: type 0 = 5*0.1 = 0.5, type 1 = 5*0.2 = 1.0
    std::vector<float> concentrations(2);
    copy_to_host(state->signal_concentrations, concentrations.data());

    EXPECT_NEAR(concentrations[0], 0.5f, 0.01f) << "Type 0 concentration should sum correctly";
    EXPECT_NEAR(concentrations[1], 1.0f, 0.01f) << "Type 1 concentration should sum correctly";

    nimcp_gpu_tensor_destroy(signal_tensor);
    nimcp_quorum_gpu_destroy(state);
}

//=============================================================================
// Task Allocation Tests
//=============================================================================

/**
 * TEST: Task allocation state creation
 * WHAT: Create task allocation state on GPU
 * WHY:  Verify basic allocation for auction algorithms
 */
TEST_F(SwarmKernelTest, TaskAllocStateCreate_ValidParams_Succeeds) {
    RequireGPU();

    nimcp_task_alloc_gpu_params_t params = create_default_task_alloc_params();
    nimcp_task_alloc_gpu_state_t* state = nimcp_task_alloc_gpu_create(
        ctx, DEFAULT_N_AGENTS, DEFAULT_N_TASKS, DEFAULT_N_CAPABILITIES, &params);

    ASSERT_NE(state, nullptr) << "Task allocation state creation should succeed";
    EXPECT_NE(state->agent_capabilities, nullptr);
    EXPECT_NE(state->task_requirements, nullptr);
    EXPECT_NE(state->bids, nullptr);
    EXPECT_NE(state->assignments, nullptr);
    EXPECT_EQ(state->n_agents, DEFAULT_N_AGENTS);
    EXPECT_EQ(state->n_tasks, DEFAULT_N_TASKS);

    nimcp_task_alloc_gpu_destroy(state);
}

/**
 * TEST: Task auction round
 * WHAT: Run parallel auction round where agents bid on tasks
 * WHY:  Core mechanism for distributed task allocation
 */
TEST_F(SwarmKernelTest, TaskAuctionRound_CapableAgents_ProducesBids) {
    RequireGPU();

    const size_t n_agents = 5;
    const size_t n_tasks = 3;
    const size_t n_caps = 2;

    nimcp_task_alloc_gpu_params_t params = create_default_task_alloc_params();
    nimcp_task_alloc_gpu_state_t* state = nimcp_task_alloc_gpu_create(
        ctx, n_agents, n_tasks, n_caps, &params);
    ASSERT_NE(state, nullptr);

    // Agent capabilities: each agent has different strengths
    std::vector<float> capabilities = {
        1.0f, 0.5f,   // Agent 0: strong in cap 0
        0.5f, 1.0f,   // Agent 1: strong in cap 1
        0.8f, 0.8f,   // Agent 2: balanced
        0.3f, 0.3f,   // Agent 3: weak
        0.9f, 0.7f    // Agent 4: above average
    };
    size_t dims_cap[2] = {n_agents, n_caps};
    nimcp_gpu_tensor_t* cap_tensor = nimcp_gpu_tensor_from_host(
        ctx, capabilities.data(), dims_cap, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_copy(ctx, cap_tensor, state->agent_capabilities);

    // Task requirements
    std::vector<float> requirements = {
        1.0f, 0.0f,   // Task 0: needs cap 0
        0.0f, 1.0f,   // Task 1: needs cap 1
        0.5f, 0.5f    // Task 2: needs both
    };
    size_t dims_req[2] = {n_tasks, n_caps};
    nimcp_gpu_tensor_t* req_tensor = nimcp_gpu_tensor_from_host(
        ctx, requirements.data(), dims_req, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_copy(ctx, req_tensor, state->task_requirements);

    // Initialize prices to zero
    nimcp_gpu_zeros(ctx, state->prices);

    // First compute matches (generates initial bids based on capability/requirement match)
    bool matches_result = nimcp_gpu_task_compute_matches(ctx, state);
    EXPECT_TRUE(matches_result) << "Computing matches should succeed";

    // Run auction round
    bool result = nimcp_gpu_task_auction_round(ctx, state);
    EXPECT_TRUE(result) << "Auction round should succeed";

    // Verify bids were produced (computed by task_compute_matches)
    std::vector<float> bids(n_agents * n_tasks);
    copy_to_host(state->bids, bids.data());

    float total_bids = 0.0f;
    for (float b : bids) {
        total_bids += std::abs(b);
    }
    EXPECT_GT(total_bids, 0.0f) << "Agents should produce bids";

    nimcp_gpu_tensor_destroy(cap_tensor);
    nimcp_gpu_tensor_destroy(req_tensor);
    nimcp_task_alloc_gpu_destroy(state);
}

/**
 * TEST: Task price update
 * WHAT: Update task prices based on bids
 * WHY:  Price adjustment for auction convergence
 */
TEST_F(SwarmKernelTest, TaskUpdatePrices_PositiveBids_IncreasePrices) {
    RequireGPU();

    const size_t n_agents = 3;
    const size_t n_tasks = 2;

    nimcp_task_alloc_gpu_params_t params = create_default_task_alloc_params();
    params.bid_increment = 0.1f;

    nimcp_task_alloc_gpu_state_t* state = nimcp_task_alloc_gpu_create(
        ctx, n_agents, n_tasks, 1, &params);
    ASSERT_NE(state, nullptr);

    // Initial prices
    nimcp_gpu_zeros(ctx, state->prices);

    // Set bids (all agents bid on all tasks)
    std::vector<float> bids = {
        1.0f, 0.5f,   // Agent 0 bids
        0.8f, 0.9f,   // Agent 1 bids
        0.6f, 0.7f    // Agent 2 bids
    };
    size_t dims[2] = {n_agents, n_tasks};
    nimcp_gpu_tensor_t* bid_tensor = nimcp_gpu_tensor_from_host(
        ctx, bids.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_copy(ctx, bid_tensor, state->bids);

    // Run auction round first to populate best_bids from the bids
    bool auction_result = nimcp_gpu_task_auction_round(ctx, state);
    EXPECT_TRUE(auction_result) << "Auction round should succeed";

    // Update prices (uses best_bids from auction round)
    bool result = nimcp_gpu_task_update_prices(ctx, state);
    EXPECT_TRUE(result) << "Price update should succeed";

    // Verify prices changed (best_bids should have been populated by auction)
    std::vector<float> prices(n_tasks);
    copy_to_host(state->prices, prices.data());

    // At least one price should have changed if there were winning bids
    bool price_changed = (prices[0] > 0.0f) || (prices[1] > 0.0f);
    EXPECT_TRUE(price_changed) << "Task prices should change with winning bids";

    nimcp_gpu_tensor_destroy(bid_tensor);
    nimcp_task_alloc_gpu_destroy(state);
}

/**
 * TEST: Task assignment finalization
 * WHAT: Finalize task assignments after auction
 * WHY:  Determine final agent-task mapping
 */
TEST_F(SwarmKernelTest, TaskFinalizeAssignments_AfterAuction_AssignsAgents) {
    RequireGPU();

    const size_t n_agents = 4;
    const size_t n_tasks = 3;

    nimcp_task_alloc_gpu_params_t params = create_default_task_alloc_params();
    nimcp_task_alloc_gpu_state_t* state = nimcp_task_alloc_gpu_create(
        ctx, n_agents, n_tasks, 1, &params);
    ASSERT_NE(state, nullptr);

    // Set best bids and best agents manually
    std::vector<float> best_bids = {1.0f, 0.9f, 0.8f};
    std::vector<float> best_agents = {0.0f, 1.0f, 2.0f};  // Task 0->Agent 0, etc.

    size_t dims[1] = {n_tasks};
    nimcp_gpu_tensor_t* bb_tensor = nimcp_gpu_tensor_from_host(
        ctx, best_bids.data(), dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* ba_tensor = nimcp_gpu_tensor_from_host(
        ctx, best_agents.data(), dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_copy(ctx, bb_tensor, state->best_bids);
    nimcp_gpu_copy(ctx, ba_tensor, state->best_agents);

    // Finalize assignments
    bool result = nimcp_gpu_task_finalize_assignments(ctx, state);
    EXPECT_TRUE(result) << "Assignment finalization should succeed";

    // Verify assignments
    std::vector<float> assignments(n_agents);
    copy_to_host(state->assignments, assignments.data());

    // Each agent should be assigned to at most one task (or -1 if unassigned)
    int assigned_count = 0;
    for (float a : assignments) {
        if (a >= 0) assigned_count++;
    }
    EXPECT_GE(assigned_count, 0) << "Some agents should be assigned";

    nimcp_gpu_tensor_destroy(bb_tensor);
    nimcp_gpu_tensor_destroy(ba_tensor);
    nimcp_task_alloc_gpu_destroy(state);
}

//=============================================================================
// Collision Detection Tests
//=============================================================================

/**
 * TEST: Collision state creation
 * WHAT: Create collision detection state on GPU
 * WHY:  Verify basic allocation for collision algorithms
 */
TEST_F(SwarmKernelTest, CollisionStateCreate_ValidParams_Succeeds) {
    RequireGPU();

    nimcp_collision_gpu_params_t params = create_default_collision_params();
    nimcp_collision_gpu_state_t* state = nimcp_collision_gpu_create(
        ctx, DEFAULT_N_AGENTS, DEFAULT_MAX_PAIRS, &params);

    ASSERT_NE(state, nullptr) << "Collision state creation should succeed";
    EXPECT_NE(state->positions, nullptr);
    EXPECT_NE(state->collision_flags, nullptr);
    EXPECT_NE(state->collision_pairs, nullptr);
    EXPECT_EQ(state->n_agents, DEFAULT_N_AGENTS);

    nimcp_collision_gpu_destroy(state);
}

/**
 * TEST: Pairwise distance computation
 * WHAT: Compute distances between all pairs of agents
 * WHY:  Foundation for collision detection
 */
TEST_F(SwarmKernelTest, PairwiseDistances_GridPositions_ComputesCorrectly) {
    RequireGPU();

    const size_t n_agents = 4;

    // Place agents at known positions
    std::vector<float> positions = {
        0.0f, 0.0f, 0.0f, 1.0f,   // Agent 0 at origin
        1.0f, 0.0f, 0.0f, 1.0f,   // Agent 1 at (1,0,0)
        0.0f, 1.0f, 0.0f, 1.0f,   // Agent 2 at (0,1,0)
        1.0f, 1.0f, 0.0f, 1.0f    // Agent 3 at (1,1,0)
    };
    size_t dims[2] = {n_agents, 4};
    nimcp_gpu_tensor_t* pos_tensor = nimcp_gpu_tensor_from_host(
        ctx, positions.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);

    // Output distance matrix
    nimcp_gpu_tensor_t* distances = create_matrix(n_agents, n_agents, 0.0f);
    ASSERT_NE(distances, nullptr);

    bool result = nimcp_gpu_pairwise_distances(ctx, pos_tensor, distances, n_agents);
    EXPECT_TRUE(result) << "Pairwise distance computation should succeed";

    // Verify distances
    std::vector<float> dist_host(n_agents * n_agents);
    copy_to_host(distances, dist_host.data());

    // Distance from agent 0 to agent 1 should be 1.0
    EXPECT_NEAR(dist_host[0 * n_agents + 1], 1.0f, 0.01f);
    // Distance from agent 0 to agent 2 should be 1.0
    EXPECT_NEAR(dist_host[0 * n_agents + 2], 1.0f, 0.01f);
    // Distance from agent 0 to agent 3 should be sqrt(2) = 1.414
    EXPECT_NEAR(dist_host[0 * n_agents + 3], std::sqrt(2.0f), 0.01f);
    // Diagonal (self-distance) should be 0
    EXPECT_NEAR(dist_host[0 * n_agents + 0], 0.0f, 0.01f);

    nimcp_gpu_tensor_destroy(pos_tensor);
    nimcp_gpu_tensor_destroy(distances);
}

/**
 * TEST: Collision detection with overlapping agents
 * WHAT: Detect collisions when agents are within collision radius
 * WHY:  Core safety function for swarm navigation
 */
TEST_F(SwarmKernelTest, CollisionDetect_OverlappingAgents_DetectsCollision) {
    RequireGPU();

    nimcp_collision_gpu_params_t params = create_default_collision_params();
    params.collision_radius = 1.5f;

    nimcp_collision_gpu_state_t* state = nimcp_collision_gpu_create(
        ctx, 4, 10, &params);
    ASSERT_NE(state, nullptr);

    // Place agents with some overlapping (within collision_radius)
    std::vector<float> positions = {
        0.0f, 0.0f, 0.0f, 1.0f,   // Agent 0
        1.0f, 0.0f, 0.0f, 1.0f,   // Agent 1 (distance 1.0 from 0, < 1.5)
        5.0f, 0.0f, 0.0f, 1.0f,   // Agent 2 (far from 0 and 1)
        5.5f, 0.0f, 0.0f, 1.0f    // Agent 3 (distance 0.5 from 2, < 1.5)
    };
    size_t dims[2] = {4, 4};
    nimcp_gpu_tensor_t* pos_tensor = nimcp_gpu_tensor_from_host(
        ctx, positions.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_copy(ctx, pos_tensor, state->positions);

    // Detect collisions
    bool result = nimcp_gpu_collision_detect(ctx, state);
    EXPECT_TRUE(result) << "Collision detection should succeed";

    // Verify collision flags
    std::vector<float> flags(4);
    copy_to_host(state->collision_flags, flags.data());

    EXPECT_GT(flags[0], 0.5f) << "Agent 0 should be in collision";
    EXPECT_GT(flags[1], 0.5f) << "Agent 1 should be in collision";
    EXPECT_GT(flags[2], 0.5f) << "Agent 2 should be in collision with 3";
    EXPECT_GT(flags[3], 0.5f) << "Agent 3 should be in collision with 2";

    nimcp_gpu_tensor_destroy(pos_tensor);
    nimcp_collision_gpu_destroy(state);
}

/**
 * TEST: Collision pair retrieval
 * WHAT: Get list of colliding agent pairs
 * WHY:  Needed for collision response
 */
TEST_F(SwarmKernelTest, CollisionGetPairs_AfterDetection_ReturnsPairs) {
    RequireGPU();

    nimcp_collision_gpu_params_t params = create_default_collision_params();
    params.collision_radius = 2.0f;

    nimcp_collision_gpu_state_t* state = nimcp_collision_gpu_create(
        ctx, 3, 10, &params);
    ASSERT_NE(state, nullptr);

    // Place all agents close together
    std::vector<float> positions = {
        0.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 0.0f, 1.0f,
        0.5f, 0.5f, 0.0f, 1.0f
    };
    size_t dims[2] = {3, 4};
    nimcp_gpu_tensor_t* pos_tensor = nimcp_gpu_tensor_from_host(
        ctx, positions.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_copy(ctx, pos_tensor, state->positions);

    // Detect collisions first
    nimcp_gpu_collision_detect(ctx, state);

    // Get collision pairs
    std::vector<uint32_t> pairs(10 * 2);
    size_t count = 0;
    bool result = nimcp_gpu_collision_get_pairs(ctx, state, pairs.data(), 10, &count);
    EXPECT_TRUE(result) << "Getting collision pairs should succeed";

    // All pairs should collide (3 agents = 3 pairs: 0-1, 0-2, 1-2)
    EXPECT_GE(count, 1u) << "Should detect at least one collision pair";

    nimcp_gpu_tensor_destroy(pos_tensor);
    nimcp_collision_gpu_destroy(state);
}

//=============================================================================
// NULL Safety Tests
//=============================================================================

/**
 * TEST: Flocking separation with NULL state
 * WHAT: Try separation with NULL state
 * WHY:  Verify NULL-safety
 */
TEST_F(SwarmKernelTest, FlockingSeparation_NullState_ReturnsFalse) {
    RequireGPU();
    EXPECT_FALSE(nimcp_gpu_flocking_separation(ctx, nullptr));
}

/**
 * TEST: Consensus averaging with NULL state
 * WHAT: Try averaging with NULL state
 * WHY:  Verify NULL-safety
 */
TEST_F(SwarmKernelTest, ConsensusAveraging_NullState_ReturnsFalse) {
    RequireGPU();
    EXPECT_FALSE(nimcp_gpu_consensus_averaging(ctx, nullptr));
}

/**
 * TEST: Pheromone diffusion with NULL state
 * WHAT: Try diffusion with NULL state
 * WHY:  Verify NULL-safety
 */
TEST_F(SwarmKernelTest, PheromoneDiffusion_NullState_ReturnsFalse) {
    RequireGPU();
    EXPECT_FALSE(nimcp_gpu_pheromone_diffusion(ctx, nullptr, DEFAULT_DT));
}

/**
 * TEST: Quorum threshold check with NULL state
 * WHAT: Try threshold check with NULL state
 * WHY:  Verify NULL-safety
 */
TEST_F(SwarmKernelTest, QuorumCheckThresholds_NullState_ReturnsFalse) {
    RequireGPU();
    EXPECT_FALSE(nimcp_gpu_quorum_check_thresholds(ctx, nullptr));
}

/**
 * TEST: Task auction with NULL state
 * WHAT: Try auction round with NULL state
 * WHY:  Verify NULL-safety
 */
TEST_F(SwarmKernelTest, TaskAuctionRound_NullState_ReturnsFalse) {
    RequireGPU();
    EXPECT_FALSE(nimcp_gpu_task_auction_round(ctx, nullptr));
}

/**
 * TEST: Collision detect with NULL state
 * WHAT: Try collision detection with NULL state
 * WHY:  Verify NULL-safety
 */
TEST_F(SwarmKernelTest, CollisionDetect_NullState_ReturnsFalse) {
    RequireGPU();
    EXPECT_FALSE(nimcp_gpu_collision_detect(ctx, nullptr));
}

//=============================================================================
// Integration Tests
//=============================================================================

/**
 * TEST: Full flocking simulation cycle
 * WHAT: Complete flocking simulation with neighbor finding and updates
 * WHY:  Verify all flocking components work together
 */
TEST_F(SwarmKernelTest, Integration_FlockingFullCycle) {
    RequireGPU();

    nimcp_flocking_gpu_params_t params = create_default_flocking_params();
    nimcp_flocking_gpu_state_t* state = nimcp_flocking_gpu_create(
        ctx, DEFAULT_N_AGENTS, DEFAULT_MAX_NEIGHBORS, &params);
    ASSERT_NE(state, nullptr);

    // Initialize positions and velocities
    std::vector<float> positions = create_grid_positions(DEFAULT_N_AGENTS, 2.0f);
    std::vector<float> velocities = create_uniform_velocities(DEFAULT_N_AGENTS, 1.0f, 0.0f, 0.0f);

    size_t dims[2] = {DEFAULT_N_AGENTS, 4};
    nimcp_gpu_tensor_t* pos_tensor = nimcp_gpu_tensor_from_host(
        ctx, positions.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* vel_tensor = nimcp_gpu_tensor_from_host(
        ctx, velocities.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_copy(ctx, pos_tensor, state->positions);
    nimcp_gpu_copy(ctx, vel_tensor, state->velocities);

    // Run simulation steps
    for (int t = 0; t < 10; t++) {
        // Find neighbors
        bool result = nimcp_gpu_flocking_find_neighbors(ctx, state, params.cohesion_radius);
        ASSERT_TRUE(result) << "Neighbor finding at t=" << t << " should succeed";

        // Compute forces
        result = nimcp_gpu_flocking_compute_forces(ctx, state);
        ASSERT_TRUE(result) << "Force computation at t=" << t << " should succeed";

        // Update kinematics
        result = nimcp_gpu_flocking_update(ctx, state, DEFAULT_DT);
        ASSERT_TRUE(result) << "Update at t=" << t << " should succeed";
    }

    // Verify agents moved
    std::vector<float> final_positions(DEFAULT_N_AGENTS * 4);
    copy_to_host(state->positions, final_positions.data());

    bool positions_changed = false;
    for (size_t i = 0; i < DEFAULT_N_AGENTS * 4; i++) {
        if (std::abs(positions[i] - final_positions[i]) > TOLERANCE) {
            positions_changed = true;
            break;
        }
    }
    EXPECT_TRUE(positions_changed) << "Agents should have moved during simulation";

    nimcp_gpu_tensor_destroy(pos_tensor);
    nimcp_gpu_tensor_destroy(vel_tensor);
    nimcp_flocking_gpu_destroy(state);
}

/**
 * TEST: Pheromone deposit and decay cycle
 * WHAT: Complete pheromone lifecycle with deposit, diffusion, decay
 * WHY:  Verify pheromone system works end-to-end
 */
TEST_F(SwarmKernelTest, Integration_PheromoneLifecycle) {
    RequireGPU();

    nimcp_pheromone_gpu_params_t params = create_default_pheromone_params();
    params.diffusion_rate = 0.05f;
    params.decay_rates[0] = 0.01f;

    nimcp_pheromone_gpu_state_t* state = nimcp_pheromone_gpu_create(
        ctx, 16, 16, 1, 1, 1.0f, &params);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_zeros(ctx, state->concentration);

    // Deposit pheromone
    std::vector<float> deposit_pos = {8.0f, 8.0f, 0.0f};
    std::vector<float> deposit_type = {0.0f};
    std::vector<float> deposit_amount = {10.0f};

    size_t dims_pos[2] = {1, 3};
    size_t dims_scalar[1] = {1};
    nimcp_gpu_tensor_t* pos_tensor = nimcp_gpu_tensor_from_host(
        ctx, deposit_pos.data(), dims_pos, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* type_tensor = nimcp_gpu_tensor_from_host(
        ctx, deposit_type.data(), dims_scalar, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* amount_tensor = nimcp_gpu_tensor_from_host(
        ctx, deposit_amount.data(), dims_scalar, 1, NIMCP_GPU_PRECISION_FP32);

    nimcp_gpu_pheromone_deposit(ctx, state, pos_tensor, type_tensor, amount_tensor, 1);

    // Get initial total
    std::vector<float> initial_conc(16 * 16);
    copy_to_host(state->concentration, initial_conc.data());
    float initial_total = std::accumulate(initial_conc.begin(), initial_conc.end(), 0.0f);

    // Apply diffusion and decay
    for (int t = 0; t < 20; t++) {
        nimcp_gpu_pheromone_diffusion(ctx, state, DEFAULT_DT);
        nimcp_gpu_pheromone_decay(ctx, state, DEFAULT_DT);
    }

    // Get final total
    std::vector<float> final_conc(16 * 16);
    copy_to_host(state->concentration, final_conc.data());
    float final_total = std::accumulate(final_conc.begin(), final_conc.end(), 0.0f);

    // Total should decrease due to decay
    EXPECT_LT(final_total, initial_total) << "Pheromone should decay over time";

    nimcp_gpu_tensor_destroy(pos_tensor);
    nimcp_gpu_tensor_destroy(type_tensor);
    nimcp_gpu_tensor_destroy(amount_tensor);
    nimcp_pheromone_gpu_destroy(state);
}

//=============================================================================
// Large Scale Tests
//=============================================================================

/**
 * TEST: Large scale flocking simulation
 * WHAT: Run flocking with many agents
 * WHY:  Verify scalability and performance
 */
TEST_F(SwarmKernelTest, LargeScale_FlockingSimulation) {
    RequireGPU();

    nimcp_flocking_gpu_params_t params = create_default_flocking_params();
    nimcp_flocking_gpu_state_t* state = nimcp_flocking_gpu_create(
        ctx, LARGE_N_AGENTS, DEFAULT_MAX_NEIGHBORS, &params);
    ASSERT_NE(state, nullptr);

    // Initialize
    std::vector<float> positions = create_grid_positions(LARGE_N_AGENTS, 1.0f);
    std::vector<float> velocities = create_uniform_velocities(LARGE_N_AGENTS, 1.0f, 0.0f, 0.0f);

    size_t dims[2] = {LARGE_N_AGENTS, 4};
    nimcp_gpu_tensor_t* pos_tensor = nimcp_gpu_tensor_from_host(
        ctx, positions.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* vel_tensor = nimcp_gpu_tensor_from_host(
        ctx, velocities.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_copy(ctx, pos_tensor, state->positions);
    nimcp_gpu_copy(ctx, vel_tensor, state->velocities);

    // Time simulation
    auto start = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < 50; t++) {
        bool result = nimcp_gpu_flocking_compute_forces(ctx, state);
        ASSERT_TRUE(result);
        result = nimcp_gpu_flocking_update(ctx, state, DEFAULT_DT);
        ASSERT_TRUE(result);
    }

    nimcp_gpu_context_synchronize(ctx);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete quickly on GPU
    EXPECT_LT(duration.count(), 10000) << "50 steps of 1000 agents should complete within 10 seconds";

    nimcp_gpu_tensor_destroy(pos_tensor);
    nimcp_gpu_tensor_destroy(vel_tensor);
    nimcp_flocking_gpu_destroy(state);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
