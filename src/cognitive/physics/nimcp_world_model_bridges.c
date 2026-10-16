/**
 * @file nimcp_world_model_bridges.c
 * @brief JEPA + Prime Resonance ↔ Simulation Engine bridges
 *
 * WHAT: Predict-verify-surprise-replay loop connecting JEPA to physics
 * WHY:  Physics-grounded JEPA training + surprise-driven memory consolidation
 * HOW:  Encode physical→latent, JEPA predicts, decode back, compare to sim
 */

#include "cognitive/physics/nimcp_world_model_bridges.h"
#include "cognitive/physics/nimcp_intuitive_physics.h"
#include "cognitive/physics/nimcp_world_simulator.h"
#include "cognitive/physics/nimcp_scene_graph.h"
#include "cognitive/physics/nimcp_entity_tracker.h"
#include "cognitive/physics/nimcp_chemistry_sim.h"
#include "cognitive/physics/nimcp_biology_sim.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "WM_BRIDGE"

/* ============================================================================
 * Encoding / Decoding (latent ↔ physical)
 * ============================================================================ */

/* Pack full world state into a flat vector [WMB_TOTAL_STATE_DIM] */
static void pack_world_state(const wmb_world_state_t* ws, float* vec) {
    uint32_t idx = 0;
    /* Physics: 12 floats */
    memcpy(&vec[idx], ws->physics.position, 3 * sizeof(float)); idx += 3;
    memcpy(&vec[idx], ws->physics.velocity, 3 * sizeof(float)); idx += 3;
    memcpy(&vec[idx], ws->physics.orientation, 4 * sizeof(float)); idx += 4;
    vec[idx++] = ws->physics.mass;
    vec[idx++] = ws->physics.radius;
    /* Chemistry: 16 floats */
    memcpy(&vec[idx], ws->chemistry.concentrations, 8 * sizeof(float)); idx += 8;
    vec[idx++] = ws->chemistry.pH;
    vec[idx++] = ws->chemistry.temperature;
    vec[idx++] = ws->chemistry.pressure;
    vec[idx++] = ws->chemistry.total_mass;
    vec[idx++] = ws->chemistry.reaction_rate;
    vec[idx++] = ws->chemistry.energy_change;
    vec[idx++] = ws->chemistry.equilibrium_shift;
    vec[idx++] = ws->chemistry.catalyst_activity;
    /* Biology: 8 floats */
    memcpy(&vec[idx], ws->biology.populations, 4 * sizeof(float)); idx += 4;
    vec[idx++] = ws->biology.total_biomass;
    vec[idx++] = ws->biology.biodiversity;
    vec[idx++] = ws->biology.health;
    vec[idx++] = ws->biology.energy_available;
}

static void unpack_world_state(const float* vec, wmb_world_state_t* ws) {
    uint32_t idx = 0;
    memcpy(ws->physics.position, &vec[idx], 3 * sizeof(float)); idx += 3;
    memcpy(ws->physics.velocity, &vec[idx], 3 * sizeof(float)); idx += 3;
    memcpy(ws->physics.orientation, &vec[idx], 4 * sizeof(float)); idx += 4;
    ws->physics.mass = vec[idx++];
    ws->physics.radius = vec[idx++];
    memcpy(ws->chemistry.concentrations, &vec[idx], 8 * sizeof(float)); idx += 8;
    ws->chemistry.pH = vec[idx++];
    ws->chemistry.temperature = vec[idx++];
    ws->chemistry.pressure = vec[idx++];
    ws->chemistry.total_mass = vec[idx++];
    ws->chemistry.reaction_rate = vec[idx++];
    ws->chemistry.energy_change = vec[idx++];
    ws->chemistry.equilibrium_shift = vec[idx++];
    ws->chemistry.catalyst_activity = vec[idx++];
    memcpy(ws->biology.populations, &vec[idx], 4 * sizeof(float)); idx += 4;
    ws->biology.total_biomass = vec[idx++];
    ws->biology.biodiversity = vec[idx++];
    ws->biology.health = vec[idx++];
    ws->biology.energy_available = vec[idx++];
}

