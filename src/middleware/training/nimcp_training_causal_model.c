/**
 * @file nimcp_training_causal_model.c
 * @brief Causal Model of Training Dynamics — implementation
 *
 * WHAT: Builds and queries a causal DAG of training hyperparameters and outcomes
 * WHY:  Enables "what if" reasoning before changing training parameters,
 *       preventing cascading misdiagnosis from correlated symptoms
 * HOW:  Wraps causal_dag_t with training-specific nodes, edges, and query helpers
 *
 * @version 1.0.0
 * @date 2026-02-26
 */

#include "middleware/training/nimcp_training_causal_model.h"
#include "cognitive/reasoning/nimcp_reasoning_causal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

/* Health agent boilerplate */
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(training_causal_model)

#define LOG_MODULE "training_causal_model"

/*=============================================================================
 * NODE NAMES (indexed by training_causal_node_t)
 *===========================================================================*/

static const char* const s_node_names[TRAIN_CAUSAL_NODE_COUNT] = {
    "learning_rate",
    "batch_size",
    "gradient_clipping",
    "regularization",
    "gradient_magnitude",
    "gradient_noise",
    "gradient_variance",
    "loss_trajectory",
    "loss_volatility",
    "convergence_speed",
    "effective_capacity",
    "arousal_level",
    "inflammation_level",
    "resource_pressure"
};

/*=============================================================================
 * DEFAULT PRIORS
 *===========================================================================*/

static const float s_node_priors[TRAIN_CAUSAL_NODE_COUNT] = {
    0.5f,  /* learning_rate */
    0.5f,  /* batch_size */
    0.5f,  /* gradient_clipping */
    0.5f,  /* regularization */
    0.5f,  /* gradient_magnitude */
    0.5f,  /* gradient_noise */
    0.5f,  /* gradient_variance */
    0.5f,  /* loss_trajectory */
    0.5f,  /* loss_volatility */
    0.5f,  /* convergence_speed */
    0.5f,  /* effective_capacity */
    0.5f,  /* arousal_level */
    0.5f,  /* inflammation_level */
    0.5f   /* resource_pressure */
};

/*=============================================================================
 * CAUSAL EDGE DEFINITIONS
 *===========================================================================*/

typedef struct {
    training_causal_node_t from;
    training_causal_node_t to;
    float strength;
} training_causal_edge_def_t;

static const training_causal_edge_def_t s_edge_defs[] = {
    /* LR → gradient_magnitude (higher LR = larger gradients) */
    { TRAIN_CAUSAL_LEARNING_RATE,     TRAIN_CAUSAL_GRADIENT_MAGNITUDE, 0.8f },
    /* LR → loss_trajectory (direct effect on loss) */
    { TRAIN_CAUSAL_LEARNING_RATE,     TRAIN_CAUSAL_LOSS_TRAJECTORY,    0.7f },
    /* batch_size → gradient_noise (larger batch = less noise) */
    { TRAIN_CAUSAL_BATCH_SIZE,        TRAIN_CAUSAL_GRADIENT_NOISE,     0.6f },
    /* gradient_clipping → gradient_variance (clipping controls variance) */
    { TRAIN_CAUSAL_GRADIENT_CLIPPING, TRAIN_CAUSAL_GRADIENT_VARIANCE,  0.7f },
    /* gradient_magnitude → loss_trajectory (gradient magnitude drives loss change) */
    { TRAIN_CAUSAL_GRADIENT_MAGNITUDE, TRAIN_CAUSAL_LOSS_TRAJECTORY,   0.8f },
    /* gradient_noise → gradient_variance (noise contributes to variance) */
    { TRAIN_CAUSAL_GRADIENT_NOISE,    TRAIN_CAUSAL_GRADIENT_VARIANCE,  0.6f },
    /* gradient_noise → loss_volatility (noisy gradients cause volatile loss) */
    { TRAIN_CAUSAL_GRADIENT_NOISE,    TRAIN_CAUSAL_LOSS_VOLATILITY,    0.5f },
    /* gradient_variance → loss_trajectory (high variance destabilizes loss) */
    { TRAIN_CAUSAL_GRADIENT_VARIANCE, TRAIN_CAUSAL_LOSS_TRAJECTORY,    0.7f },
    /* loss_volatility → convergence_speed (volatile loss slows convergence) */
    { TRAIN_CAUSAL_LOSS_VOLATILITY,   TRAIN_CAUSAL_CONVERGENCE_SPEED,  0.6f },
    /* loss_trajectory → convergence_speed (loss drives convergence) */
    { TRAIN_CAUSAL_LOSS_TRAJECTORY,   TRAIN_CAUSAL_CONVERGENCE_SPEED,  0.9f },
    /* regularization → effective_capacity (regularization limits capacity) */
    { TRAIN_CAUSAL_REGULARIZATION,    TRAIN_CAUSAL_EFFECTIVE_CAPACITY,  0.5f },
    /* effective_capacity → loss_trajectory (capacity affects loss floor) */
    { TRAIN_CAUSAL_EFFECTIVE_CAPACITY, TRAIN_CAUSAL_LOSS_TRAJECTORY,    0.6f },
    /* arousal → gradient_magnitude (biological arousal modulates gradient response) */
    { TRAIN_CAUSAL_AROUSAL_LEVEL,     TRAIN_CAUSAL_GRADIENT_MAGNITUDE, 0.4f },
    /* inflammation → convergence_speed (inflammation causes conservative learning) */
    { TRAIN_CAUSAL_INFLAMMATION_LEVEL, TRAIN_CAUSAL_CONVERGENCE_SPEED, 0.5f },
    /* resource_pressure → batch_size (Portia reduces batch under resource pressure) */
    { TRAIN_CAUSAL_RESOURCE_PRESSURE, TRAIN_CAUSAL_BATCH_SIZE,         0.3f },
};

