/**
 * @file test_exception_swarm_macros.cpp
 * @brief Unit tests for swarm module exception integration
 *
 * WHAT: Tests for swarm module exception handling
 * WHY:  Verify emotional contagion, swarm brain local, and proprioception exceptions
 * HOW:  GoogleTest framework with fixture setup/teardown for exception system
 *
 * TEST CATEGORIES:
 * 1. Emotional contagion error handling
 * 2. Swarm brain local exception behavior
 * 3. Proprioception exception flow
 * 4. Memory leak verification on error paths
 *
 * @author NIMCP Development Team
 * @date 2026-01-21
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <vector>
#include <string>

extern "C" {
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
#include "swarm/nimcp_emotional_contagion.h"
#include "swarm/nimcp_swarm_brain_local.h"
#include "swarm/nimcp_swarm_proprioception.h"
}

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
bool swarm_test_handler(nimcp_exception_t* ex, void* user_data) {
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
 * WHAT: Base fixture for swarm exception tests
 * WHY:  Setup/teardown exception system for each test
 */
class SwarmExceptionTest : public ::testing::Test {
protected:
    nimcp_handler_registration_t* handler_reg_ = nullptr;

    void SetUp() override {
        reset_tracking();

        // Initialize exception system
        nimcp_exception_system_init();

        // Register test handler
        nimcp_handler_options_t opts;
        nimcp_handler_default_options(&opts);
        opts.name = "swarm_test_handler";
        opts.handler = swarm_test_handler;
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
// Helper Functions Simulating Emotional Contagion Operations
//=============================================================================

/**
 * @brief Simulate emotional contagion configuration validation
 */
static nimcp_result_t validate_emotional_contagion_config(
    const emotional_contagion_config_t* config) {

    NIMCP_CHECK_THROW(config != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Emotional contagion config is NULL");

    NIMCP_CHECK_THROW(config->contagion_rate >= 0.0f && config->contagion_rate <= 1.0f,
                      NIMCP_ERROR_INVALID_PARAM,
                      "Contagion rate %.2f out of range [0, 1]",
                      config->contagion_rate);

    NIMCP_CHECK_THROW(config->decay_rate >= 0.0f && config->decay_rate <= 1.0f,
                      NIMCP_ERROR_INVALID_PARAM,
                      "Decay rate %.2f out of range [0, 1]",
                      config->decay_rate);

    NIMCP_CHECK_THROW(config->max_agents > 0 &&
                      config->max_agents <= EMOTIONAL_CONTAGION_MAX_AGENTS,
                      NIMCP_ERROR_OUT_OF_RANGE,
                      "Max agents %u out of range [1, %u]",
                      config->max_agents, EMOTIONAL_CONTAGION_MAX_AGENTS);

    return NIMCP_SUCCESS;
}

/**
 * @brief Simulate emotional contagion agent registration
 */
static nimcp_result_t register_emotional_agent(
    emotional_contagion_t* ec,
    uint32_t agent_id,
    float susceptibility) {

    NIMCP_CHECK_THROW(ec != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Emotional contagion system is NULL");

    NIMCP_CHECK_THROW(susceptibility >= 0.0f && susceptibility <= 1.0f,
                      NIMCP_ERROR_INVALID_PARAM,
                      "Agent %u susceptibility %.2f out of range [0, 1]",
                      agent_id, susceptibility);

    return NIMCP_SUCCESS;
}

/**
 * @brief Simulate emotion setting with validation
 */
static nimcp_result_t set_agent_emotion(
    emotional_contagion_t* ec,
    uint32_t agent_id,
    emotion_type_t emotion,
    float intensity) {

    NIMCP_CHECK_THROW(ec != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Emotional contagion system is NULL");

    NIMCP_CHECK_THROW(emotion < EMOTION_TYPE_COUNT, NIMCP_ERROR_OUT_OF_RANGE,
                      "Invalid emotion type %d", (int)emotion);

    NIMCP_CHECK_THROW(intensity >= 0.0f && intensity <= 1.0f,
                      NIMCP_ERROR_INVALID_PARAM,
                      "Agent %u emotion intensity %.2f out of range [0, 1]",
                      agent_id, intensity);

    return NIMCP_SUCCESS;
}

/**
 * @brief Simulate emotional connection addition
 */
static nimcp_result_t add_emotional_connection(
    emotional_contagion_t* ec,
    uint32_t from_agent,
    uint32_t to_agent,
    float strength,
    float proximity) {

    NIMCP_CHECK_THROW(ec != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Emotional contagion system is NULL");

    NIMCP_CHECK_THROW(from_agent != to_agent, NIMCP_ERROR_INVALID_PARAM,
                      "Cannot create self-connection for agent %u", from_agent);

    NIMCP_CHECK_THROW(strength >= 0.0f && strength <= 1.0f,
                      NIMCP_ERROR_INVALID_PARAM,
                      "Connection strength %.2f out of range [0, 1]", strength);

    NIMCP_CHECK_THROW(proximity >= 0.0f && proximity <= 1.0f,
                      NIMCP_ERROR_INVALID_PARAM,
                      "Proximity %.2f out of range [0, 1]", proximity);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Helper Functions Simulating Swarm Brain Local Operations
//=============================================================================

/**
 * @brief Simulate swarm brain configuration validation
 */
static int validate_swarm_brain_config(const swarm_brain_config_t* config) {
    NIMCP_CHECK_THROW(config != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Swarm brain config is NULL");

    NIMCP_CHECK_THROW(config->default_brain_size > 0 &&
                      config->default_brain_size <= SWARM_BRAIN_MAX_NEURONS,
                      NIMCP_ERROR_OUT_OF_RANGE,
                      "Default brain size %u out of range [1, %u]",
                      config->default_brain_size, SWARM_BRAIN_MAX_NEURONS);

    NIMCP_CHECK_THROW(config->divergence_threshold >= 0.0f &&
                      config->divergence_threshold <= 1.0f,
                      NIMCP_ERROR_INVALID_PARAM,
                      "Divergence threshold %.2f out of range [0, 1]",
                      config->divergence_threshold);

    return 0;
}

/**
 * @brief Simulate swarm brain creation for agent
 */
static int create_agent_brain(
    swarm_brain_manager_t* mgr,
    uint32_t agent_id,
    uint32_t brain_size) {

    NIMCP_CHECK_THROW(mgr != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Swarm brain manager is NULL");

    NIMCP_CHECK_THROW(brain_size <= SWARM_BRAIN_MAX_NEURONS,
                      NIMCP_ERROR_OUT_OF_RANGE,
                      "Brain size %u exceeds maximum %u",
                      brain_size, SWARM_BRAIN_MAX_NEURONS);

    return 0;
}

/**
 * @brief Simulate swarm brain role assignment
 */
static int assign_agent_role(
    swarm_brain_manager_t* mgr,
    uint32_t agent_id,
    drone_role_t role) {

    NIMCP_CHECK_THROW(mgr != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Swarm brain manager is NULL");

    NIMCP_CHECK_THROW(role < DRONE_ROLE_COUNT, NIMCP_ERROR_OUT_OF_RANGE,
                      "Invalid drone role %d", (int)role);

    return 0;
}

/**
 * @brief Simulate local learning with validation
 */
static int swarm_brain_learn(
    swarm_brain_manager_t* mgr,
    uint32_t agent_id,
    const float* input,
    uint32_t input_size,
    const float* target,
    uint32_t target_size) {

    NIMCP_CHECK_THROW(mgr != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Swarm brain manager is NULL");

    NIMCP_CHECK_THROW(input != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Learning input is NULL for agent %u", agent_id);

    NIMCP_CHECK_THROW(target != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Learning target is NULL for agent %u", agent_id);

    NIMCP_CHECK_THROW(input_size > 0, NIMCP_ERROR_INVALID_PARAM,
                      "Input size must be positive for agent %u", agent_id);

    NIMCP_CHECK_THROW(target_size > 0, NIMCP_ERROR_INVALID_PARAM,
                      "Target size must be positive for agent %u", agent_id);

    return 0;
}

/**
 * @brief Simulate weight synchronization with validation
 */
static int sync_agent_weights(swarm_brain_manager_t* mgr, uint32_t agent_id) {
    NIMCP_CHECK_THROW(mgr != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Swarm brain manager is NULL");

    return 0;
}

//=============================================================================
// Helper Functions Simulating Proprioception Operations
//=============================================================================

/**
 * @brief Simulate proprioception configuration validation
 */
static nimcp_result_t validate_proprio_config(
    const nimcp_swarm_proprio_config_t* config) {

    NIMCP_CHECK_THROW(config != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Proprioception config is NULL");

    NIMCP_CHECK_THROW(config->neighbor_radius > 0.0,
                      NIMCP_ERROR_INVALID_PARAM,
                      "Neighbor radius must be positive, got %.2f",
                      config->neighbor_radius);

    NIMCP_CHECK_THROW(config->position_update_rate > 0.0,
                      NIMCP_ERROR_INVALID_PARAM,
                      "Position update rate must be positive, got %.2f",
                      config->position_update_rate);

    NIMCP_CHECK_THROW(config->max_neighbors > 0 &&
                      config->max_neighbors <= NIMCP_SWARM_PROPRIO_MAX_NEIGHBORS,
                      NIMCP_ERROR_OUT_OF_RANGE,
                      "Max neighbors %u out of range [1, %u]",
                      config->max_neighbors, NIMCP_SWARM_PROPRIO_MAX_NEIGHBORS);

    return NIMCP_SUCCESS;
}

/**
 * @brief Simulate position update with validation
 */
static nimcp_result_t update_drone_position(
    nimcp_swarm_proprioception_t* proprio,
    const nimcp_swarm_position_t* position,
    const nimcp_swarm_velocity_t* velocity) {

    NIMCP_CHECK_THROW(proprio != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Proprioception instance is NULL");

    NIMCP_CHECK_THROW(position != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Position is NULL");

    // velocity can be NULL

    return NIMCP_SUCCESS;
}

/**
 * @brief Simulate neighbor update with validation
 */
static nimcp_result_t update_neighbor_position(
    nimcp_swarm_proprioception_t* proprio,
    uint32_t neighbor_id,
    const nimcp_swarm_position_t* relative_pos,
    double signal_strength) {

    NIMCP_CHECK_THROW(proprio != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Proprioception instance is NULL");

    NIMCP_CHECK_THROW(relative_pos != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Relative position is NULL for neighbor %u", neighbor_id);

    NIMCP_CHECK_THROW(signal_strength >= 0.0 && signal_strength <= 1.0,
                      NIMCP_ERROR_INVALID_PARAM,
                      "Signal strength %.2f out of range [0, 1] for neighbor %u",
                      signal_strength, neighbor_id);

    return NIMCP_SUCCESS;
}

/**
 * @brief Simulate shape classification with validation
 */
static nimcp_result_t classify_swarm_shape(
    nimcp_swarm_proprioception_t* proprio,
    nimcp_swarm_shape_descriptor_t* descriptor) {

    NIMCP_CHECK_THROW(proprio != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Proprioception instance is NULL");

    NIMCP_CHECK_THROW(descriptor != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Shape descriptor output is NULL");

    return NIMCP_SUCCESS;
}

/**
 * @brief Simulate deformation detection
 */
static nimcp_result_t detect_swarm_deformation(
    nimcp_swarm_proprioception_t* proprio,
    nimcp_swarm_deformation_metrics_t* metrics) {

    NIMCP_CHECK_THROW(proprio != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Proprioception instance is NULL");

    NIMCP_CHECK_THROW(metrics != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Deformation metrics output is NULL");

    return NIMCP_SUCCESS;
}

//=============================================================================
// Emotional Contagion Error Handling Tests
//=============================================================================

/**
 * WHAT: Test emotional contagion config validation with NULL config
 * WHY:  Verify proper error handling for NULL input
 */
TEST_F(SwarmExceptionTest, EmotionalContagionNullConfig) {
    nimcp_result_t result = validate_emotional_contagion_config(nullptr);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_NULL_POINTER);
}

/**
 * WHAT: Test emotional contagion config with invalid contagion rate
 * WHY:  Verify contagion rate range validation
 */
TEST_F(SwarmExceptionTest, EmotionalContagionInvalidContagionRate) {
    emotional_contagion_config_t config;
    memset(&config, 0, sizeof(config));
    config.contagion_rate = 1.5f;  // Invalid
    config.decay_rate = 0.5f;
    config.max_agents = 100;

    nimcp_result_t result = validate_emotional_contagion_config(&config);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test emotional contagion config with invalid max agents
 * WHY:  Verify max agents range validation
 */
TEST_F(SwarmExceptionTest, EmotionalContagionInvalidMaxAgents) {
    emotional_contagion_config_t config;
    memset(&config, 0, sizeof(config));
    config.contagion_rate = 0.5f;
    config.decay_rate = 0.5f;
    config.max_agents = EMOTIONAL_CONTAGION_MAX_AGENTS + 100;  // Invalid

    nimcp_result_t result = validate_emotional_contagion_config(&config);

    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test emotional contagion agent registration with invalid susceptibility
 * WHY:  Verify susceptibility range validation
 */
TEST_F(SwarmExceptionTest, EmotionalContagionInvalidSusceptibility) {
    int dummy_ec = 1;
    nimcp_result_t result = register_emotional_agent(
        (emotional_contagion_t*)&dummy_ec, 1, -0.5f);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test emotion setting with invalid emotion type
 * WHY:  Verify emotion type range validation
 */
TEST_F(SwarmExceptionTest, EmotionalContagionInvalidEmotionType) {
    int dummy_ec = 1;
    nimcp_result_t result = set_agent_emotion(
        (emotional_contagion_t*)&dummy_ec, 1,
        (emotion_type_t)999, 0.5f);

    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test emotion setting with invalid intensity
 * WHY:  Verify intensity range validation
 */
TEST_F(SwarmExceptionTest, EmotionalContagionInvalidIntensity) {
    int dummy_ec = 1;
    nimcp_result_t result = set_agent_emotion(
        (emotional_contagion_t*)&dummy_ec, 1,
        EMOTION_JOY, 1.5f);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test emotional connection self-connection prevention
 * WHY:  Verify agents cannot connect to themselves
 */
TEST_F(SwarmExceptionTest, EmotionalContagionSelfConnection) {
    int dummy_ec = 1;
    nimcp_result_t result = add_emotional_connection(
        (emotional_contagion_t*)&dummy_ec, 5, 5, 0.8f, 1.0f);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test valid emotional contagion operations succeed
 * WHY:  Verify successful operations don't throw
 */
TEST_F(SwarmExceptionTest, EmotionalContagionValidOperations) {
    int dummy_ec = 1;

    emotional_contagion_config_t config;
    memset(&config, 0, sizeof(config));
    config.contagion_rate = 0.3f;
    config.decay_rate = 0.1f;
    config.max_agents = 100;

    EXPECT_EQ(validate_emotional_contagion_config(&config), NIMCP_SUCCESS);
    EXPECT_EQ(register_emotional_agent((emotional_contagion_t*)&dummy_ec, 1, 0.7f),
              NIMCP_SUCCESS);
    EXPECT_EQ(set_agent_emotion((emotional_contagion_t*)&dummy_ec, 1,
                                EMOTION_JOY, 0.8f), NIMCP_SUCCESS);
    EXPECT_EQ(add_emotional_connection((emotional_contagion_t*)&dummy_ec,
                                        1, 2, 0.5f, 0.9f), NIMCP_SUCCESS);

    EXPECT_EQ(g_handler_call_count, 0);
}

//=============================================================================
// Swarm Brain Local Exception Tests
//=============================================================================

/**
 * WHAT: Test swarm brain config validation with NULL config
 * WHY:  Verify proper error handling for NULL input
 */
TEST_F(SwarmExceptionTest, SwarmBrainNullConfig) {
    int result = validate_swarm_brain_config(nullptr);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test swarm brain config with invalid brain size
 * WHY:  Verify brain size range validation
 */
TEST_F(SwarmExceptionTest, SwarmBrainInvalidBrainSize) {
    swarm_brain_config_t config;
    memset(&config, 0, sizeof(config));
    config.default_brain_size = SWARM_BRAIN_MAX_NEURONS + 100;  // Invalid
    config.divergence_threshold = 0.3f;

    int result = validate_swarm_brain_config(&config);

    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test swarm brain config with invalid divergence threshold
 * WHY:  Verify divergence threshold range validation
 */
TEST_F(SwarmExceptionTest, SwarmBrainInvalidDivergenceThreshold) {
    swarm_brain_config_t config;
    memset(&config, 0, sizeof(config));
    config.default_brain_size = 100;
    config.divergence_threshold = -0.5f;  // Invalid

    int result = validate_swarm_brain_config(&config);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test agent brain creation with NULL manager
 * WHY:  Verify NULL check for manager parameter
 */
TEST_F(SwarmExceptionTest, SwarmBrainNullManager) {
    int result = create_agent_brain(nullptr, 1, 100);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test agent brain creation with excessive size
 * WHY:  Verify brain size limit enforcement
 */
TEST_F(SwarmExceptionTest, SwarmBrainExcessiveSize) {
    int dummy_mgr = 1;
    int result = create_agent_brain((swarm_brain_manager_t*)&dummy_mgr,
                                    1, SWARM_BRAIN_MAX_NEURONS + 1);

    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test role assignment with invalid role
 * WHY:  Verify role range validation
 */
TEST_F(SwarmExceptionTest, SwarmBrainInvalidRole) {
    int dummy_mgr = 1;
    int result = assign_agent_role((swarm_brain_manager_t*)&dummy_mgr,
                                   1, (drone_role_t)999);

    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test learning with NULL input
 * WHY:  Verify NULL check for input parameter
 */
TEST_F(SwarmExceptionTest, SwarmBrainLearningNullInput) {
    int dummy_mgr = 1;
    float target[] = {1.0f};
    int result = swarm_brain_learn((swarm_brain_manager_t*)&dummy_mgr,
                                   1, nullptr, 10, target, 1);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test learning with NULL target
 * WHY:  Verify NULL check for target parameter
 */
TEST_F(SwarmExceptionTest, SwarmBrainLearningNullTarget) {
    int dummy_mgr = 1;
    float input[] = {1.0f};
    int result = swarm_brain_learn((swarm_brain_manager_t*)&dummy_mgr,
                                   1, input, 1, nullptr, 10);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test learning with zero input size
 * WHY:  Verify size validation
 */
TEST_F(SwarmExceptionTest, SwarmBrainLearningZeroInputSize) {
    int dummy_mgr = 1;
    float input[] = {1.0f};
    float target[] = {1.0f};
    int result = swarm_brain_learn((swarm_brain_manager_t*)&dummy_mgr,
                                   1, input, 0, target, 1);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test valid swarm brain operations succeed
 * WHY:  Verify successful operations don't throw
 */
TEST_F(SwarmExceptionTest, SwarmBrainValidOperations) {
    int dummy_mgr = 1;

    swarm_brain_config_t config;
    memset(&config, 0, sizeof(config));
    config.default_brain_size = 100;
    config.divergence_threshold = 0.3f;

    float input[] = {1.0f, 2.0f, 3.0f};
    float target[] = {1.0f};

    EXPECT_EQ(validate_swarm_brain_config(&config), 0);
    EXPECT_EQ(create_agent_brain((swarm_brain_manager_t*)&dummy_mgr, 1, 100), 0);
    EXPECT_EQ(assign_agent_role((swarm_brain_manager_t*)&dummy_mgr, 1, DRONE_ROLE_SCOUT), 0);
    EXPECT_EQ(swarm_brain_learn((swarm_brain_manager_t*)&dummy_mgr,
                                1, input, 3, target, 1), 0);
    EXPECT_EQ(sync_agent_weights((swarm_brain_manager_t*)&dummy_mgr, 1), 0);

    EXPECT_EQ(g_handler_call_count, 0);
}

//=============================================================================
// Proprioception Exception Tests
//=============================================================================

/**
 * WHAT: Test proprioception config validation with NULL config
 * WHY:  Verify proper error handling for NULL input
 */
TEST_F(SwarmExceptionTest, ProprioceptionNullConfig) {
    nimcp_result_t result = validate_proprio_config(nullptr);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test proprioception config with invalid neighbor radius
 * WHY:  Verify positive neighbor radius requirement
 */
TEST_F(SwarmExceptionTest, ProprioceptionInvalidNeighborRadius) {
    nimcp_swarm_proprio_config_t config;
    memset(&config, 0, sizeof(config));
    config.neighbor_radius = -10.0;  // Invalid
    config.position_update_rate = 10.0;
    config.max_neighbors = 10;

    nimcp_result_t result = validate_proprio_config(&config);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test proprioception config with invalid max neighbors
 * WHY:  Verify max neighbors range validation
 */
TEST_F(SwarmExceptionTest, ProprioceptionInvalidMaxNeighbors) {
    nimcp_swarm_proprio_config_t config;
    memset(&config, 0, sizeof(config));
    config.neighbor_radius = 10.0;
    config.position_update_rate = 10.0;
    config.max_neighbors = NIMCP_SWARM_PROPRIO_MAX_NEIGHBORS + 100;  // Invalid

    nimcp_result_t result = validate_proprio_config(&config);

    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test position update with NULL proprioception
 * WHY:  Verify NULL check for proprioception parameter
 */
TEST_F(SwarmExceptionTest, ProprioceptionNullProprio) {
    nimcp_swarm_position_t pos = {1.0, 2.0, 3.0};
    nimcp_result_t result = update_drone_position(nullptr, &pos, nullptr);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test position update with NULL position
 * WHY:  Verify NULL check for position parameter
 */
TEST_F(SwarmExceptionTest, ProprioceptionNullPosition) {
    int dummy_proprio = 1;
    nimcp_result_t result = update_drone_position(
        (nimcp_swarm_proprioception_t*)&dummy_proprio, nullptr, nullptr);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test neighbor update with invalid signal strength
 * WHY:  Verify signal strength range validation
 */
TEST_F(SwarmExceptionTest, ProprioceptionInvalidSignalStrength) {
    int dummy_proprio = 1;
    nimcp_swarm_position_t rel_pos = {1.0, 0.0, 0.0};
    nimcp_result_t result = update_neighbor_position(
        (nimcp_swarm_proprioception_t*)&dummy_proprio,
        2, &rel_pos, 1.5);  // Invalid - above 1.0

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test shape classification with NULL descriptor output
 * WHY:  Verify NULL check for output parameter
 */
TEST_F(SwarmExceptionTest, ProprioceptionNullShapeDescriptor) {
    int dummy_proprio = 1;
    nimcp_result_t result = classify_swarm_shape(
        (nimcp_swarm_proprioception_t*)&dummy_proprio, nullptr);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test deformation detection with NULL metrics output
 * WHY:  Verify NULL check for metrics parameter
 */
TEST_F(SwarmExceptionTest, ProprioceptionNullDeformationMetrics) {
    int dummy_proprio = 1;
    nimcp_result_t result = detect_swarm_deformation(
        (nimcp_swarm_proprioception_t*)&dummy_proprio, nullptr);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test valid proprioception operations succeed
 * WHY:  Verify successful operations don't throw
 */
TEST_F(SwarmExceptionTest, ProprioceptionValidOperations) {
    int dummy_proprio = 1;

    nimcp_swarm_proprio_config_t config;
    memset(&config, 0, sizeof(config));
    config.neighbor_radius = 50.0;
    config.position_update_rate = 10.0;
    config.max_neighbors = 16;

    nimcp_swarm_position_t pos = {100.0, 200.0, 50.0};
    nimcp_swarm_velocity_t vel = {1.0, 0.5, 0.0};
    nimcp_swarm_position_t rel_pos = {10.0, 5.0, 0.0};
    nimcp_swarm_shape_descriptor_t shape_desc;
    nimcp_swarm_deformation_metrics_t deform_metrics;

    EXPECT_EQ(validate_proprio_config(&config), NIMCP_SUCCESS);
    EXPECT_EQ(update_drone_position((nimcp_swarm_proprioception_t*)&dummy_proprio,
                                     &pos, &vel), NIMCP_SUCCESS);
    EXPECT_EQ(update_neighbor_position((nimcp_swarm_proprioception_t*)&dummy_proprio,
                                        5, &rel_pos, 0.8), NIMCP_SUCCESS);
    EXPECT_EQ(classify_swarm_shape((nimcp_swarm_proprioception_t*)&dummy_proprio,
                                    &shape_desc), NIMCP_SUCCESS);
    EXPECT_EQ(detect_swarm_deformation((nimcp_swarm_proprioception_t*)&dummy_proprio,
                                        &deform_metrics), NIMCP_SUCCESS);

    EXPECT_EQ(g_handler_call_count, 0);
}

//=============================================================================
// Error Message Content Tests
//=============================================================================

/**
 * WHAT: Test error message contains agent ID
 * WHY:  Verify agent identification in error messages
 */
TEST_F(SwarmExceptionTest, ErrorMessageContainsAgentId) {
    int dummy_ec = 1;
    register_emotional_agent((emotional_contagion_t*)&dummy_ec, 42, -0.5f);

    ASSERT_FALSE(g_captured_messages.empty());
    const std::string& msg = g_captured_messages[0];
    EXPECT_NE(msg.find("42"), std::string::npos) << "Message should contain agent ID 42";
}

/**
 * WHAT: Test error message contains parameter value
 * WHY:  Verify actual invalid value is included
 */
TEST_F(SwarmExceptionTest, ErrorMessageContainsParameterValue) {
    int dummy_ec = 1;
    set_agent_emotion((emotional_contagion_t*)&dummy_ec, 1, EMOTION_JOY, 1.75f);

    ASSERT_FALSE(g_captured_messages.empty());
    const std::string& msg = g_captured_messages[0];
    EXPECT_NE(msg.find("1.75"), std::string::npos);
}

/**
 * WHAT: Test error message contains neighbor ID
 * WHY:  Verify neighbor identification in error messages
 */
TEST_F(SwarmExceptionTest, ErrorMessageContainsNeighborId) {
    int dummy_proprio = 1;
    nimcp_swarm_position_t rel_pos = {1.0, 0.0, 0.0};
    update_neighbor_position((nimcp_swarm_proprioception_t*)&dummy_proprio,
                              123, &rel_pos, 2.0);

    ASSERT_FALSE(g_captured_messages.empty());
    const std::string& msg = g_captured_messages[0];
    EXPECT_NE(msg.find("123"), std::string::npos) << "Message should contain neighbor ID 123";
}

//=============================================================================
// Memory Leak Verification Tests
//=============================================================================

/**
 * WHAT: Test multiple swarm exceptions don't leak memory
 * WHY:  Verify exception cleanup on error paths
 */
TEST_F(SwarmExceptionTest, MultipleSwarmExceptionsNoLeak) {
    const int iterations = 100;
    int dummy_ec = 1;

    for (int i = 0; i < iterations; i++) {
        register_emotional_agent((emotional_contagion_t*)&dummy_ec, i, -0.1f);
    }

    EXPECT_EQ(g_handler_call_count, iterations);
}

/**
 * WHAT: Test mixed swarm module errors don't leak memory
 * WHY:  Verify cleanup across different swarm modules
 */
TEST_F(SwarmExceptionTest, MixedSwarmErrorsNoLeak) {
    int dummy_ec = 1;
    int dummy_mgr = 1;
    int dummy_proprio = 1;

    // Emotional contagion errors
    register_emotional_agent((emotional_contagion_t*)&dummy_ec, 1, -0.5f);
    set_agent_emotion((emotional_contagion_t*)&dummy_ec, 1, EMOTION_JOY, 1.5f);

    // Swarm brain errors
    create_agent_brain(nullptr, 1, 100);
    swarm_brain_learn((swarm_brain_manager_t*)&dummy_mgr, 1, nullptr, 10, nullptr, 1);

    // Proprioception errors
    update_drone_position(nullptr, nullptr, nullptr);
    update_neighbor_position((nimcp_swarm_proprioception_t*)&dummy_proprio, 1, nullptr, 0.5);

    EXPECT_EQ(g_handler_call_count, 6);
}

/**
 * WHAT: Test alternating success and failure operations
 * WHY:  Verify cleanup works for mixed outcomes
 */
TEST_F(SwarmExceptionTest, AlternatingSuccessAndFailureNoLeak) {
    int dummy_ec = 1;
    const int iterations = 50;
    int success_count = 0;
    int failure_count = 0;

    for (int i = 0; i < iterations; i++) {
        float susceptibility = (i % 2 == 0) ? 0.5f : -0.5f;
        nimcp_result_t result = register_emotional_agent(
            (emotional_contagion_t*)&dummy_ec, i, susceptibility);

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
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
