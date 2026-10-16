/**
 * @file nimcp_social_interaction.c
 * @brief Multi-agent social interaction framework — single brain, multiple personas
 *
 * WHAT: Simulates 2-8 agent personas sharing one neural network with different
 *       perspective masks, independent belief states, and ToM models.
 * WHY:  Enables social cognition training (cooperation, teaching, debate,
 *       deception detection, joint attention) without multiple brain instances.
 * HOW:  Each agent sees a masked subset of the environment, runs brain inference
 *       on its view, communicates via message embeddings, updates ToM models
 *       of other agents, and adjusts trust based on prediction accuracy.
 *       Biologically analogous to hemispheric/regional perspective differences.
 */

#include "cognitive/social/nimcp_social_interaction.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "cognitive/kg/nimcp_wave10_affective_kg.h"  /* W10: social episode events */
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define LOG_MODULE "SOCIAL_INTERACTION"

/* Brain API — weak stubs so this module links independently */
__attribute__((weak))
int nimcp_brain_infer(void* brain, const float* features,
                       uint32_t num_features, float* outputs,
                       uint32_t num_outputs) {
    (void)brain; (void)features; (void)num_features;
    (void)outputs; (void)num_outputs;
    return -1;
}
__attribute__((weak))
int nimcp_brain_learn(void* brain, const float* features,
                       uint32_t n_feat, const float* target,
                       uint32_t n_tgt, const char* label,
                       float confidence, float lr) {
    (void)brain; (void)features; (void)n_feat; (void)target; (void)n_tgt;
    (void)label; (void)confidence; (void)lr;
    return -1;
}

/* Working memory scratchpad — weak stubs */
__attribute__((weak))
int nimcp_wms_write(void* wms, uint32_t slot_idx,
                    const float* data, uint32_t data_dim, const char* label) {
    (void)wms; (void)slot_idx; (void)data; (void)data_dim; (void)label;
    return -1;
}
__attribute__((weak))
int nimcp_wms_read(void* wms, uint32_t slot_idx,
                   float* data_out, uint32_t max_dim) {
    (void)wms; (void)slot_idx; (void)data_out; (void)max_dim;
    return -1;
}

/* ============================================================================
 * Internal struct
 * ============================================================================ */

#define SOCIAL_BRAIN_OUT_DIM 4096
#define SOCIAL_EMBED_DIM     256
#define SOCIAL_INBOX_CAP     64

struct nimcp_social_interaction {
    void* brain;
    nimcp_social_config_t config;
    nimcp_social_agent_t agents[NIMCP_SOCIAL_MAX_AGENTS];
    uint32_t num_agents;

    /* Shared environment */
    float* environment;
    uint32_t env_dim;

    /* Communication buffer (all messages this round) */
    nimcp_social_message_t* message_buffer;
    uint32_t msg_count;
    uint32_t msg_capacity;

    /* Episode tracking */
    uint32_t total_episodes;
    uint32_t total_rounds;

    /* PRNG */
    uint32_t rng_state;
};

/* ============================================================================
 * Utility functions
 * ============================================================================ */

static uint32_t social_xorshift(uint32_t* state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static float social_randf(uint32_t* rng) {
    return (social_xorshift(rng) & 0x7FFFFFFF) / (float)0x7FFFFFFF;
}

static float social_gaussian(uint32_t* rng) {
    float u1 = social_randf(rng);
    float u2 = social_randf(rng);
    if (u1 < 1e-10f) u1 = 1e-10f;
    return sqrtf(-2.0f * logf(u1)) * cosf(2.0f * 3.14159265f * u2);
}

static float social_cosine_similarity(const float* a, const float* b, uint32_t dim) {
    float dot = 0.0f, na = 0.0f, nb = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        na += a[i] * a[i];
        nb += b[i] * b[i];
    }
    float denom = sqrtf(na) * sqrtf(nb);
    if (denom < 1e-10f) return 0.0f;
    return dot / denom;
}

static float social_cosine_distance(const float* a, const float* b, uint32_t dim) {
    float sim = social_cosine_similarity(a, b, dim);
    return 1.0f - sim;
}

/* ============================================================================
 * Perspective mask generation
 * ============================================================================ */

/**
 * Generate deterministic perspective mask for an agent.
 *
 * With overlap=0.3 and 3 agents: each sees ~50% of features, with 30% overlap.
 * Mask values: 1.0 (visible), 0.0 (hidden), 0.5 (partially visible).
 * Deterministic based on agent_id for reproducibility.
 */
