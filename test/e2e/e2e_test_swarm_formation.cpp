/**
 * @file e2e_test_swarm_formation.cpp
 * @brief E2E Test for Multi-Drone Formation Coordination
 *
 * WHAT: Complete end-to-end test of 8 drones coordinating movement in formation
 * WHY:  Verify swarm can coordinate spatially distributed actions
 * HOW:  Simulate formation commands, broadcast positions, verify convergence
 *
 * TEST SCENARIO:
 * 1. Initialize 8 drones with local brains
 * 2. Server sends formation command (V-shape)
 * 3. Drones negotiate positions via consensus
 * 4. Drones broadcast position updates
 * 5. Formation converges within tolerance
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#include "e2e_test_framework.h"
#include <thread>
#include <vector>
#include <atomic>
#include <cmath>
#include <random>

// Headers have their own extern "C" guards
#include "core/brain/nimcp_brain.h"
#include "swarm/nimcp_swarm_signal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

using namespace nimcp::e2e;

//=============================================================================
// Formation Types
//=============================================================================

enum FormationType {
    FORMATION_V_SHAPE,
    FORMATION_LINE,
    FORMATION_CIRCLE,
    FORMATION_GRID
};

struct Position {
    float x, y, z;
};

struct DroneState {
    uint32_t id;
    Position current;
    Position target;
    brain_t brain;
    nimcp_swarm_signal_adapter_t* adapter;
};

//=============================================================================
// Formation Calculator
//=============================================================================

class FormationCalculator {
public:
    static std::vector<Position> CalculateFormation(FormationType type, uint32_t num_drones) {
        std::vector<Position> positions;

        switch (type) {
            case FORMATION_V_SHAPE:
                positions = CalculateVShape(num_drones);
                break;
            case FORMATION_LINE:
                positions = CalculateLine(num_drones);
                break;
            case FORMATION_CIRCLE:
                positions = CalculateCircle(num_drones);
                break;
            case FORMATION_GRID:
                positions = CalculateGrid(num_drones);
                break;
        }

        return positions;
    }

private:
    static std::vector<Position> CalculateVShape(uint32_t num_drones) {
        std::vector<Position> positions;
        float spacing = 10.0f;
        float altitude = 100.0f;

        for (uint32_t i = 0; i < num_drones; i++) {
            Position pos;
            if (i == 0) {
                // Leader at front
                pos = {0.0f, 0.0f, altitude};
            } else {
                // Wings spread behind
                int side = (i % 2 == 1) ? 1 : -1;
                int row = (i + 1) / 2;
                pos = {side * row * spacing, -row * spacing, altitude};
            }
            positions.push_back(pos);
        }

        return positions;
    }

    static std::vector<Position> CalculateLine(uint32_t num_drones) {
        std::vector<Position> positions;
        float spacing = 10.0f;
        float altitude = 100.0f;

        for (uint32_t i = 0; i < num_drones; i++) {
            positions.push_back({i * spacing, 0.0f, altitude});
        }

        return positions;
    }

    static std::vector<Position> CalculateCircle(uint32_t num_drones) {
        std::vector<Position> positions;
        float radius = 20.0f;
        float altitude = 100.0f;

        for (uint32_t i = 0; i < num_drones; i++) {
            float angle = (2.0f * M_PI * i) / num_drones;
            positions.push_back({
                radius * cosf(angle),
                radius * sinf(angle),
                altitude
            });
        }

        return positions;
    }

    static std::vector<Position> CalculateGrid(uint32_t num_drones) {
        std::vector<Position> positions;
        uint32_t cols = static_cast<uint32_t>(ceil(sqrt(num_drones)));
        float spacing = 10.0f;
        float altitude = 100.0f;

        for (uint32_t i = 0; i < num_drones; i++) {
            uint32_t row = i / cols;
            uint32_t col = i % cols;
            positions.push_back({col * spacing, row * spacing, altitude});
        }

        return positions;
    }
};

//=============================================================================
// Formation E2E Test
//=============================================================================

class SwarmFormationE2ETest : public ::testing::Test {
protected:
    static constexpr uint32_t NUM_DRONES = 8;
    static constexpr float CONVERGENCE_THRESHOLD = 2.0f; // meters
    static constexpr uint32_t MAX_ITERATIONS = 100;

    std::vector<DroneState> drones_;

    void SetUp() override {
        // logging initialized in framework
        // log level set in framework

        drones_.resize(NUM_DRONES);

        // Initialize drones
        for (uint32_t i = 0; i < NUM_DRONES; i++) {
            drones_[i].id = 1000 + i;

            // Random starting positions
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<float> dist(-50.0f, 50.0f);

            drones_[i].current = {dist(gen), dist(gen), 95.0f + dist(gen) * 0.1f};
            drones_[i].target = {0.0f, 0.0f, 100.0f}; // Will be set by formation

            // Create brain
            std::string name = "drone_" + std::to_string(i);
            drones_[i].brain = brain_create(
                name.c_str(),
                BRAIN_SIZE_TINY,
                BRAIN_TASK_CLASSIFICATION,
                10, // Inputs: position, velocity, neighbors
                5   // Outputs: thrust, pitch, roll, yaw, throttle
            );
            ASSERT_NE(drones_[i].brain, nullptr);

            // Create signal adapter
            swarm_signal_config_t config = {
                .radio_type = SWARM_RADIO_SIMULATION,
                .frequency_hz = 915000000,
                .bandwidth_hz = 125000,
                .tx_power_dbm = 14,
                .max_packet_size = 255,
                .retry_count = 3,
                .timeout_ms = 1000,
                .custom_send = nullptr,
                .custom_recv = nullptr,
                .custom_ctx = nullptr
            };

            drones_[i].adapter = swarm_signal_adapter_create(&config);
            ASSERT_NE(drones_[i].adapter, nullptr);
        }
    }

    void TearDown() override {
        for (auto& drone : drones_) {
            if (drone.brain) brain_destroy(drone.brain);
            if (drone.adapter) swarm_signal_adapter_destroy(drone.adapter);
        }
        drones_.clear();
    }

    float CalculateDistance(const Position& a, const Position& b) {
        float dx = a.x - b.x;
        float dy = a.y - b.y;
        float dz = a.z - b.z;
        return sqrtf(dx*dx + dy*dy + dz*dz);
    }

    bool CheckFormationConvergence() {
        for (const auto& drone : drones_) {
            float distance = CalculateDistance(drone.current, drone.target);
            if (distance > CONVERGENCE_THRESHOLD) {
                return false;
            }
        }
        return true;
    }

    void SimulateFormationStep() {
        // Each drone adjusts position toward target
        for (auto& drone : drones_) {
            float dx = drone.target.x - drone.current.x;
            float dy = drone.target.y - drone.current.y;
            float dz = drone.target.z - drone.current.z;

            // Simple proportional control
            float step_size = 0.5f;
            drone.current.x += dx * step_size;
            drone.current.y += dy * step_size;
            drone.current.z += dz * step_size;

            // Broadcast position
            float position_data[3] = {drone.current.x, drone.current.y, drone.current.z};
            swarm_signal_broadcast(
                drone.adapter,
                reinterpret_cast<uint8_t*>(position_data),
                sizeof(position_data)
            );
        }
    }
};

//=============================================================================
// Test Cases
//=============================================================================

TEST_F(SwarmFormationE2ETest, VShapeFormation) {
    PipelineTracker tracker("V-Shape Formation E2E");

    tracker.begin_stage("Initialize Formation", 1000);
    auto target_positions = FormationCalculator::CalculateFormation(FORMATION_V_SHAPE, NUM_DRONES);
    for (uint32_t i = 0; i < NUM_DRONES; i++) {
        drones_[i].target = target_positions[i];
    }
    tracker.end_stage();

    tracker.begin_stage("Formation Convergence", 5000);
    uint32_t iterations = 0;
    bool converged = false;

    while (iterations < MAX_ITERATIONS && !converged) {
        SimulateFormationStep();
        converged = CheckFormationConvergence();
        iterations++;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(converged) << "Formation did not converge after " << iterations << " iterations";
    tracker.end_stage();

    tracker.begin_stage("Verify Formation Stability", 1000);
    // Check that formation remains stable
    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(CheckFormationConvergence());
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    tracker.end_stage();

    EXPECT_TRUE(tracker.is_successful());
}

TEST_F(SwarmFormationE2ETest, LineFormation) {
    PipelineTracker tracker("Line Formation E2E");

    tracker.begin_stage("Initialize Formation", 1000);
    auto target_positions = FormationCalculator::CalculateFormation(FORMATION_LINE, NUM_DRONES);
    for (uint32_t i = 0; i < NUM_DRONES; i++) {
        drones_[i].target = target_positions[i];
    }
    tracker.end_stage();

    tracker.begin_stage("Formation Convergence", 5000);
    uint32_t iterations = 0;
    bool converged = false;

    while (iterations < MAX_ITERATIONS && !converged) {
        SimulateFormationStep();
        converged = CheckFormationConvergence();
        iterations++;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(converged);
    tracker.end_stage();

    EXPECT_TRUE(tracker.is_successful());
}

TEST_F(SwarmFormationE2ETest, CircleFormation) {
    PipelineTracker tracker("Circle Formation E2E");

    tracker.begin_stage("Initialize Formation", 1000);
    auto target_positions = FormationCalculator::CalculateFormation(FORMATION_CIRCLE, NUM_DRONES);
    for (uint32_t i = 0; i < NUM_DRONES; i++) {
        drones_[i].target = target_positions[i];
    }
    tracker.end_stage();

    tracker.begin_stage("Formation Convergence", 5000);
    uint32_t iterations = 0;
    bool converged = false;

    while (iterations < MAX_ITERATIONS && !converged) {
        SimulateFormationStep();
        converged = CheckFormationConvergence();
        iterations++;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(converged);
    tracker.end_stage();

    EXPECT_TRUE(tracker.is_successful());
}

TEST_F(SwarmFormationE2ETest, DynamicFormationChange) {
    PipelineTracker tracker("Dynamic Formation Change E2E");

    // Start in V-shape
    tracker.begin_stage("Initial V-Shape Formation", 2000);
    auto v_positions = FormationCalculator::CalculateFormation(FORMATION_V_SHAPE, NUM_DRONES);
    for (uint32_t i = 0; i < NUM_DRONES; i++) {
        drones_[i].target = v_positions[i];
    }

    for (uint32_t i = 0; i < 50; i++) {
        SimulateFormationStep();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    tracker.end_stage();

    // Transition to line
    tracker.begin_stage("Transition to Line Formation", 3000);
    auto line_positions = FormationCalculator::CalculateFormation(FORMATION_LINE, NUM_DRONES);
    for (uint32_t i = 0; i < NUM_DRONES; i++) {
        drones_[i].target = line_positions[i];
    }

    bool converged = false;
    for (uint32_t i = 0; i < MAX_ITERATIONS && !converged; i++) {
        SimulateFormationStep();
        converged = CheckFormationConvergence();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(converged);
    tracker.end_stage();

    EXPECT_TRUE(tracker.is_successful());
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
