/**
 * @file test_embodiment_integration.cpp
 * @brief Integration tests for embodiment module cross-subsystem interactions
 *
 * WHAT: Tests integration between affordance, body ownership, simulation,
 *       and interoceptive prediction subsystems
 * WHY:  Verify embodiment modules cooperate correctly when combined
 * HOW:  GTest with scenarios that exercise multiple subsystems together
 *
 * MODULES TESTED:
 *   - nimcp_affordance_processing.h (create/destroy, register object, detect, compete)
 *   - nimcp_body_ownership.h (create/destroy, add_part, update, boundaries)
 *   - nimcp_embodied_simulation.h (create/destroy, set_effector, start/run sim)
 *   - nimcp_interoceptive_prediction.h (create/destroy, register_system, process_signal)
 *
 * @date 2026-02-19
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "embodiment/nimcp_affordance_processing.h"
#include "embodiment/nimcp_body_ownership.h"
#include "embodiment/nimcp_embodied_simulation.h"
#include "embodiment/nimcp_interoceptive_prediction.h"
}

/* ============================================================================
 * Affordance + Body Ownership Integration
 * ============================================================================ */

class AffordanceBodyOwnershipIntegration : public ::testing::Test {
protected:
    nimcp_affordance_context_t* aff_ctx = nullptr;
    nimcp_body_context_t* body_ctx = nullptr;

    void SetUp() override {
        aff_ctx = nimcp_affordance_create(nullptr);
        ASSERT_NE(aff_ctx, nullptr) << "Failed to create affordance context";

        body_ctx = nimcp_body_create(nullptr);
        ASSERT_NE(body_ctx, nullptr) << "Failed to create body context";
    }

    void TearDown() override {
        if (aff_ctx) { nimcp_affordance_destroy(aff_ctx); aff_ctx = nullptr; }
        if (body_ctx) { nimcp_body_destroy(body_ctx); body_ctx = nullptr; }
    }

    nimcp_object_properties_t makeGraspableObject(uint32_t id, double dist) {
        nimcp_object_properties_t obj;
        memset(&obj, 0, sizeof(obj));
        obj.object_id = id;
        obj.category = NIMCP_OBJECT_CATEGORY_TOOL;
        obj.position[0] = dist;
        obj.position[1] = 0.0;
        obj.position[2] = 0.5;
        obj.distance = dist;
        obj.dimensions[0] = 0.1;
        obj.dimensions[1] = 0.05;
        obj.dimensions[2] = 0.3;
        obj.estimated_mass = 0.2;
        obj.surface_friction = 0.6;
        obj.rigidity = 0.9;
        obj.is_graspable = true;
        obj.is_movable = true;
        obj.has_handle = true;
        obj.is_stationary = true;
        return obj;
    }
};

TEST_F(AffordanceBodyOwnershipIntegration, RegisterObjectAndDetectAffordances) {
    /* Register a tool object */
    nimcp_object_properties_t obj = makeGraspableObject(1, 0.5);
    nimcp_affordance_error_t err = nimcp_affordance_register_object(aff_ctx, &obj);
    EXPECT_EQ(err, NIMCP_AFFORDANCE_OK);

    /* Detect affordances */
    nimcp_affordance_t affordances[16];
    uint32_t num_detected = 0;
    err = nimcp_affordance_detect(aff_ctx, 1, affordances, 16, &num_detected);
    EXPECT_EQ(err, NIMCP_AFFORDANCE_OK);
    EXPECT_GT(num_detected, 0u);
}

TEST_F(AffordanceBodyOwnershipIntegration, BodySchemaInitAndOwnership) {
    /* Initialize human body schema */
    nimcp_body_error_t err = nimcp_body_init_human_schema(body_ctx);
    EXPECT_EQ(err, NIMCP_BODY_OK);

    /* Check we have body parts */
    nimcp_body_part_t parts[32];
    uint32_t num_parts = 0;
    err = nimcp_body_get_all_parts(body_ctx, parts, 32, &num_parts);
    EXPECT_EQ(err, NIMCP_BODY_OK);
    EXPECT_GT(num_parts, 0u);
}