static void generate_perspective_mask(float* mask, uint32_t dim,
                                      uint32_t agent_id, uint32_t num_agents,
                                      float overlap) {
    if (!mask || dim == 0 || num_agents == 0) return;

    /* Base visibility: fraction of features each agent can fully see */
    float base_visibility = (1.0f + overlap) / (float)num_agents;
    if (base_visibility > 1.0f) base_visibility = 1.0f;

    /* Deterministic PRNG seeded by agent_id */
    uint32_t rng = (agent_id + 1) * 2654435761u;

    /* Phase offset for each agent: spread them evenly across feature space */
    float phase = (float)agent_id / (float)num_agents;

    for (uint32_t i = 0; i < dim; i++) {
        /* Sinusoidal visibility pattern with agent-specific phase */
        float t = (float)i / (float)dim;
        float angle = 2.0f * 3.14159265f * (t - phase) * (float)num_agents;
        float vis = 0.5f + 0.5f * cosf(angle);

        /* Overlap region: features near boundaries get partial visibility */
        if (vis > base_visibility) {
            mask[i] = 1.0f;       /* Fully visible */
        } else if (vis > base_visibility * 0.5f) {
            mask[i] = 0.5f;       /* Partially visible (overlap region) */
        } else {
            mask[i] = 0.0f;       /* Hidden */
        }

        /* Add slight hash-based variation for non-trivial patterns */
        uint32_t h = social_xorshift(&rng);
        if ((h & 0xFF) < (uint32_t)(overlap * 64.0f)) {
            /* Overlap boost: some hidden features become partially visible */
            if (mask[i] < 0.5f) mask[i] = 0.5f;
        }
    }
}

/* ============================================================================
 * Agent initialization / cleanup
 * ============================================================================ */

static int init_agent(nimcp_social_agent_t* agent, uint32_t id,
                      nimcp_social_role_t role, const nimcp_social_config_t* cfg,
                      uint32_t env_dim) {
    if (!agent || !cfg) return -1;

    memset(agent, 0, sizeof(*agent));
    agent->agent_id = id;
    agent->role = role;
    snprintf(agent->name, sizeof(agent->name), "agent_%u", id);

    /* Perspective mask */
    agent->mask_dim = env_dim;
    agent->perspective_mask = nimcp_calloc(env_dim, sizeof(float));
    if (!agent->perspective_mask) return -1;
    generate_perspective_mask(agent->perspective_mask, env_dim,
                              id, cfg->num_agents, cfg->perspective_overlap);

    /* Belief state */
    agent->belief_dim = cfg->belief_dim;
    agent->belief_state = nimcp_calloc(cfg->belief_dim, sizeof(float));
    if (!agent->belief_state) {
        nimcp_free(agent->perspective_mask);
        return -1;
    }

    /* ToM models: what this agent thinks each other agent believes */
    agent->tom_models = nimcp_calloc(
        (size_t)NIMCP_SOCIAL_MAX_AGENTS * cfg->belief_dim, sizeof(float));
    if (!agent->tom_models) {
        nimcp_free(agent->belief_state);
        nimcp_free(agent->perspective_mask);
        return -1;
    }

    /* Inbox */
    agent->inbox_capacity = SOCIAL_INBOX_CAP;
    agent->inbox = nimcp_calloc(SOCIAL_INBOX_CAP, sizeof(nimcp_social_message_t));
    if (!agent->inbox) {
        nimcp_free(agent->tom_models);
        nimcp_free(agent->belief_state);
        nimcp_free(agent->perspective_mask);
        return -1;
    }
    agent->inbox_count = 0;

    /* Trust: start at 0.5 (neutral) */
    for (uint32_t j = 0; j < NIMCP_SOCIAL_MAX_AGENTS; j++) {
        agent->trust_scores[j] = 0.5f;
    }
    agent->trust_scores[id] = 1.0f; /* Trust self fully */

    agent->interaction_count = 0;
    agent->cumulative_reward = 0.0f;
    return 0;
}

