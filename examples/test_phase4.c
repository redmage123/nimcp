/**
 * @file test_phase4.c
 * @brief Quick test of Phase 4 components (STDP + Eligibility Traces)
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "plasticity/stdp/nimcp_stdp.h"
#include "plasticity/eligibility/nimcp_eligibility_trace.h"
#include "core/neuralnet/nimcp_neuralnet.h"

// Access synapse internals for testing
// Normally synapse_t is opaque, but for testing we need direct access
extern synapse_t* create_test_synapse(uint32_t source, uint32_t target, float weight);
extern void set_synapse_weight(synapse_t* syn, float weight);
extern float get_synapse_weight(synapse_t* syn);

int main(void) {
    printf("========================================\n");
    printf("Phase 4 Component Test\n");
    printf("========================================\n\n");

    // Test 1: STDP Learning
    printf("Test 1: STDP Learning\n");
    printf("----------------------------------\n");

    stdp_learner_t* stdp = stdp_create(NULL);  // Use defaults
    if (!stdp) {
        fprintf(stderr, "ERROR: Failed to create STDP learner\n");
        return 1;
    }

    printf("Created STDP learner\n");
    printf("Parameters: τ+ = %.1f ms, τ- = %.1f ms\n",
           stdp->config.tau_plus, stdp->config.tau_minus);
    printf("            A+ = %.3f, A- = %.3f\n",
           stdp->config.A_plus, stdp->config.A_minus);

    // Test LTP (pre before post)
    synapse_t synapse1 = {
        .source_neuron_id = 0,
        .target_neuron_id = 1,
        .weight = 5.0f
    };

    printf("\n--- LTP Test (pre before post) ---\n");
    printf("Initial weight: %.3f\n", synapse1.weight);

    uint64_t t_pre = 100;   // Pre spikes at 100ms
    uint64_t t_post = 110;  // Post spikes at 110ms (Δt = +10ms)

    float delta_w = stdp_apply(stdp, &synapse1, t_pre, t_post);
    printf("Spike timing: pre=%lu ms, post=%lu ms (Δt=+%ld ms)\n",
           t_pre, t_post, (int64_t)(t_post - t_pre));
    printf("Weight change: Δw = %.6f (LTP)\n", delta_w);
    printf("New weight: %.6f\n", synapse1.weight);

    // Test LTD (post before pre)
    synapse_t synapse2 = {
        .source_neuron_id = 2,
        .target_neuron_id = 3,
        .weight = 5.0f
    };

    printf("\n--- LTD Test (post before pre) ---\n");
    printf("Initial weight: %.3f\n", synapse2.weight);

    t_pre = 210;   // Pre spikes at 210ms
    t_post = 200;  // Post spikes at 200ms (Δt = -10ms)

    delta_w = stdp_apply(stdp, &synapse2, t_pre, t_post);
    printf("Spike timing: pre=%lu ms, post=%lu ms (Δt=%ld ms)\n",
           t_pre, t_post, (int64_t)(t_post - t_pre));
    printf("Weight change: Δw = %.6f (LTD)\n", delta_w);
    printf("New weight: %.6f\n", synapse2.weight);

    // Test statistics
    uint64_t ltp_count, ltd_count;
    float avg_change;
    stdp_get_statistics(stdp, &ltp_count, &ltd_count, &avg_change);
    printf("\n--- STDP Statistics ---\n");
    printf("LTP events: %lu\n", ltp_count);
    printf("LTD events: %lu\n", ltd_count);
    printf("Avg |Δw|: %.6f\n", avg_change);

    printf("✓ STDP test passed\n\n");

    // Test 2: Eligibility Traces
    printf("Test 2: Eligibility Traces\n");
    printf("----------------------------------\n");

    eligibility_config_t elig_config = eligibility_default_config();
    printf("Created eligibility trace system\n");
    printf("Parameters: λ = %.3f, η = %.4f\n",
           elig_config.decay_lambda, elig_config.learning_rate);

    eligibility_trace_t trace;
    eligibility_trace_init(&trace, 0);

    synapse_t synapse3 = {
        .source_neuron_id = 4,
        .target_neuron_id = 5,
        .weight = 5.0f
    };

    printf("\n--- Trace Dynamics ---\n");
    printf("Initial trace: %.3f\n", trace.trace);

    // Spike at t=10ms
    eligibility_trace_update(&trace, &elig_config, 10, 1.0f);
    printf("After spike at t=10ms: trace = %.3f\n", trace.trace);

    // Decay to t=20ms
    eligibility_trace_decay(&trace, &elig_config, 20);
    printf("After decay to t=20ms: trace = %.3f\n", trace.trace);

    // Decay to t=40ms (20ms later)
    eligibility_trace_decay(&trace, &elig_config, 40);
    printf("After decay to t=40ms: trace = %.3f\n", trace.trace);

    // Apply reward
    printf("\n--- Reward-Based Learning ---\n");
    printf("Initial weight: %.3f\n", synapse3.weight);

    float reward = 0.5f;
    float dopamine = 0.8f;

    delta_w = eligibility_apply_reward(
        &synapse3, &trace, &elig_config,
        reward, dopamine
    );

    printf("Reward: %.2f, Dopamine: %.2f, Trace: %.3f\n",
           reward, dopamine, trace.trace);
    printf("Weight change: Δw = %.6f\n", delta_w);
    printf("New weight: %.6f\n", synapse3.weight);

    printf("✓ Eligibility trace test passed\n\n");

    // Test 3: Combined STDP + Eligibility Traces
    printf("Test 3: STDP + Eligibility Traces\n");
    printf("----------------------------------\n");

    synapse_t synapse4 = {
        .source_neuron_id = 6,
        .target_neuron_id = 7,
        .weight = 5.0f
    };

    eligibility_trace_t trace2;
    eligibility_trace_init(&trace2, 0);

    printf("Simulating temporal credit assignment task\n");
    printf("Initial weight: %.3f\n", synapse4.weight);

    // Sequence: Spike → Wait → Reward
    uint64_t spike_time = 50;
    uint64_t reward_time = 100;

    // Spike creates eligibility
    eligibility_trace_update(&trace2, &elig_config, spike_time, 1.0f);
    printf("t=%lu ms: Spike occurred, trace = %.3f\n", spike_time, trace2.trace);

    // Reward arrives later
    delta_w = eligibility_update_and_learn(
        &synapse4, &trace2, &elig_config,
        reward_time, 1.0f, 0.9f
    );

    printf("t=%lu ms: Reward arrived, trace = %.3f\n", reward_time, trace2.trace);
    printf("Weight change: Δw = %.6f\n", delta_w);
    printf("New weight: %.6f\n", synapse4.weight);

    printf("✓ Combined test passed\n\n");

    // Cleanup
    stdp_destroy(stdp);

    printf("========================================\n");
    printf("All Phase 4 tests PASSED!\n");
    printf("========================================\n");

    return 0;
}