TEST_F(AffordanceBodyOwnershipIntegration, AffordanceCompetitionWithMultipleObjects) {
    /* Register multiple objects */
    nimcp_object_properties_t near = makeGraspableObject(1, 0.3);
    nimcp_object_properties_t far = makeGraspableObject(2, 1.5);

    nimcp_affordance_register_object(aff_ctx, &near);
    nimcp_affordance_register_object(aff_ctx, &far);

    /* Detect all affordances */
    nimcp_affordance_t affordances[32];
    uint32_t num_detected = 0;
    nimcp_affordance_error_t err = nimcp_affordance_detect_all(
        aff_ctx, affordances, 32, &num_detected);
    EXPECT_EQ(err, NIMCP_AFFORDANCE_OK);

    /* Run competition if we detected affordances */
    if (num_detected > 0) {
        nimcp_competition_result_t result;
        memset(&result, 0, sizeof(result));
        err = nimcp_affordance_compete(aff_ctx, &result);
        EXPECT_EQ(err, NIMCP_AFFORDANCE_OK);
    }
}

TEST_F(AffordanceBodyOwnershipIntegration, BodyBoundaryCheck) {
    nimcp_body_init_human_schema(body_ctx);

    /* Check boundary at body center */
    nimcp_body_position_t center_pos = {0.0, 0.0, 0.5};
    bool is_inside = false;
    double dist = 0.0;
    nimcp_body_error_t err = nimcp_body_check_boundary(body_ctx, &center_pos, &is_inside, &dist);
    EXPECT_EQ(err, NIMCP_BODY_OK);
}

TEST_F(AffordanceBodyOwnershipIntegration, BodyCenterOfMass) {
    nimcp_body_init_human_schema(body_ctx);

    nimcp_body_position_t com;
    nimcp_body_error_t err = nimcp_body_get_center_of_mass(body_ctx, &com);
    EXPECT_EQ(err, NIMCP_BODY_OK);
}

/* ============================================================================
 * Simulation + Interoception Integration
 * ============================================================================ */

class SimulationInteroceptionIntegration : public ::testing::Test {
protected:
    nimcp_sim_context_t* sim_ctx = nullptr;
    nimcp_intero_context_t* intero_ctx = nullptr;

    void SetUp() override {
        sim_ctx = nimcp_sim_create(nullptr);
        ASSERT_NE(sim_ctx, nullptr) << "Failed to create simulation context";

        intero_ctx = nimcp_intero_create(nullptr);
        ASSERT_NE(intero_ctx, nullptr) << "Failed to create interoceptive context";
    }

    void TearDown() override {
        if (sim_ctx) { nimcp_sim_destroy(sim_ctx); sim_ctx = nullptr; }
        if (intero_ctx) { nimcp_intero_destroy(intero_ctx); intero_ctx = nullptr; }
    }
};

TEST_F(SimulationInteroceptionIntegration, SimulationLifecycleAndRun) {
    /* Set up an effector */
    nimcp_effector_state_t effector;
    memset(&effector, 0, sizeof(effector));
    effector.effector_id = 0;
    effector.type = NIMCP_EFFECTOR_RIGHT_HAND;
    effector.position.x = 0.0;
    effector.position.y = 0.5;
    effector.position.z = 0.5;
    effector.max_force = 50.0;
    nimcp_sim_error_t err = nimcp_sim_set_effector(sim_ctx, &effector);
    EXPECT_EQ(err, NIMCP_SIM_OK);

    /* Start a simulation */
    uint32_t sim_id = 0;
    err = nimcp_sim_start(sim_ctx, &sim_id);
    EXPECT_EQ(err, NIMCP_SIM_OK);

    /* Add an action step: reach to position */
    nimcp_action_step_t step;
    memset(&step, 0, sizeof(step));
    step.step_id = 0;
    step.primitive = NIMCP_ACTION_PRIM_REACH;
    step.effector_id = 0;
    step.target_position.x = 0.5;
    step.target_position.y = 0.5;
    step.target_position.z = 0.5;
    step.duration = 1.0;
    step.max_velocity = 1.0;
    step.max_force = 20.0;
    step.precision = 0.01;
    err = nimcp_sim_add_step(sim_ctx, sim_id, &step);
    EXPECT_EQ(err, NIMCP_SIM_OK);

    /* Run the simulation */
    nimcp_sim_result_t result;
    memset(&result, 0, sizeof(result));
    err = nimcp_sim_run(sim_ctx, sim_id, &result);
    EXPECT_EQ(err, NIMCP_SIM_OK);
    EXPECT_GE(result.steps_completed, 1u);
}

