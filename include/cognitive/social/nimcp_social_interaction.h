/**
 * @file nimcp_social_interaction.h
 * @brief Multi-agent social interaction framework on a single brain
 *
 * WHAT: Simulates multiple agent personas sharing one neural network
 * WHY:  Enables Theory-of-Mind, cooperation, deception detection, and
 *       joint attention training without multiple brain instances
 * HOW:  Each agent gets a perspective mask (subset of features), independent
 *       belief state, ToM models of other agents, and a communication channel.
 *       Biologically analogous to hemispheric/regional perspective differences.
 */
#ifndef NIMCP_SOCIAL_INTERACTION_H
#define NIMCP_SOCIAL_INTERACTION_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NIMCP_SOCIAL_MAX_AGENTS 8
#define NIMCP_SOCIAL_MAX_MSG_LEN 256

/* Agent role in social scenario */
typedef enum {
    NIMCP_SOCIAL_ROLE_OBSERVER = 0,  /* Sees environment, reports */
    NIMCP_SOCIAL_ROLE_TEACHER,        /* Has knowledge, shares */
    NIMCP_SOCIAL_ROLE_LEARNER,        /* Seeks knowledge, asks */
    NIMCP_SOCIAL_ROLE_COOPERATOR,     /* Works toward shared goal */
    NIMCP_SOCIAL_ROLE_ADVERSARY,      /* Has conflicting goal (for deception detection) */
} nimcp_social_role_t;

/* Message between agents */
typedef struct {
    uint32_t sender_id;
    uint32_t receiver_id;          /* 0xFFFFFFFF = broadcast */
    char content[NIMCP_SOCIAL_MAX_MSG_LEN];
    float embedding[256];           /* Neural embedding of message */
    uint32_t embed_dim;
    float confidence;
    uint64_t timestamp;
} nimcp_social_message_t;

/* Per-agent persona state */
typedef struct {
    uint32_t agent_id;
    nimcp_social_role_t role;
    char name[32];

    /* Perspective mask: which input features this agent can "see" */
    float* perspective_mask;        /* [input_dim] -- 1.0 = visible, 0.0 = hidden */
    uint32_t mask_dim;

    /* Independent state per agent */
    float* belief_state;            /* What this agent believes about the world */
    uint32_t belief_dim;
    float* tom_models;              /* What this agent thinks OTHER agents believe */
    /* Layout: [MAX_AGENTS * belief_dim] -- tom_models[j*belief_dim..] = model of agent j */

    /* Communication history */
    nimcp_social_message_t* inbox;
    uint32_t inbox_count;
    uint32_t inbox_capacity;

    float trust_scores[NIMCP_SOCIAL_MAX_AGENTS]; /* Trust in each other agent */
    uint32_t interaction_count;
    float cumulative_reward;
} nimcp_social_agent_t;

/* Scenario types */
typedef enum {
    NIMCP_SCENARIO_COOPERATIVE = 0,  /* Agents share info to solve puzzle */
    NIMCP_SCENARIO_TEACHING,          /* One teaches, others learn */
    NIMCP_SCENARIO_DEBATE,            /* Agents argue different positions */
    NIMCP_SCENARIO_DECEPTION,         /* One agent gives false info */
    NIMCP_SCENARIO_JOINT_ATTENTION,   /* Agents coordinate observation */
} nimcp_scenario_type_t;

/* Social interaction config */
typedef struct {
    uint32_t num_agents;            /* 2-8 agents */
    nimcp_scenario_type_t scenario;
    uint32_t max_rounds;            /* Max communication rounds per episode */
    uint32_t belief_dim;            /* Dimension of belief state (default 128) */
    float perspective_overlap;       /* 0.0=no overlap, 1.0=full overlap (default 0.3) */
    float trust_learning_rate;       /* How fast trust updates (default 0.1) */
    float communication_noise;       /* Noise added to messages (default 0.05) */
    bool enable_deception;           /* Allow adversary agent (default false) */
} nimcp_social_config_t;

/* Episode result */
typedef struct {
    uint32_t rounds_completed;
    float collective_reward;
    float belief_convergence;       /* How much agents agree (0=disagree, 1=consensus) */
    float tom_accuracy;             /* How well agents predicted each other's beliefs */
    uint32_t messages_exchanged;
    uint32_t deceptions_detected;   /* If adversary present */
} nimcp_social_result_t;

typedef struct nimcp_social_interaction nimcp_social_interaction_t;

/* API */
nimcp_social_interaction_t* nimcp_social_interaction_create(
    void* brain, const nimcp_social_config_t* config);
void nimcp_social_interaction_destroy(nimcp_social_interaction_t* si);

/* Run one social episode: agents observe, communicate, update beliefs */
int nimcp_social_run_episode(nimcp_social_interaction_t* si,
    const float* environment_state, uint32_t state_dim,
    nimcp_social_result_t* result);

/* Run multiple episodes for training */
int nimcp_social_train(nimcp_social_interaction_t* si,
    uint32_t num_episodes, nimcp_social_result_t* aggregate_result);

/* Get agent's current belief state */
int nimcp_social_get_belief(const nimcp_social_interaction_t* si,
    uint32_t agent_id, float* belief, uint32_t max_dim);

/* Get what agent_a thinks agent_b believes */
int nimcp_social_get_tom_model(const nimcp_social_interaction_t* si,
    uint32_t agent_a, uint32_t agent_b, float* model, uint32_t max_dim);

/* Get trust score between agents */
float nimcp_social_get_trust(const nimcp_social_interaction_t* si,
    uint32_t from_agent, uint32_t to_agent);

nimcp_social_config_t nimcp_social_config_default(void);

#ifdef __cplusplus
}
#endif
#endif /* NIMCP_SOCIAL_INTERACTION_H */