#define NUM_EDGE_DEFS (sizeof(s_edge_defs) / sizeof(s_edge_defs[0]))

/*=============================================================================
 * INTERNAL STRUCTURE
 *===========================================================================*/

struct training_causal_model {
    causal_dag_t* dag;                                        /**< Underlying causal DAG */
    int node_ids[TRAIN_CAUSAL_NODE_COUNT];                    /**< Mapped DAG node IDs */
    training_causal_observation_t last_obs;                   /**< Last observation snapshot */
    bool has_observation;                                      /**< True if at least one observation made */
};

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Wire all causal edges into the DAG
 * @return 0 on success, -1 on error
 */
static int wire_edges(training_causal_model_t* model)
{
    for (size_t i = 0; i < NUM_EDGE_DEFS; i++) {
        int from_id = model->node_ids[s_edge_defs[i].from];
        int to_id   = model->node_ids[s_edge_defs[i].to];

        if (from_id < 0 || to_id < 0) {
            return -1;
        }

        int rc = causal_dag_add_edge(model->dag,
                                      (uint32_t)from_id,
                                      (uint32_t)to_id,
                                      s_edge_defs[i].strength);
        if (rc != 0) {
            return -1;
        }
    }
    return 0;
}

/**
 * @brief Perform a do-calculus intervention query
 *
 * Applies do(node_id = value), queries target, then clears intervention.
 *
 * @return 0 on success, -1 on error
 */
static int do_intervention_query(training_causal_model_t* model,
                                  training_causal_node_t node,
                                  float value,
                                  training_causal_node_t target,
                                  training_intervention_result_t* result)
{
    int node_id   = model->node_ids[node];
    int target_id = model->node_ids[target];

    if (node_id < 0 || target_id < 0) {
        return -1;
    }

    /* Apply do-operator */
    int rc = causal_dag_intervene(model->dag, (uint32_t)node_id, value);
    if (rc != 0) {
        return -1;
    }

    /* Build query */
    causal_query_t query;
    memset(&query, 0, sizeof(query));
    query.type = CAUSAL_QUERY_INTERVENTION;
    query.target_id = (uint32_t)target_id;
    query.intervention_id = (uint32_t)node_id;
    query.intervention_value = value;

    causal_result_t cr;
    memset(&cr, 0, sizeof(cr));
    rc = causal_dag_query(model->dag, &query, &cr);

    /* Always clear intervention */
    causal_dag_clear_intervention(model->dag, (uint32_t)node_id);

    if (rc != 0) {
        return -1;
    }

    /* Translate causal_result_t → training_intervention_result_t */
    result->predicted_effect = cr.probability;
    result->confidence       = cr.confidence;
    result->causal_strength  = cr.causal_strength;

    /* Clamp confidence to [0,1] */
    if (result->confidence < 0.0f) result->confidence = 0.0f;
    if (result->confidence > 1.0f) result->confidence = 1.0f;

    /* Determine if beneficial:
     * For loss_trajectory: lower predicted_effect is better
     * For convergence_speed: higher predicted_effect is better
     */
    if (target == TRAIN_CAUSAL_LOSS_TRAJECTORY) {
        /* Lower loss = beneficial. Compare to current observed loss (prior = 0.5 if unobserved) */
        float baseline = model->has_observation ? model->last_obs.loss_current : 0.5f;
        /* Normalize: if baseline > 0, beneficial if predicted < baseline (normalized) */
        result->is_beneficial = (cr.probability < baseline);
    } else if (target == TRAIN_CAUSAL_CONVERGENCE_SPEED) {
        /* Higher convergence speed = beneficial */
        result->is_beneficial = (cr.probability > 0.5f);
    } else {
        result->is_beneficial = (cr.probability < 0.5f);
    }

    /* Copy explanation */
    strncpy(result->explanation, cr.explanation, sizeof(result->explanation) - 1);
    result->explanation[sizeof(result->explanation) - 1] = '\0';

    return 0;
}

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