static void cleanup_agent(nimcp_social_agent_t* agent) {
    if (!agent) return;
    nimcp_free(agent->perspective_mask);
    nimcp_free(agent->belief_state);
    nimcp_free(agent->tom_models);
    nimcp_free(agent->inbox);
    agent->perspective_mask = NULL;
    agent->belief_state = NULL;
    agent->tom_models = NULL;
    agent->inbox = NULL;
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

nimcp_social_config_t nimcp_social_config_default(void) {
    nimcp_social_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.num_agents = 3;
    cfg.scenario = NIMCP_SCENARIO_COOPERATIVE;
    cfg.max_rounds = 10;
    cfg.belief_dim = 128;
    cfg.perspective_overlap = 0.3f;
    cfg.trust_learning_rate = 0.1f;
    cfg.communication_noise = 0.05f;
    cfg.enable_deception = false;
    return cfg;
}

nimcp_social_interaction_t* nimcp_social_interaction_create(
    void* brain, const nimcp_social_config_t* config)
{
    if (!brain) {
        LOG_ERROR("[%s] create: brain is required", LOG_MODULE);
        return NULL;
    }

    nimcp_social_config_t cfg = config ? *config : nimcp_social_config_default();

    if (cfg.num_agents < 2) cfg.num_agents = 2;
    if (cfg.num_agents > NIMCP_SOCIAL_MAX_AGENTS) cfg.num_agents = NIMCP_SOCIAL_MAX_AGENTS;
    if (cfg.belief_dim == 0) cfg.belief_dim = 128;
    if (cfg.max_rounds == 0) cfg.max_rounds = 10;

    nimcp_social_interaction_t* si = nimcp_calloc(1, sizeof(nimcp_social_interaction_t));
    if (!si) {
        LOG_ERROR("[%s] create: allocation failed", LOG_MODULE);
        return NULL;
    }

    si->brain = brain;
    si->config = cfg;
    si->num_agents = cfg.num_agents;
    si->env_dim = 0; /* Set when episode runs */
    si->environment = NULL;

    /* Message buffer */
    si->msg_capacity = cfg.num_agents * cfg.max_rounds * 2;
    si->message_buffer = nimcp_calloc(si->msg_capacity, sizeof(nimcp_social_message_t));
    if (!si->message_buffer) {
        nimcp_free(si);
        return NULL;
    }
    si->msg_count = 0;

    /* PRNG */
    si->rng_state = (uint32_t)(nimcp_time_now_us() & 0xFFFFFFFF);
    if (si->rng_state == 0) si->rng_state = 42;

    /* Initialize agents with scenario-appropriate roles */
    for (uint32_t i = 0; i < cfg.num_agents; i++) {
        nimcp_social_role_t role;
        switch (cfg.scenario) {
        case NIMCP_SCENARIO_TEACHING:
            role = (i == 0) ? NIMCP_SOCIAL_ROLE_TEACHER : NIMCP_SOCIAL_ROLE_LEARNER;
            break;
        case NIMCP_SCENARIO_DECEPTION:
            if (cfg.enable_deception && i == cfg.num_agents - 1) {
                role = NIMCP_SOCIAL_ROLE_ADVERSARY;
            } else {
                role = NIMCP_SOCIAL_ROLE_COOPERATOR;
            }
            break;
        case NIMCP_SCENARIO_DEBATE:
            role = NIMCP_SOCIAL_ROLE_OBSERVER;
            break;
        case NIMCP_SCENARIO_JOINT_ATTENTION:
            role = NIMCP_SOCIAL_ROLE_OBSERVER;
            break;
        default:
            role = NIMCP_SOCIAL_ROLE_COOPERATOR;
            break;
        }

        /* Use a placeholder env_dim; will be resized on first episode if needed */
        if (init_agent(&si->agents[i], i, role, &cfg, cfg.belief_dim) != 0) {
            LOG_ERROR("[%s] create: failed to init agent %u", LOG_MODULE, i);
            for (uint32_t j = 0; j < i; j++) {
                cleanup_agent(&si->agents[j]);
            }
            nimcp_free(si->message_buffer);
            nimcp_free(si);
            return NULL;
        }
    }

    si->total_episodes = 0;
    si->total_rounds = 0;

    LOG_INFO("[%s] Created (%u agents, scenario=%d, max_rounds=%u, "
             "belief_dim=%u, overlap=%.2f)",
             LOG_MODULE, cfg.num_agents, cfg.scenario, cfg.max_rounds,
             cfg.belief_dim, cfg.perspective_overlap);
    return si;
}

void nimcp_social_interaction_destroy(nimcp_social_interaction_t* si) {
    if (!si) return;

    for (uint32_t i = 0; i < si->num_agents; i++) {
        cleanup_agent(&si->agents[i]);
    }
    nimcp_free(si->message_buffer);
    nimcp_free(si->environment);
    nimcp_free(si);
}

/* ============================================================================
 * Episode execution
 * ============================================================================ */

/**
 * Resize perspective masks if environment dimension changed.
 */
static int ensure_masks(nimcp_social_interaction_t* si, uint32_t state_dim) {
    for (uint32_t i = 0; i < si->num_agents; i++) {
        nimcp_social_agent_t* a = &si->agents[i];
        if (a->mask_dim != state_dim) {
            nimcp_free(a->perspective_mask);
            a->perspective_mask = nimcp_calloc(state_dim, sizeof(float));
            if (!a->perspective_mask) return -1;
            a->mask_dim = state_dim;
            generate_perspective_mask(a->perspective_mask, state_dim,
                                      a->agent_id, si->num_agents,
                                      si->config.perspective_overlap);
        }
    }
    return 0;
}

/**
 * Apply perspective mask: agent_input = environment * mask
 */
static void apply_mask(const float* env, const float* mask,
                       float* out, uint32_t dim) {
    for (uint32_t i = 0; i < dim; i++) {
        out[i] = env[i] * mask[i];
    }
}

/**
 * Compose a message embedding from an agent's brain output.
 * Takes the first SOCIAL_EMBED_DIM values of brain output, adds noise.
 */
static void compose_message(nimcp_social_interaction_t* si,
                            uint32_t sender_id,
                            const float* brain_output, uint32_t out_dim,
                            nimcp_social_message_t* msg) {
    memset(msg, 0, sizeof(*msg));
    msg->sender_id = sender_id;
    msg->receiver_id = 0xFFFFFFFF; /* broadcast */
    msg->timestamp = nimcp_time_now_us();

    uint32_t embed = (out_dim < SOCIAL_EMBED_DIM) ? out_dim : SOCIAL_EMBED_DIM;
    msg->embed_dim = embed;

    for (uint32_t k = 0; k < embed; k++) {
        msg->embedding[k] = brain_output[k];
    }

    /* Add communication noise */
    if (si->config.communication_noise > 0.0f) {
        for (uint32_t k = 0; k < embed; k++) {
            msg->embedding[k] += social_gaussian(&si->rng_state) *
                                  si->config.communication_noise;
        }
    }

    /* Adversary: invert embedding to simulate deception */
    nimcp_social_agent_t* sender = &si->agents[sender_id];
    if (sender->role == NIMCP_SOCIAL_ROLE_ADVERSARY) {
        for (uint32_t k = 0; k < embed; k++) {
            msg->embedding[k] = -msg->embedding[k];
        }
    }

    msg->confidence = 0.8f;
    snprintf(msg->content, NIMCP_SOCIAL_MAX_MSG_LEN, "agent_%u_round_msg", sender_id);
}

/**
 * Deliver a message to a receiving agent's inbox.
 */
static void deliver_message(nimcp_social_agent_t* receiver,
                            const nimcp_social_message_t* msg) {
    if (!receiver || !msg) return;
    if (msg->sender_id == receiver->agent_id) return; /* Don't self-deliver */

    if (receiver->inbox_count < receiver->inbox_capacity) {
        receiver->inbox[receiver->inbox_count++] = *msg;
    }
}

/**
 * Update agent's ToM model of sender based on received message.
 */
static void update_tom(nimcp_social_agent_t* agent,
                       uint32_t sender_id, const float* msg_embedding,
                       uint32_t embed_dim, uint32_t belief_dim) {
    if (!agent || !msg_embedding) return;
    if (sender_id >= NIMCP_SOCIAL_MAX_AGENTS) return;

    uint32_t dim = (embed_dim < belief_dim) ? embed_dim : belief_dim;
    float* tom = &agent->tom_models[sender_id * belief_dim];

    for (uint32_t k = 0; k < dim; k++) {
        tom[k] = 0.7f * tom[k] + 0.3f * msg_embedding[k];
    }
}

/**
 * Update trust based on how well we predicted the sender's message.
 */
static void update_trust(nimcp_social_agent_t* agent, uint32_t sender_id,
                         const float* msg_embedding, uint32_t embed_dim,
                         uint32_t belief_dim, float lr) {
    if (!agent || !msg_embedding) return;
    if (sender_id >= NIMCP_SOCIAL_MAX_AGENTS) return;

    /* Compare our ToM model of sender with their actual message */
    float* our_model = &agent->tom_models[sender_id * belief_dim];
    uint32_t dim = (embed_dim < belief_dim) ? embed_dim : belief_dim;

    float pred_err = social_cosine_distance(our_model, msg_embedding, dim);
    float accuracy = 1.0f - pred_err;

    agent->trust_scores[sender_id] =
        agent->trust_scores[sender_id] * (1.0f - lr) + accuracy * lr;

    /* Clamp trust to [0, 1] */
    if (agent->trust_scores[sender_id] < 0.0f) agent->trust_scores[sender_id] = 0.0f;
    if (agent->trust_scores[sender_id] > 1.0f) agent->trust_scores[sender_id] = 1.0f;
}

/**
 * Integrate received messages into agent's own belief state.
 * Weight by trust in sender.
 */
static void integrate_messages(nimcp_social_agent_t* agent, uint32_t belief_dim) {
    if (!agent || agent->inbox_count == 0) return;

    float total_weight = 1.0f; /* Self weight */
    float weighted_belief[4096]; /* Temp buffer, clamped to belief_dim */
    uint32_t dim = (belief_dim < 4096) ? belief_dim : 4096;

    /* Start with own belief weighted at 1.0 */
    for (uint32_t k = 0; k < dim; k++) {
        weighted_belief[k] = agent->belief_state[k];
    }

    /* Add trusted messages */
    for (uint32_t m = 0; m < agent->inbox_count; m++) {
        nimcp_social_message_t* msg = &agent->inbox[m];
        float trust = agent->trust_scores[msg->sender_id];
        uint32_t msg_dim = (msg->embed_dim < dim) ? msg->embed_dim : dim;

        for (uint32_t k = 0; k < msg_dim; k++) {
            weighted_belief[k] += msg->embedding[k] * trust;
        }
        total_weight += trust;
    }

    /* Normalize */
    if (total_weight > 0.0f) {
        for (uint32_t k = 0; k < dim; k++) {
            agent->belief_state[k] = weighted_belief[k] / total_weight;
        }
    }
}

/**
 * Compute belief convergence: average pairwise cosine similarity.
 */
static float compute_convergence(const nimcp_social_interaction_t* si) {
    if (si->num_agents < 2) return 1.0f;

    float total_sim = 0.0f;
    uint32_t pairs = 0;
    uint32_t dim = si->config.belief_dim;

    for (uint32_t i = 0; i < si->num_agents; i++) {
        for (uint32_t j = i + 1; j < si->num_agents; j++) {
            total_sim += social_cosine_similarity(
                si->agents[i].belief_state,
                si->agents[j].belief_state, dim);
            pairs++;
        }
    }
    return (pairs > 0) ? (total_sim / (float)pairs) : 0.0f;
}

/**
 * Compute ToM accuracy: how well each agent's model of others matches
 * the other agents' actual belief states.
 */
static float compute_tom_accuracy(const nimcp_social_interaction_t* si) {
    float total_acc = 0.0f;
    uint32_t count = 0;
    uint32_t dim = si->config.belief_dim;

    for (uint32_t i = 0; i < si->num_agents; i++) {
        for (uint32_t j = 0; j < si->num_agents; j++) {
            if (i == j) continue;
            const float* model = &si->agents[i].tom_models[j * dim];
            const float* actual = si->agents[j].belief_state;
            total_acc += social_cosine_similarity(model, actual, dim);
            count++;
        }
    }
    return (count > 0) ? (total_acc / (float)count) : 0.0f;
}

/**
 * Compute collective reward based on scenario type.
 */
static float compute_reward(const nimcp_social_interaction_t* si,
                            const float* ground_truth, uint32_t gt_dim) {
    float convergence = compute_convergence(si);
    uint32_t dim = si->config.belief_dim;
    if (gt_dim < dim) dim = gt_dim;

    switch (si->config.scenario) {
    case NIMCP_SCENARIO_COOPERATIVE: {
        /* All agents rewarded when consensus matches ground truth */
        float avg_accuracy = 0.0f;
        for (uint32_t i = 0; i < si->num_agents; i++) {
            avg_accuracy += social_cosine_similarity(
                si->agents[i].belief_state, ground_truth, dim);
        }
        avg_accuracy /= (float)si->num_agents;
        return avg_accuracy * convergence;
    }

    case NIMCP_SCENARIO_TEACHING: {
        /* Reward when learner's belief matches teacher's */
        if (si->num_agents < 2) return 0.0f;
        float teacher_accuracy = social_cosine_similarity(
            si->agents[0].belief_state, ground_truth, dim);
        float learner_avg = 0.0f;
        for (uint32_t i = 1; i < si->num_agents; i++) {
            learner_avg += social_cosine_similarity(
                si->agents[i].belief_state,
                si->agents[0].belief_state, dim);
        }
        learner_avg /= (float)(si->num_agents - 1);
        return teacher_accuracy * learner_avg;
    }

    case NIMCP_SCENARIO_DEBATE: {
        /* Reward for finding truth despite different initial biases */
        float best_accuracy = 0.0f;
        for (uint32_t i = 0; i < si->num_agents; i++) {
            float acc = social_cosine_similarity(
                si->agents[i].belief_state, ground_truth, dim);
            if (acc > best_accuracy) best_accuracy = acc;
        }
        return best_accuracy;
    }

    case NIMCP_SCENARIO_DECEPTION: {
        /* Reward honest agents for detecting and excluding adversary */
        float honest_accuracy = 0.0f;
        uint32_t honest_count = 0;
        for (uint32_t i = 0; i < si->num_agents; i++) {
            if (si->agents[i].role != NIMCP_SOCIAL_ROLE_ADVERSARY) {
                honest_accuracy += social_cosine_similarity(
                    si->agents[i].belief_state, ground_truth, dim);
                honest_count++;
            }
        }
        if (honest_count > 0) honest_accuracy /= (float)honest_count;
        return honest_accuracy;
    }

    case NIMCP_SCENARIO_JOINT_ATTENTION: {
        /* Reward when combined agent beliefs cover all features */
        float coverage = 0.0f;
        for (uint32_t k = 0; k < dim; k++) {
            float best = 0.0f;
            for (uint32_t i = 0; i < si->num_agents; i++) {
                float diff = fabsf(si->agents[i].belief_state[k] - ground_truth[k]);
                float acc = 1.0f - (diff / (fabsf(ground_truth[k]) + 1e-6f));
                if (acc < 0.0f) acc = 0.0f;
                if (acc > best) best = acc;
            }
            coverage += best;
        }
        return coverage / (float)dim;
    }

    default:
        return convergence;
    }
}

int nimcp_social_run_episode(nimcp_social_interaction_t* si,
                             const float* environment_state, uint32_t state_dim,
                             nimcp_social_result_t* result)
{
    if (!si || !environment_state || state_dim == 0) {
        LOG_ERROR("[%s] run_episode: invalid args", LOG_MODULE);
        return -1;
    }

    /* Ensure environment buffer */
    if (si->env_dim != state_dim) {
        nimcp_free(si->environment);
        si->environment = nimcp_calloc(state_dim, sizeof(float));
        if (!si->environment) return -1;
        si->env_dim = state_dim;

        if (ensure_masks(si, state_dim) != 0) return -1;
    }
    memcpy(si->environment, environment_state, state_dim * sizeof(float));

    /* Reset agent inboxes and message buffer */
    si->msg_count = 0;
    for (uint32_t i = 0; i < si->num_agents; i++) {
        si->agents[i].inbox_count = 0;
    }

    /* Working buffers */
    float* masked_input = nimcp_calloc(state_dim, sizeof(float));
    float* brain_output = nimcp_calloc(SOCIAL_BRAIN_OUT_DIM, sizeof(float));
    if (!masked_input || !brain_output) {
        nimcp_free(masked_input);
        nimcp_free(brain_output);
        return -1;
    }

    uint32_t total_messages = 0;
    uint32_t deceptions_detected = 0;
    uint32_t rounds_completed = 0;

    for (uint32_t round = 0; round < si->config.max_rounds; round++) {
        rounds_completed = round + 1;

        /* --- OBSERVE: each agent runs brain inference on its masked input --- */
        for (uint32_t i = 0; i < si->num_agents; i++) {
            nimcp_social_agent_t* a = &si->agents[i];
            uint32_t mask_dim = (a->mask_dim < state_dim) ? a->mask_dim : state_dim;

            apply_mask(si->environment, a->perspective_mask, masked_input, mask_dim);

            memset(brain_output, 0, SOCIAL_BRAIN_OUT_DIM * sizeof(float));
            nimcp_brain_infer(si->brain, masked_input, mask_dim,
                              brain_output, SOCIAL_BRAIN_OUT_DIM);

            /* Update belief from brain output (first belief_dim values) */
            uint32_t bd = si->config.belief_dim;
            uint32_t copy_dim = (SOCIAL_BRAIN_OUT_DIM < bd) ? SOCIAL_BRAIN_OUT_DIM : bd;
            for (uint32_t k = 0; k < copy_dim; k++) {
                a->belief_state[k] = 0.6f * a->belief_state[k] + 0.4f * brain_output[k];
            }

            /* --- COMMUNICATE: compose message from belief --- */
            nimcp_social_message_t msg;
            compose_message(si, i, brain_output, SOCIAL_BRAIN_OUT_DIM, &msg);

            /* Store in global buffer */
            if (si->msg_count < si->msg_capacity) {
                si->message_buffer[si->msg_count++] = msg;
            }

            /* Deliver to all other agents */
            for (uint32_t j = 0; j < si->num_agents; j++) {
                if (j != i) {
                    deliver_message(&si->agents[j], &msg);
                }
            }
            total_messages++;
        }

        /* --- RECEIVE: each agent processes inbox --- */
        for (uint32_t i = 0; i < si->num_agents; i++) {
            nimcp_social_agent_t* a = &si->agents[i];
            uint32_t bd = si->config.belief_dim;

            for (uint32_t m = 0; m < a->inbox_count; m++) {
                nimcp_social_message_t* msg = &a->inbox[m];
                uint32_t sender = msg->sender_id;

                /* Update ToM model of sender */
                update_tom(a, sender, msg->embedding, msg->embed_dim, bd);

                /* Update trust based on prediction accuracy */
                update_trust(a, sender, msg->embedding, msg->embed_dim,
                             bd, si->config.trust_learning_rate);

                /* Detect deception: if trust drops very low, flag it */
                if (a->trust_scores[sender] < 0.2f &&
                    si->agents[sender].role == NIMCP_SOCIAL_ROLE_ADVERSARY) {
                    deceptions_detected++;
                }
            }

            /* Integrate messages into own belief (weighted by trust) */
            integrate_messages(a, bd);

            /* Clear inbox for next round */
            a->inbox_count = 0;
            a->interaction_count++;
        }

        /* --- CHECK CONVERGENCE --- */
        float conv = compute_convergence(si);
        if (conv > 0.9f) {
            LOG_DEBUG("[%s] Early convergence at round %u (sim=%.3f)",
                      LOG_MODULE, round, conv);
            break;
        }

        /* --- LEARN: train brain on social outcomes --- */
        for (uint32_t i = 0; i < si->num_agents; i++) {
            nimcp_social_agent_t* a = &si->agents[i];
            uint32_t mask_dim = (a->mask_dim < state_dim) ? a->mask_dim : state_dim;

            apply_mask(si->environment, a->perspective_mask, masked_input, mask_dim);

            nimcp_brain_learn(si->brain, masked_input, mask_dim,
                              a->belief_state, si->config.belief_dim,
                              "social_cooperation", 0.7f, 0.001f);
        }
    }

    si->total_episodes++;
    si->total_rounds += rounds_completed;

    /* Compute results */
    if (result) {
        memset(result, 0, sizeof(*result));
        result->rounds_completed = rounds_completed;
        result->collective_reward = compute_reward(si, environment_state, state_dim);
        result->belief_convergence = compute_convergence(si);
        result->tom_accuracy = compute_tom_accuracy(si);
        result->messages_exchanged = total_messages;
        result->deceptions_detected = deceptions_detected;
    }

    /* Accumulate per-agent rewards */
    float ep_reward = result ? result->collective_reward : 0.0f;
    for (uint32_t i = 0; i < si->num_agents; i++) {
        si->agents[i].cumulative_reward += ep_reward;
    }

    nimcp_free(masked_input);
    nimcp_free(brain_output);

    LOG_DEBUG("[%s] Episode %u complete: %u rounds, convergence=%.3f, "
              "tom_acc=%.3f, msgs=%u",
              LOG_MODULE, si->total_episodes, rounds_completed,
              result ? result->belief_convergence : 0.0f,
              result ? result->tom_accuracy : 0.0f,
              total_messages);

    /* W10 (2026-04-24): emit episode outcome + query convergence bias. */
    if (si->brain && result) {
        wave10_social_emit_episode_outcome(
            (struct brain_struct*)si->brain,
            si->num_agents,
            result->belief_convergence,
            result->collective_reward);
        (void)wave10_social_query_convergence_bias(
            (struct brain_struct*)si->brain);
    }

    return 0;
}

/* ============================================================================
 * Multi-episode training
 * ============================================================================ */

int nimcp_social_train(nimcp_social_interaction_t* si,
                       uint32_t num_episodes, nimcp_social_result_t* aggregate_result)
{
    if (!si || num_episodes == 0) {
        LOG_ERROR("[%s] train: invalid args", LOG_MODULE);
        return -1;
    }

    nimcp_social_result_t agg;
    memset(&agg, 0, sizeof(agg));

    /* Generate environment states with PRNG */
    uint32_t env_dim = si->config.belief_dim;
    float* env = nimcp_calloc(env_dim, sizeof(float));
    if (!env) return -1;

    uint32_t adversary_rotation = 0;

    for (uint32_t ep = 0; ep < num_episodes; ep++) {
        /* Generate random environment */
        for (uint32_t k = 0; k < env_dim; k++) {
            env[k] = social_randf(&si->rng_state) * 2.0f - 1.0f;
        }

        /* Rotate adversary in deception scenarios */
        if (si->config.scenario == NIMCP_SCENARIO_DECEPTION &&
            si->config.enable_deception) {
            /* Reset all to cooperator */
            for (uint32_t i = 0; i < si->num_agents; i++) {
                si->agents[i].role = NIMCP_SOCIAL_ROLE_COOPERATOR;
            }
            /* Assign new adversary */
            uint32_t adv_id = adversary_rotation % si->num_agents;
            si->agents[adv_id].role = NIMCP_SOCIAL_ROLE_ADVERSARY;
            adversary_rotation++;
        }

        /* Curriculum: increase perspective overlap over time */
        float progress = (float)ep / (float)num_episodes;
        float curriculum_overlap = si->config.perspective_overlap +
                                    progress * (1.0f - si->config.perspective_overlap) * 0.3f;
        /* Temporarily increase overlap for mask regeneration */
        float original_overlap = si->config.perspective_overlap;
        si->config.perspective_overlap = curriculum_overlap;

        /* Force mask regeneration by invalidating env_dim */
        uint32_t saved_env_dim = si->env_dim;
        si->env_dim = 0;

        nimcp_social_result_t ep_result;
        int rc = nimcp_social_run_episode(si, env, env_dim, &ep_result);

        si->config.perspective_overlap = original_overlap;

        if (rc != 0) {
            LOG_WARN("[%s] train: episode %u failed", LOG_MODULE, ep);
            continue;
        }

        /* Accumulate */
        agg.rounds_completed += ep_result.rounds_completed;
        agg.collective_reward += ep_result.collective_reward;
        agg.belief_convergence += ep_result.belief_convergence;
        agg.tom_accuracy += ep_result.tom_accuracy;
        agg.messages_exchanged += ep_result.messages_exchanged;
        agg.deceptions_detected += ep_result.deceptions_detected;
    }

    /* Average the accumulated metrics */
    if (num_episodes > 0) {
        agg.rounds_completed /= num_episodes;
        agg.collective_reward /= (float)num_episodes;
        agg.belief_convergence /= (float)num_episodes;
        agg.tom_accuracy /= (float)num_episodes;
    }

    if (aggregate_result) {
        *aggregate_result = agg;
    }

    nimcp_free(env);

    LOG_INFO("[%s] Training complete: %u episodes, avg_reward=%.3f, "
             "avg_convergence=%.3f, avg_tom=%.3f",
             LOG_MODULE, num_episodes, agg.collective_reward,
             agg.belief_convergence, agg.tom_accuracy);
    return 0;
}

/* ============================================================================
 * Accessors
 * ============================================================================ */

int nimcp_social_get_belief(const nimcp_social_interaction_t* si,
                            uint32_t agent_id, float* belief, uint32_t max_dim) {
    if (!si || !belief) return -1;
    if (agent_id >= si->num_agents) return -1;

    const nimcp_social_agent_t* a = &si->agents[agent_id];
    uint32_t dim = (a->belief_dim < max_dim) ? a->belief_dim : max_dim;
    memcpy(belief, a->belief_state, dim * sizeof(float));
    return (int)dim;
}

int nimcp_social_get_tom_model(const nimcp_social_interaction_t* si,
                               uint32_t agent_a, uint32_t agent_b,
                               float* model, uint32_t max_dim) {
    if (!si || !model) return -1;
    if (agent_a >= si->num_agents || agent_b >= si->num_agents) return -1;

    const nimcp_social_agent_t* a = &si->agents[agent_a];
    uint32_t dim = (a->belief_dim < max_dim) ? a->belief_dim : max_dim;
    const float* tom = &a->tom_models[agent_b * a->belief_dim];
    memcpy(model, tom, dim * sizeof(float));
    return (int)dim;
}

float nimcp_social_get_trust(const nimcp_social_interaction_t* si,
                             uint32_t from_agent, uint32_t to_agent) {
    if (!si) return 0.0f;
    if (from_agent >= si->num_agents || to_agent >= si->num_agents) return 0.0f;
    return si->agents[from_agent].trust_scores[to_agent];
}