TEST_F(SimulationInteroceptionIntegration, InteroceptiveSystemRegistrationAndSignal) {
    /* Register cardiovascular system */
    uint32_t sys_id = 0;
    nimcp_intero_error_t err = nimcp_intero_register_system(
        intero_ctx, NIMCP_SYSTEM_CARDIOVASCULAR, &sys_id);
    EXPECT_EQ(err, NIMCP_INTERO_OK);

    /* Process a heart rate signal */
    nimcp_intero_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.signal_id = 0;
    signal.type = NIMCP_SIGNAL_HEART_RATE;
    signal.system = NIMCP_SYSTEM_CARDIOVASCULAR;
    signal.value = 72.0;
    signal.precision = 0.9;
    signal.is_valid = true;
    signal.timestamp = 1000;

    err = nimcp_intero_process_signal(intero_ctx, &signal);
    EXPECT_EQ(err, NIMCP_INTERO_OK);

    /* Get the signal value back */
    double value = 0.0;
    err = nimcp_intero_get_signal(intero_ctx, NIMCP_SIGNAL_HEART_RATE, &value);
    EXPECT_EQ(err, NIMCP_INTERO_OK);
    EXPECT_NEAR(value, 72.0, 1.0);
}

TEST_F(SimulationInteroceptionIntegration, InteroceptiveStressAndRecovery) {
    /* Initialize standard body systems */
    nimcp_intero_error_t err = nimcp_intero_init_standard_systems(intero_ctx);
    EXPECT_EQ(err, NIMCP_INTERO_OK);

    /* Apply stress */
    err = nimcp_intero_apply_stress(intero_ctx, 0.8, 5.0);
    EXPECT_EQ(err, NIMCP_INTERO_OK);

    /* Check allostatic load increased */
    nimcp_allostatic_load_t load;
    memset(&load, 0, sizeof(load));
    err = nimcp_intero_get_allostatic_load(intero_ctx, &load);
    EXPECT_EQ(err, NIMCP_INTERO_OK);
    double load_after_stress = load.total_load;

    /* Apply recovery */
    err = nimcp_intero_apply_recovery(intero_ctx, 10.0);
    EXPECT_EQ(err, NIMCP_INTERO_OK);

    /* Check load decreased (or at least didn't increase) */
    err = nimcp_intero_get_allostatic_load(intero_ctx, &load);
    EXPECT_EQ(err, NIMCP_INTERO_OK);
    EXPECT_LE(load.total_load, load_after_stress + 0.01);
}

TEST_F(SimulationInteroceptionIntegration, SimulationEffortEstimation) {
    /* Set up effector */
    nimcp_effector_state_t effector;
    memset(&effector, 0, sizeof(effector));
    effector.effector_id = 0;
    effector.type = NIMCP_EFFECTOR_RIGHT_HAND;
    effector.position.x = 0.0;
    effector.position.y = 0.5;
    effector.position.z = 0.5;
    effector.max_force = 50.0;
    nimcp_sim_set_effector(sim_ctx, &effector);

    /* Estimate effort for a reach action */
    nimcp_action_step_t step;
    memset(&step, 0, sizeof(step));
    step.primitive = NIMCP_ACTION_PRIM_REACH;
    step.effector_id = 0;
    step.target_position.x = 0.5;
    step.target_position.y = 0.5;
    step.target_position.z = 0.5;
    step.duration = 1.0;
    step.max_velocity = 1.0;

    nimcp_effort_estimate_t estimate;
    memset(&estimate, 0, sizeof(estimate));
    nimcp_sim_error_t err = nimcp_sim_estimate_effort(sim_ctx, &step, &estimate);
    EXPECT_EQ(err, NIMCP_SIM_OK);
    EXPECT_GE(estimate.metabolic_cost, 0.0);
    EXPECT_GE(estimate.time_cost, 0.0);
}