training_causal_model_t* training_causal_model_create(void)
{
    training_causal_model_t* model =
        (training_causal_model_t*)nimcp_calloc(1, sizeof(training_causal_model_t));
    if (!model) {
        return NULL;
    }

    /* Create the underlying causal DAG with default config */
    causal_dag_config_t dag_config = causal_dag_default_config();
    model->dag = causal_dag_create(&dag_config);
    if (!model->dag) {
        nimcp_free(model);
        return NULL;
    }

    /* Add all training causal nodes */
    for (int i = 0; i < (int)TRAIN_CAUSAL_NODE_COUNT; i++) {
        int node_id = causal_dag_add_node(model->dag, s_node_names[i], s_node_priors[i]);
        if (node_id < 0) {
            causal_dag_destroy(model->dag);
            nimcp_free(model);
            return NULL;
        }
        model->node_ids[i] = node_id;
    }

    /* Wire the causal edges */
    if (wire_edges(model) != 0) {
        causal_dag_destroy(model->dag);
        nimcp_free(model);
        return NULL;
    }

    model->has_observation = false;
    memset(&model->last_obs, 0, sizeof(model->last_obs));

    return model;
}

void training_causal_model_destroy(training_causal_model_t* model)
{
    if (!model) return;

    if (model->dag) {
        causal_dag_destroy(model->dag);
        model->dag = NULL;
    }

    nimcp_free(model);
}

/*=============================================================================
 * OBSERVATION
 *===========================================================================*/

int training_causal_model_observe(training_causal_model_t* model,
                                   const training_causal_observation_t* obs)
{
    if (!model || !obs) return -1;
    if (!model->dag)     return -1;

    /* Observe each metric that is not NAN */
    struct {
        training_causal_node_t node;
        float value;
    } observations[] = {
        { TRAIN_CAUSAL_LEARNING_RATE,      obs->learning_rate     },
        { TRAIN_CAUSAL_BATCH_SIZE,         obs->batch_size        },
        { TRAIN_CAUSAL_GRADIENT_CLIPPING,  obs->gradient_clip     },
        { TRAIN_CAUSAL_REGULARIZATION,     obs->regularization    },
        { TRAIN_CAUSAL_GRADIENT_MAGNITUDE, obs->gradient_norm     },
        { TRAIN_CAUSAL_LOSS_TRAJECTORY,    obs->loss_current      },
        { TRAIN_CAUSAL_LOSS_VOLATILITY,    obs->loss_volatility   },
        { TRAIN_CAUSAL_GRADIENT_VARIANCE,  obs->gradient_variance },
        { TRAIN_CAUSAL_AROUSAL_LEVEL,      obs->arousal_level     },
        { TRAIN_CAUSAL_INFLAMMATION_LEVEL, obs->inflammation_level },
        { TRAIN_CAUSAL_RESOURCE_PRESSURE,  obs->resource_pressure },
    };

    size_t count = sizeof(observations) / sizeof(observations[0]);
    for (size_t i = 0; i < count; i++) {
        if (!isnan(observations[i].value)) {
            int node_id = model->node_ids[observations[i].node];
            if (node_id >= 0) {
                causal_dag_observe(model->dag, (uint32_t)node_id,
                                    observations[i].value);
            }
        }
    }

    /* Store the observation snapshot */
    model->last_obs = *obs;
    model->has_observation = true;

    return 0;
}

/*=============================================================================
 * INTERVENTION QUERIES
 *===========================================================================*/