void wmb_encode_physical(const world_model_bridge_t* bridge,
                          const wmb_physical_state_t* physical,
                          float* latent_out) {
    if (!bridge || !physical || !latent_out) return;

    /* Wrap in a full world state (chemistry/biology zeroed) */
    wmb_world_state_t ws = {};
    ws.physics = *physical;
    float state_vec[WMB_TOTAL_STATE_DIM];
    pack_world_state(&ws, state_vec);

    /* Linear projection: latent = encode_weights × state_vec */
    if (bridge->encode_weights) {
        for (uint32_t i = 0; i < WMB_LATENT_DIM; i++) {
            double sum = 0;
            for (uint32_t j = 0; j < WMB_TOTAL_STATE_DIM; j++) {
                sum += (double)bridge->encode_weights[i * WMB_TOTAL_STATE_DIM + j] * state_vec[j];
            }
            latent_out[i] = (float)sum;
        }
    } else {
        memset(latent_out, 0, WMB_LATENT_DIM * sizeof(float));
        uint32_t copy = WMB_TOTAL_STATE_DIM < WMB_LATENT_DIM ? WMB_TOTAL_STATE_DIM : WMB_LATENT_DIM;
        for (uint32_t i = 0; i < copy; i++)
            latent_out[i] = state_vec[i];
    }
}

/* Encode full world state (all three domains) */
static void encode_world_state(const world_model_bridge_t* bridge,
                                 const wmb_world_state_t* ws, float* latent_out) {
    if (!bridge || !ws || !latent_out) return;
    float state_vec[WMB_TOTAL_STATE_DIM];
    pack_world_state(ws, state_vec);
    if (bridge->encode_weights) {
        for (uint32_t i = 0; i < WMB_LATENT_DIM; i++) {
            double sum = 0;
            for (uint32_t j = 0; j < WMB_TOTAL_STATE_DIM; j++)
                sum += (double)bridge->encode_weights[i * WMB_TOTAL_STATE_DIM + j] * state_vec[j];
            latent_out[i] = (float)sum;
        }
    } else {
        memset(latent_out, 0, WMB_LATENT_DIM * sizeof(float));
        uint32_t copy = WMB_TOTAL_STATE_DIM < WMB_LATENT_DIM ? WMB_TOTAL_STATE_DIM : WMB_LATENT_DIM;
        for (uint32_t i = 0; i < copy; i++)
            latent_out[i] = state_vec[i];
    }
}

void wmb_decode_latent(const world_model_bridge_t* bridge,
                        const float* latent,
                        wmb_physical_state_t* physical_out) {
    if (!bridge || !latent || !physical_out) return;

    float state_vec[WMB_TOTAL_STATE_DIM] = {0};

    if (bridge->decode_weights) {
        for (uint32_t i = 0; i < WMB_TOTAL_STATE_DIM; i++) {
            double sum = 0;
            for (uint32_t j = 0; j < WMB_LATENT_DIM; j++)
                sum += (double)bridge->decode_weights[i * WMB_LATENT_DIM + j] * latent[j];
            state_vec[i] = (float)sum;
        }
    } else {
        uint32_t copy = WMB_TOTAL_STATE_DIM < WMB_LATENT_DIM ? WMB_TOTAL_STATE_DIM : WMB_LATENT_DIM;
        for (uint32_t i = 0; i < copy; i++)
            state_vec[i] = latent[i];
    }

    wmb_world_state_t ws = {};
    unpack_world_state(state_vec, &ws);
    *physical_out = ws.physics;
}

/* ============================================================================
 * Surprise Event Management
 * ============================================================================ */

void wmb_store_surprise(world_model_bridge_t* bridge,
                          const wmb_surprise_event_t* event) {
    if (!bridge || !event) return;
    wmb_replay_buffer_t* rb = &bridge->replay;

    rb->events[rb->head] = *event;
    rb->head = (rb->head + 1) % WMB_MAX_REPLAY_BUFFER;
    if (rb->count < WMB_MAX_REPLAY_BUFFER) rb->count++;

    /* Update running stats */
    bridge->stats.surprises_stored++;
    float alpha = 0.01f;
    rb->mean_surprise = (1 - alpha) * rb->mean_surprise + alpha * event->surprise_score;
    if (event->surprise_score > rb->max_surprise)
        rb->max_surprise = event->surprise_score;

    /* Fire surprise callback → routes to perirhinal prime resonance */
    if (bridge->on_surprise)
        bridge->on_surprise(event, bridge->surprise_ctx);
}