TEST_F(SimulationInteroceptionIntegration, FeasibilityCheck) {
    /* Set up effector */
    nimcp_effector_state_t effector;
    memset(&effector, 0, sizeof(effector));
    effector.effector_id = 0;
    effector.type = NIMCP_EFFECTOR_RIGHT_HAND;
    effector.position.x = 0.0;
    effector.position.y = 0.5;
    effector.position.z = 0.5;
    effector.max_force = 50.0;
    nimcp_sim_set_effector(sim_ctx, &effector);

    /* Check feasibility of a reach */
    nimcp_action_step_t step;
    memset(&step, 0, sizeof(step));
    step.primitive = NIMCP_ACTION_PRIM_REACH;
    step.effector_id = 0;
    step.target_position.x = 0.3;
    step.target_position.y = 0.5;
    step.target_position.z = 0.5;
    step.duration = 1.0;

    bool is_feasible = false;
    char reason[NIMCP_SIMULATION_REASON_MAX];
    memset(reason, 0, sizeof(reason));
    nimcp_sim_error_t err = nimcp_sim_check_feasibility(sim_ctx, &step, &is_feasible, reason);
    EXPECT_EQ(err, NIMCP_SIM_OK);
}

/* ============================================================================
 * Full Embodiment Pipeline Integration
 * ============================================================================ */

class FullEmbodimentIntegration : public ::testing::Test {
protected:
    nimcp_affordance_context_t* aff_ctx = nullptr;
    nimcp_body_context_t* body_ctx = nullptr;
    nimcp_sim_context_t* sim_ctx = nullptr;
    nimcp_intero_context_t* intero_ctx = nullptr;

    void SetUp() override {
        aff_ctx = nimcp_affordance_create(nullptr);
        body_ctx = nimcp_body_create(nullptr);
        sim_ctx = nimcp_sim_create(nullptr);
        intero_ctx = nimcp_intero_create(nullptr);

        ASSERT_NE(aff_ctx, nullptr);
        ASSERT_NE(body_ctx, nullptr);
        ASSERT_NE(sim_ctx, nullptr);
        ASSERT_NE(intero_ctx, nullptr);
    }

    void TearDown() override {
        if (aff_ctx) nimcp_affordance_destroy(aff_ctx);
        if (body_ctx) nimcp_body_destroy(body_ctx);
        if (sim_ctx) nimcp_sim_destroy(sim_ctx);
        if (intero_ctx) nimcp_intero_destroy(intero_ctx);
        aff_ctx = nullptr;
        body_ctx = nullptr;
        sim_ctx = nullptr;
        intero_ctx = nullptr;
    }
};

TEST_F(FullEmbodimentIntegration, AllModulesCreateAndDestroy) {
    /* If we got here, all modules were created successfully */
    SUCCEED();
}

TEST_F(FullEmbodimentIntegration, AllModulesUpdate) {
    /* Update each module with a time step */
    nimcp_affordance_error_t aff_err = nimcp_affordance_update(aff_ctx, 0.016);
    EXPECT_EQ(aff_err, NIMCP_AFFORDANCE_OK);

    nimcp_body_error_t body_err = nimcp_body_update(body_ctx, 0.016);
    EXPECT_EQ(body_err, NIMCP_BODY_OK);

    nimcp_intero_error_t intero_err = nimcp_intero_update(intero_ctx, 0.016);
    EXPECT_EQ(intero_err, NIMCP_INTERO_OK);
}

TEST_F(FullEmbodimentIntegration, AllModulesGetStats) {
    nimcp_affordance_stats_t aff_stats;
    memset(&aff_stats, 0, sizeof(aff_stats));
    nimcp_affordance_error_t err = nimcp_affordance_get_stats(aff_ctx, &aff_stats);
    EXPECT_EQ(err, NIMCP_AFFORDANCE_OK);

    nimcp_body_stats_t body_stats;
    memset(&body_stats, 0, sizeof(body_stats));
    nimcp_body_error_t berr = nimcp_body_get_stats(body_ctx, &body_stats);
    EXPECT_EQ(berr, NIMCP_BODY_OK);

    nimcp_sim_stats_t sim_stats;
    memset(&sim_stats, 0, sizeof(sim_stats));
    nimcp_sim_error_t serr = nimcp_sim_get_stats(sim_ctx, &sim_stats);
    EXPECT_EQ(serr, NIMCP_SIM_OK);

    nimcp_intero_stats_t intero_stats;
    memset(&intero_stats, 0, sizeof(intero_stats));
    nimcp_intero_error_t ierr = nimcp_intero_get_stats(intero_ctx, &intero_stats);
    EXPECT_EQ(ierr, NIMCP_INTERO_OK);
}