int training_causal_model_query_lr_intervention(training_causal_model_t* model,
                                                  float proposed_lr,
                                                  training_intervention_result_t* result)
{
    if (!model || !result) return -1;
    if (!model->dag)       return -1;

    return do_intervention_query(model,
                                 TRAIN_CAUSAL_LEARNING_RATE,
                                 proposed_lr,
                                 TRAIN_CAUSAL_LOSS_TRAJECTORY,
                                 result);
}

int training_causal_model_query_batch_intervention(training_causal_model_t* model,
                                                     float proposed_batch_factor,
                                                     training_intervention_result_t* result)
{
    if (!model || !result) return -1;
    if (!model->dag)       return -1;

    return do_intervention_query(model,
                                 TRAIN_CAUSAL_BATCH_SIZE,
                                 proposed_batch_factor,
                                 TRAIN_CAUSAL_LOSS_TRAJECTORY,
                                 result);
}

int training_causal_model_query_clip_intervention(training_causal_model_t* model,
                                                    float proposed_clip,
                                                    training_intervention_result_t* result)
{
    if (!model || !result) return -1;
    if (!model->dag)       return -1;

    return do_intervention_query(model,
                                 TRAIN_CAUSAL_GRADIENT_CLIPPING,
                                 proposed_clip,
                                 TRAIN_CAUSAL_LOSS_TRAJECTORY,
                                 result);
}

int training_causal_model_query_intervention(training_causal_model_t* model,
                                               training_causal_node_t node,
                                               float value,
                                               training_causal_node_t target,
                                               training_intervention_result_t* result)
{
    if (!model || !result)                     return -1;
    if (!model->dag)                           return -1;
    if (node >= TRAIN_CAUSAL_NODE_COUNT)       return -1;
    if (target >= TRAIN_CAUSAL_NODE_COUNT)     return -1;

    return do_intervention_query(model, node, value, target, result);
}

/*=============================================================================
 * EXPLANATION
 *===========================================================================*/

int training_causal_model_explain_state(const training_causal_model_t* model,
                                          char* buf, size_t buf_len)
{
    if (!model || !buf || buf_len == 0) return -1;
    if (!model->dag)                    return -1;

    int offset = 0;
    int remaining = (int)buf_len;

    /* Header */
    int written = snprintf(buf + offset, (size_t)remaining,
                            "Training Causal Model State:\n");
    if (written > 0 && written < remaining) {
        offset += written;
        remaining -= written;
    }

    /* DAG statistics */
    causal_dag_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    if (causal_dag_get_stats(model->dag, &stats) == 0 && remaining > 0) {
        written = snprintf(buf + offset, (size_t)remaining,
                            "  Nodes: %u, Edges: %u, Queries: %u\n",
                            stats.num_nodes, stats.num_edges, stats.num_queries);
        if (written > 0 && written < remaining) {
            offset += written;
            remaining -= written;
        }
    }

    /* Last observation summary */
    if (model->has_observation && remaining > 0) {
        const training_causal_observation_t* obs = &model->last_obs;
        written = snprintf(buf + offset, (size_t)remaining,
                            "  Last observation:\n"
                            "    LR=%.6f, batch=%.0f, clip=%.3f, reg=%.4f\n"
                            "    grad_norm=%.4f, loss=%.4f, volatility=%.4f\n"
                            "    grad_var=%.4f, arousal=%.2f, inflammation=%.2f\n"
                            "    resource_pressure=%.2f\n",
                            obs->learning_rate, obs->batch_size,
                            obs->gradient_clip, obs->regularization,
                            obs->gradient_norm, obs->loss_current,
                            obs->loss_volatility, obs->gradient_variance,
                            obs->arousal_level, obs->inflammation_level,
                            obs->resource_pressure);
        if (written > 0 && written < remaining) {
            offset += written;
            remaining -= written;
        }
    } else if (remaining > 0) {
        written = snprintf(buf + offset, (size_t)remaining,
                            "  No observations recorded yet.\n");
        if (written > 0 && written < remaining) {
            offset += written;
            remaining -= written;
        }
    }

    /* Key causal relationships */
    if (remaining > 0) {
        written = snprintf(buf + offset, (size_t)remaining,
                            "  Key causal paths:\n"
                            "    LR --(0.8)--> grad_magnitude --(0.8)--> loss\n"
                            "    LR --(0.7)--> loss (direct)\n"
                            "    batch --(0.6)--> grad_noise --(0.5)--> volatility\n"
                            "    inflammation --(0.5)--> convergence_speed\n");
        if (written > 0 && written < remaining) {
            offset += written;
            remaining -= written;
        }
    }

    /* Ensure null termination */
    buf[buf_len - 1] = '\0';
    return 0;
}
