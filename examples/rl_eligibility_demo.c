/**
 * @file rl_eligibility_demo.c
 * @brief Reinforcement Learning with Eligibility Traces Demo
 *
 * WHAT: Demonstrates reward-based learning with eligibility traces
 * WHY:  Show how to use brain_apply_reward_learning() for RL tasks
 * HOW:  Simple cart-pole-like task with delayed rewards
 *
 * KEY FEATURES:
 * - Eligibility traces for temporal credit assignment
 * - Delayed reward propagation to earlier actions
 * - Three-factor learning rule (Hebbian + Reward + Dopamine)
 *
 * USAGE:
 *   ./rl_eligibility_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "core/brain/nimcp_brain.h"

//=============================================================================
// Simple Cart-Pole Environment (Simplified)
//=============================================================================

typedef struct {
    float position;    // Cart position [-2.4, 2.4]
    float velocity;    // Cart velocity
    float angle;       // Pole angle [-0.42, 0.42] radians (~24°)
    float angular_vel; // Angular velocity
} cart_pole_state_t;

// Initialize cart-pole
void cartpole_reset(cart_pole_state_t* state) {
    state->position = 0.0f;
    state->velocity = 0.0f;
    state->angle = 0.05f;  // Small initial disturbance
    state->angular_vel = 0.0f;
}

// Step physics (simplified)
bool cartpole_step(cart_pole_state_t* state, int action, float* reward) {
    const float gravity = 9.8f;
    const float masscart = 1.0f;
    const float masspole = 0.1f;
    const float length = 0.5f;
    const float force_mag = 10.0f;
    const float tau = 0.02f;  // Timestep

    float force = (action == 1) ? force_mag : -force_mag;

    float costheta = cosf(state->angle);
    float sintheta = sinf(state->angle);

    // Simplified physics
    float temp = (force + masspole * length * state->angular_vel * state->angular_vel * sintheta) / (masscart + masspole);
    float thetaacc = (gravity * sintheta - costheta * temp) / (length * (4.0f / 3.0f - masspole * costheta * costheta / (masscart + masspole)));
    float xacc = temp - masspole * length * thetaacc * costheta / (masscart + masspole);

    // Update state
    state->position += tau * state->velocity;
    state->velocity += tau * xacc;
    state->angle += tau * state->angular_vel;
    state->angular_vel += tau * thetaacc;

    // Check termination
    bool done = (fabsf(state->position) > 2.4f || fabsf(state->angle) > 0.21f);

    // Reward: +1 for staying upright
    *reward = done ? 0.0f : 1.0f;

    return done;
}

// Convert state to features
void cartpole_features(cart_pole_state_t* state, float* features) {
    features[0] = state->position / 2.4f;      // Normalize to [-1, 1]
    features[1] = state->velocity / 3.0f;      // Approximate normalization
    features[2] = state->angle / 0.21f;        // Normalize to [-1, 1]
    features[3] = state->angular_vel / 2.0f;   // Approximate normalization
}

//=============================================================================
// Main Demo
//=============================================================================

int main(int argc, char** argv) {
    printf("=================================================================\n");
    printf("Reinforcement Learning with Eligibility Traces Demo\n");
    printf("=================================================================\n\n");

    // Create brain for cart-pole task
    printf("Creating brain (4 inputs, 2 outputs: left/right action)...\n");

    brain_t brain = brain_create("cartpole_rl", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 4, 2);
    if (!brain) {
        fprintf(stderr, "ERROR: Failed to create brain\n");
        return 1;
    }
    printf("✓ Brain created\n\n");

    // Training loop
    const int num_episodes = 50;
    const int max_steps = 200;

    printf("Training with eligibility traces...\n");
    printf("Episode | Steps | Avg Reward | Synapses Modified\n");
    printf("--------|-------|------------|------------------\n");

    for (int episode = 0; episode < num_episodes; episode++) {
        cart_pole_state_t state;
        cartpole_reset(&state);

        float features[4];
        float episode_reward = 0.0f;
        int steps = 0;
        int total_synapses_modified = 0;

        for (int step = 0; step < max_steps; step++) {
            // Get current state features
            cartpole_features(&state, features);

            // Brain decides action
            brain_decision_t* decision = brain_decide(brain, features, 4);
            int action = (decision && strcmp(decision->label, "left") == 0) ? 0 : 1;

            // Take action in environment
            float reward;
            bool done = cartpole_step(&state, action, &reward);
            episode_reward += reward;
            steps++;

            // Apply reward-based learning with eligibility traces
            // This propagates reward to recently active synapses
            uint32_t num_modified = brain_apply_reward_learning(brain, reward);
            total_synapses_modified += num_modified;

            if (done) {
                break;
            }
        }

        float avg_reward = episode_reward / steps;

        // Print progress every 5 episodes
        if ((episode + 1) % 5 == 0 || episode == 0) {
            printf("%7d | %5d | %10.3f | %16d\n",
                   episode + 1, steps, avg_reward, total_synapses_modified);
        }
    }

    printf("\n✓ Training complete!\n\n");

    // Test the trained brain
    printf("Testing trained brain...\n");
    cart_pole_state_t test_state;
    cartpole_reset(&test_state);

    float test_features[4];
    int test_steps = 0;
    float test_reward_sum = 0.0f;

    printf("Step | Position | Angle | Action | Reward\n");
    printf("-----|----------|-------|--------|-------\n");

    for (int step = 0; step < max_steps; step++) {
        cartpole_features(&test_state, test_features);

        brain_decision_t* decision = brain_decide(brain, test_features, 4);
        int action = (decision && strcmp(decision->label, "left") == 0) ? 0 : 1;

        float reward;
        bool done = cartpole_step(&test_state, action, &reward);
        test_reward_sum += reward;
        test_steps++;

        if (step % 20 == 0) {
            printf("%4d | %8.3f | %5.3f | %6s | %.1f\n",
                   step, test_state.position, test_state.angle,
                   action == 0 ? "left" : "right", reward);
        }

        if (done) {
            break;
        }
    }

    printf("\nTest Results:\n");
    printf("  Total steps: %d\n", test_steps);
    printf("  Average reward: %.3f\n", test_reward_sum / test_steps);
    printf("  Success: %s\n", test_steps >= 100 ? "YES (stayed upright 100+ steps)" : "PARTIAL");

    // Cleanup
    brain_destroy(brain);
    printf("\n✓ Demo complete!\n");

    printf("\n=================================================================\n");
    printf("Key Concepts Demonstrated:\n");
    printf("=================================================================\n");
    printf("1. Eligibility Traces: Recent synaptic activity marked as 'eligible'\n");
    printf("2. Delayed Rewards: Rewards propagate to earlier actions via traces\n");
    printf("3. Three-Factor Rule: Hebbian activity + Reward + Dopamine\n");
    printf("4. Temporal Credit Assignment: Which action led to the reward?\n");
    printf("5. Reinforcement Learning: Learn from trial and error\n");
    printf("=================================================================\n");

    return 0;
}