TEST_F(FullEmbodimentIntegration, AllModulesReset) {
    nimcp_affordance_error_t aff_err = nimcp_affordance_reset(aff_ctx);
    EXPECT_EQ(aff_err, NIMCP_AFFORDANCE_OK);

    nimcp_body_error_t body_err = nimcp_body_reset(body_ctx);
    EXPECT_EQ(body_err, NIMCP_BODY_OK);

    nimcp_sim_error_t sim_err = nimcp_sim_reset(sim_ctx);
    EXPECT_EQ(sim_err, NIMCP_SIM_OK);

    nimcp_intero_error_t intero_err = nimcp_intero_reset(intero_ctx);
    EXPECT_EQ(intero_err, NIMCP_INTERO_OK);
}

TEST_F(FullEmbodimentIntegration, InteroceptionEmotionalState) {
    /* Initialize standard systems */
    nimcp_intero_init_standard_systems(intero_ctx);

    /* Get emotional state */
    nimcp_emotional_state_t emo;
    memset(&emo, 0, sizeof(emo));
    nimcp_intero_error_t err = nimcp_intero_get_emotional_state(intero_ctx, &emo);
    EXPECT_EQ(err, NIMCP_INTERO_OK);
    EXPECT_GE(emo.arousal_value, 0.0);
    EXPECT_LE(emo.arousal_value, 1.0);
}

TEST_F(FullEmbodimentIntegration, BodyProprioceptionUpdate) {
    nimcp_body_init_human_schema(body_ctx);

    /* Get all parts to find a valid part_id */
    nimcp_body_part_t parts[32];
    uint32_t num_parts = 0;
    nimcp_body_get_all_parts(body_ctx, parts, 32, &num_parts);
    ASSERT_GT(num_parts, 0u);

    /* Send proprioceptive signal for first part */
    nimcp_proprio_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.part_id = parts[0].part_id;
    signal.position[0] = 0.1;
    signal.position[1] = 0.2;
    signal.position[2] = 0.3;
    signal.confidence = 0.9;
    signal.timestamp = 1000;

    nimcp_body_error_t err = nimcp_body_process_proprio(body_ctx, &signal);
    EXPECT_EQ(err, NIMCP_BODY_OK);
}

TEST_F(FullEmbodimentIntegration, NameStringFunctions) {
    /* Verify name functions return valid strings */
    const char* action_name = nimcp_affordance_action_name(NIMCP_ACTION_GRASP);
    ASSERT_NE(action_name, nullptr);
    EXPECT_GT(strlen(action_name), 0u);

    const char* state_name = nimcp_affordance_state_name(NIMCP_AFFORDANCE_STATE_DETECTED);
    ASSERT_NE(state_name, nullptr);
    EXPECT_GT(strlen(state_name), 0u);

    const char* cat_name = nimcp_affordance_category_name(NIMCP_OBJECT_CATEGORY_TOOL);
    ASSERT_NE(cat_name, nullptr);
    EXPECT_GT(strlen(cat_name), 0u);

    const char* part_name = nimcp_body_part_type_name(NIMCP_BODY_PART_RIGHT_HAND);
    ASSERT_NE(part_name, nullptr);
    EXPECT_GT(strlen(part_name), 0u);

    const char* own_name = nimcp_body_ownership_state_name(NIMCP_OWNERSHIP_FULL);
    ASSERT_NE(own_name, nullptr);
    EXPECT_GT(strlen(own_name), 0u);

    const char* sim_state = nimcp_sim_state_name(NIMCP_SIM_STATE_RUNNING);
    ASSERT_NE(sim_state, nullptr);
    EXPECT_GT(strlen(sim_state), 0u);

    const char* prim_name = nimcp_sim_primitive_name(NIMCP_ACTION_PRIM_REACH);
    ASSERT_NE(prim_name, nullptr);
    EXPECT_GT(strlen(prim_name), 0u);

    const char* sys_name = nimcp_intero_system_name(NIMCP_SYSTEM_CARDIOVASCULAR);
    ASSERT_NE(sys_name, nullptr);
    EXPECT_GT(strlen(sys_name), 0u);

    const char* sig_name = nimcp_intero_signal_name(NIMCP_SIGNAL_HEART_RATE);
    ASSERT_NE(sig_name, nullptr);
    EXPECT_GT(strlen(sig_name), 0u);
}