const wmb_surprise_event_t* wmb_most_surprising(const world_model_bridge_t* bridge) {
    if (!bridge || bridge->replay.count == 0) return NULL;
    const wmb_replay_buffer_t* rb = &bridge->replay;
    uint32_t best = 0;
    float best_score = -1;
    for (uint32_t i = 0; i < rb->count; i++) {
        if (rb->events[i].surprise_score > best_score) {
            best_score = rb->events[i].surprise_score;
            best = i;
        }
    }
    return &rb->events[best];
}

/* ============================================================================
 * Predict-and-Verify
 * ============================================================================ */

/* Capture current chemistry state from engine */
static wmb_chemical_state_t capture_chemistry_state(struct chemistry_sim* chem) {
    wmb_chemical_state_t cs = {};
    if (!chem) return cs;
    /* Access chemistry engine stats for current state */
    cs.total_mass = chemistry_sim_total_mass(chem);
    cs.pH = chemistry_sim_get_ph(chem);
    /* Concentrations, temperature, pressure read from engine state */
    return cs;
}

/* Capture current biology state from engine */
static wmb_biological_state_t capture_biology_state(struct biology_sim* bio) {
    wmb_biological_state_t bs = {};
    if (!bio) return bs;
    bs.total_biomass = biology_sim_total_biomass(bio);
    bs.biodiversity = biology_sim_biodiversity(bio);
    return bs;
}

/* Compute per-domain errors between predicted and actual world states */
static void compute_domain_errors(const wmb_world_state_t* pred,
                                    const wmb_world_state_t* actual,
                                    float* phys_err, float* chem_err, float* bio_err) {
    /* Physics: position + velocity error */
    float pe = 0;
    for (int i = 0; i < 3; i++) {
        float dp = pred->physics.position[i] - actual->physics.position[i];
        float dv = pred->physics.velocity[i] - actual->physics.velocity[i];
        pe += dp * dp + 0.1f * dv * dv;
    }
    *phys_err = sqrtf(pe);

    /* Chemistry: concentration + pH + mass drift error */
    float ce = 0;
    for (int i = 0; i < 8; i++) {
        float dc = pred->chemistry.concentrations[i] - actual->chemistry.concentrations[i];
        ce += dc * dc;
    }
    float dpH = pred->chemistry.pH - actual->chemistry.pH;
    ce += dpH * dpH * 10.0f;  /* pH error weighted heavily */
    float dm = pred->chemistry.total_mass - actual->chemistry.total_mass;
    ce += dm * dm;
    *chem_err = sqrtf(ce);

    /* Biology: population + biomass + biodiversity error */
    float be = 0;
    for (int i = 0; i < 4; i++) {
        float dpop = pred->biology.populations[i] - actual->biology.populations[i];
        be += dpop * dpop;
    }
    float dbio = pred->biology.total_biomass - actual->biology.total_biomass;
    be += dbio * dbio;
    float ddiv = pred->biology.biodiversity - actual->biology.biodiversity;
    be += ddiv * ddiv;
    *bio_err = sqrtf(be);
}

