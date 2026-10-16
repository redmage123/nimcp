/**
 * @file nimcp_cerebellum_jepa_bridge.h
 * @brief Cerebellum ↔ JEPA sensorimotor prediction bridge (Phase 4s).
 *
 * Cerebellum biologically implements forward models: given a motor
 * command, predict the sensory outcome. That's exactly JEPA's role,
 * one level up: given a motor-command embedding, predict an outcome
 * embedding in latent space.
 *
 * The bridge:
 *  - Owns a shared jepa_predictor_t sized to the motor/outcome dim
 *  - On each record(motor_cmd, outcome) call, trains JEPA on that pair
 *  - Provides a predict(motor_cmd) for ahead-of-time outcome estimation
 *
 * Complements (does not replace) the cerebellum's own forward model.
 */
#ifndef NIMCP_CEREBELLUM_JEPA_BRIDGE_H
#define NIMCP_CEREBELLUM_JEPA_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cerebellum_jepa_bridge_s cerebellum_jepa_bridge_t;

/**
 * @brief Create a new cerebellum JEPA bridge. Returns NULL on failure.
 * @param embed_dim Dimension of motor_command + outcome vectors.
 */
cerebellum_jepa_bridge_t* cerebellum_jepa_bridge_create(uint32_t embed_dim);

/** Tear down. NULL-safe, idempotent. */
void cerebellum_jepa_bridge_destroy(cerebellum_jepa_bridge_t* b);

/**
 * @brief Train one step on (motor_cmd → outcome) pair. Returns 0 on
 *        success, -1 on NULL args / dim mismatch / underlying failure.
 */
int cerebellum_jepa_bridge_record(cerebellum_jepa_bridge_t* b,
                                   const float* motor_cmd,
                                   const float* outcome,
                                   uint32_t dim,
                                   float* loss_out);

/**
 * @brief Forward-predict outcome embed from a motor-command embed.
 */
int cerebellum_jepa_bridge_predict(cerebellum_jepa_bridge_t* b,
                                    const float* motor_cmd,
                                    uint32_t dim,
                                    float* out_pred);

uint32_t cerebellum_jepa_bridge_n_steps(const cerebellum_jepa_bridge_t* b);
float    cerebellum_jepa_bridge_last_loss(const cerebellum_jepa_bridge_t* b);

/** Self-driving tick: internal prev/cur pair management. Accepts any
 *  dim via truncation or zero-pad against embed_dim. */
int cerebellum_jepa_bridge_tick_from_vec(cerebellum_jepa_bridge_t* b,
                                          const float* vec,
                                          uint32_t dim);

#ifdef __cplusplus
}
#endif

#endif