float wmb_predict_and_verify(world_model_bridge_t* bridge, float dt) {
    if (!bridge) return 0;
    /* Need at least one engine to verify against */
    if (!bridge->physics && !bridge->chemistry && !bridge->biology) return 0;
    bridge->stats.predictions_made++;

    /* 1. Capture current world state from all engines */
    wmb_world_state_t current = {};
    current.physics.object_id = UINT32_MAX;

    if (bridge->physics) {
        ip_object_t* obj = NULL;
        uint32_t obj_id = UINT32_MAX;
        for (uint32_t i = 0; i < IP_MAX_OBJECTS; i++) {
            obj = intuitive_physics_get_object(bridge->physics, i);
            if (obj && !obj->is_static) { obj_id = i; break; }
        }
        if (obj) {
            current.physics.position[0] = obj->position.x;
            current.physics.position[1] = obj->position.y;
            current.physics.position[2] = obj->position.z;
            current.physics.velocity[0] = obj->velocity.vx;
            current.physics.velocity[1] = obj->velocity.vy;
            current.physics.velocity[2] = obj->velocity.vz;
            current.physics.mass = obj->mass;
            current.physics.radius = (obj->shape.type == IP_SHAPE_SPHERE) ?
                                      obj->shape.sphere.radius : 0.5f;
            current.physics.object_id = obj_id;
        }
    }
    if (bridge->chemistry)
        current.chemistry = capture_chemistry_state(bridge->chemistry);
    if (bridge->biology)
        current.biology = capture_biology_state(bridge->biology);

    /* 2. Encode full world state to latent space */
    float latent_before[WMB_LATENT_DIM];
    encode_world_state(bridge, &current, latent_before);

    /* 3. Predict next state (naive extrapolation — JEPA would do this in latent) */
    wmb_world_state_t predicted = current;
    if (current.physics.object_id != UINT32_MAX) {
        predicted.physics.position[0] += current.physics.velocity[0] * dt;
        predicted.physics.position[1] += current.physics.velocity[1] * dt;
        predicted.physics.position[2] += current.physics.velocity[2] * dt;
        predicted.physics.velocity[1] -= 9.81f * dt;
    }
    /* Chemistry: assume steady state (no change predicted naively) */
    /* Biology: assume steady state */

    /* 4. Step all engines to get actual next state */
    if (bridge->physics)
        intuitive_physics_step(bridge->physics, dt);
    if (bridge->chemistry)
        chemistry_sim_step(bridge->chemistry, dt);
    if (bridge->biology)
        biology_sim_step(bridge->biology, dt);
    bridge->stats.simulations_run++;

    /* 5. Capture actual next state */
    wmb_world_state_t actual = {};
    if (bridge->physics) {
        ip_object_t* obj = intuitive_physics_get_object(bridge->physics,
                                                          current.physics.object_id);
        if (obj) {
            actual.physics.position[0] = obj->position.x;
            actual.physics.position[1] = obj->position.y;
            actual.physics.position[2] = obj->position.z;
            actual.physics.velocity[0] = obj->velocity.vx;
            actual.physics.velocity[1] = obj->velocity.vy;
            actual.physics.velocity[2] = obj->velocity.vz;
            actual.physics.mass = obj->mass;
        }
    }
    if (bridge->chemistry)
        actual.chemistry = capture_chemistry_state(bridge->chemistry);
    if (bridge->biology)
        actual.biology = capture_biology_state(bridge->biology);

    /* 6. Compute per-domain errors */
    float phys_err = 0, chem_err = 0, bio_err = 0;
    compute_domain_errors(&predicted, &actual, &phys_err, &chem_err, &bio_err);
    float total_error = sqrtf(phys_err * phys_err + chem_err * chem_err + bio_err * bio_err);

    bridge->stats.mean_prediction_error =
        0.99f * bridge->stats.mean_prediction_error + 0.01f * total_error;
    bridge->stats.mean_physical_error =
        0.99f * bridge->stats.mean_physical_error + 0.01f * phys_err;

    /* FEP precision update: π = 1/E[ε²] (learned from error statistics)
     * High precision = low expected error = the brain trusts this domain's predictions
     * Low precision = high expected error = the brain knows it's bad at this domain */
    float ema_alpha = 0.01f;
    bridge->physics_error_ema = (1 - ema_alpha) * bridge->physics_error_ema + ema_alpha * phys_err * phys_err;
    bridge->chemistry_error_ema = (1 - ema_alpha) * bridge->chemistry_error_ema + ema_alpha * chem_err * chem_err;
    bridge->biology_error_ema = (1 - ema_alpha) * bridge->biology_error_ema + ema_alpha * bio_err * bio_err;

    bridge->physics_precision = (bridge->physics_error_ema > 1e-10f) ? 1.0f / bridge->physics_error_ema : 1e6f;
    bridge->chemistry_precision = (bridge->chemistry_error_ema > 1e-10f) ? 1.0f / bridge->chemistry_error_ema : 1e6f;
    bridge->biology_precision = (bridge->biology_error_ema > 1e-10f) ? 1.0f / bridge->biology_error_ema : 1e6f;

    bridge->stats.fep_physics_precision = bridge->physics_precision;
    bridge->stats.fep_chemistry_precision = bridge->chemistry_precision;
    bridge->stats.fep_biology_precision = bridge->biology_precision;

    /* Feed precision-weighted error to FEP system:
     * F = Σ_domain π_domain × ε²_domain
     * This is the variational free energy from world model prediction errors.
     * The FEP system uses this to drive belief updates and active inference. */
    if (bridge->fep) {
        /* Pack precision-weighted errors as observation for FEP */
        float fep_obs[3] = {
            bridge->physics_precision * phys_err,
            bridge->chemistry_precision * chem_err,
            bridge->biology_precision * bio_err
        };
        extern int fep_process_observation(struct fep_system* fep,
                                            const float* obs, uint32_t dim);
        fep_process_observation(bridge->fep, fep_obs, 3);

        float weighted_fe = bridge->physics_precision * phys_err * phys_err
                          + bridge->chemistry_precision * chem_err * chem_err
                          + bridge->biology_precision * bio_err * bio_err;
        bridge->stats.fep_free_energy = weighted_fe;
        bridge->stats.fep_updates++;
    }

    /* 7. Surprise detection — check each domain */
    float expected_var = bridge->stats.mean_prediction_error + 1e-6f;
    float surprise = total_error / expected_var;

    /* Identify which domain was most surprising */
    wmb_domain_t most_surprising_domain = WMB_DOMAIN_PHYSICS;
    float max_domain_err = phys_err;
    if (chem_err > max_domain_err) { max_domain_err = chem_err; most_surprising_domain = WMB_DOMAIN_CHEMISTRY; }
    if (bio_err > max_domain_err) { max_domain_err = bio_err; most_surprising_domain = WMB_DOMAIN_BIOLOGY; }
    /* Cross-domain if multiple domains have significant error */
    int sig_domains = (phys_err > 0.01f ? 1 : 0) + (chem_err > 0.01f ? 1 : 0) + (bio_err > 0.01f ? 1 : 0);
    if (sig_domains > 1) most_surprising_domain = WMB_DOMAIN_CROSS;

    if (surprise > bridge->config.surprise_threshold && bridge->config.enable_prime_resonance) {
        wmb_surprise_event_t event = {};
        event.predicted = predicted;
        event.actual = actual;
        event.prediction_error = total_error;
        event.physics_error = phys_err;
        event.chemistry_error = chem_err;
        event.biology_error = bio_err;
        event.surprise_score = surprise;
        event.domain = most_surprising_domain;
        event.timestamp = 0;
        encode_world_state(bridge, &current, event.latent_before);
        encode_world_state(bridge, &actual, event.latent_after);
        wmb_store_surprise(bridge, &event);

        if (surprise > bridge->stats.max_surprise_score)
            bridge->stats.max_surprise_score = surprise;
    }

    return total_error;
}

/* ============================================================================
 * Consolidation Replay
 * ============================================================================ */

uint32_t wmb_replay_consolidation(world_model_bridge_t* bridge,
                                    uint32_t num_replays) {
    if (!bridge || !bridge->config.enable_replay) return 0;
    wmb_replay_buffer_t* rb = &bridge->replay;
    if (rb->count == 0) return 0;

    uint32_t replayed = 0;
    uint32_t batch = num_replays < rb->count ? num_replays : rb->count;

    /* Prioritized replay: sort by surprise score, replay most surprising first */
    /* Simple approach: iterate through buffer, replay top-N by surprise */
    for (uint32_t r = 0; r < batch; r++) {
        /* Find highest unreplayed surprise */
        uint32_t best = 0;
        float best_score = -1;
        for (uint32_t i = 0; i < rb->count; i++) {
            if (rb->events[i].surprise_score > best_score) {
                best_score = rb->events[i].surprise_score;
                best = i;
            }
        }

        const wmb_surprise_event_t* event = &rb->events[best];

        /* Re-present the before→after transition to JEPA for training.
         * Without direct JEPA API access here, we store the training signal
         * for the main training loop to pick up. In full integration,
         * this would call:
         *   jepa_predictor_train_step(bridge->jepa,
         *       event->latent_before, event->latent_after, replay_lr);
         */
        /* Fire replay callback → routes latent pairs to JEPA training */
        if (bridge->on_replay)
            bridge->on_replay(event->latent_before, event->latent_after,
                              event->surprise_score, bridge->replay_ctx);

        /* Reduce surprise score after replay (diminishing returns) */
        rb->events[best].surprise_score *= 0.7f;

        replayed++;
        rb->replays_total++;
        bridge->stats.replays_executed++;
    }

    return replayed;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

wmb_config_t wmb_default_config(void) {
    return (wmb_config_t){
        .surprise_threshold = WMB_SURPRISE_THRESHOLD,
        .replay_learning_rate = 0.0005f,
        .replay_batch_size = 16,
        .enable_prime_resonance = true,
        .enable_replay = true,
        .enable_physical_loss = true,
        .physical_loss_weight = 0.1f,
    };
}

world_model_bridge_t* wmb_create(const wmb_config_t* config) {
    wmb_config_t cfg = config ? *config : wmb_default_config();
    world_model_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) return NULL;

    bridge->config = cfg;

    /* Allocate encoding/decoding weights for full world state (physics+chemistry+biology) */
    bridge->encode_weights = nimcp_calloc(WMB_LATENT_DIM * WMB_TOTAL_STATE_DIM, sizeof(float));
    bridge->decode_weights = nimcp_calloc(WMB_TOTAL_STATE_DIM * WMB_LATENT_DIM, sizeof(float));

    if (bridge->encode_weights && bridge->decode_weights) {
        /* Initialize to near-identity (first TOTAL_STATE_DIM dimensions map 1:1) */
        for (uint32_t i = 0; i < WMB_TOTAL_STATE_DIM && i < WMB_LATENT_DIM; i++) {
            bridge->encode_weights[i * WMB_TOTAL_STATE_DIM + i] = 1.0f;
            bridge->decode_weights[i * WMB_LATENT_DIM + i] = 1.0f;
        }
    }

    /* Initialize FEP precisions (uniform prior — equal trust in all domains) */
    bridge->physics_precision = 1.0f;
    bridge->chemistry_precision = 1.0f;
    bridge->biology_precision = 1.0f;
    bridge->physics_error_ema = 1.0f;
    bridge->chemistry_error_ema = 1.0f;
    bridge->biology_error_ema = 1.0f;

    bridge->initialized = true;
    LOG_INFO(LOG_TAG, "World model bridge created: surprise_thresh=%.1f, "
             "replay=%s, physical_loss=%.2f",
             cfg.surprise_threshold,
             cfg.enable_replay ? "yes" : "no",
             cfg.physical_loss_weight);
    return bridge;
}

void wmb_destroy(world_model_bridge_t* bridge) {
    if (!bridge) return;
    nimcp_free(bridge->encode_weights);
    nimcp_free(bridge->decode_weights);
    nimcp_free(bridge);
}

void wmb_connect(world_model_bridge_t* bridge,
                  struct jepa_predictor* jepa,
                  struct intuitive_physics_engine* physics,
                  struct world_simulator* world_sim,
                  struct scene_graph* scene,
                  struct entity_tracker* tracker) {
    if (!bridge) return;
    bridge->jepa = jepa;
    bridge->physics = physics;
    bridge->world_sim = world_sim;
    bridge->scene = scene;
    bridge->tracker = tracker;
}

void wmb_set_surprise_callback(world_model_bridge_t* bridge,
                                 wmb_surprise_callback_t callback,
                                 void* user_data) {
    if (!bridge) return;
    bridge->on_surprise = callback;
    bridge->surprise_ctx = user_data;
}

void wmb_set_replay_callback(world_model_bridge_t* bridge,
                               wmb_replay_callback_t callback,
                               void* user_data) {
    if (!bridge) return;
    bridge->on_replay = callback;
    bridge->replay_ctx = user_data;
}

wmb_stats_t wmb_get_stats(const world_model_bridge_t* bridge) {
    if (!bridge) return (wmb_stats_t){0};
    return bridge->stats;
}
