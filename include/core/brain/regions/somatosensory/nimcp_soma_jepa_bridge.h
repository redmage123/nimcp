/**
 * @file nimcp_soma_jepa_bridge.h
 * @brief Somatosensory ↔ JEPA body-state prediction bridge (Phase 4r).
 *
 * Learns transitions in the body-state latent: given last tick's
 * pain+temperature+position summary, predict the current tick's summary.
 * Self-driving — reads the somatosensory module directly per tick.
 */
#ifndef NIMCP_SOMA_JEPA_BRIDGE_H
#define NIMCP_SOMA_JEPA_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>
#include "core/brain/regions/somatosensory/nimcp_somatosensory.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct soma_jepa_bridge_s soma_jepa_bridge_t;

soma_jepa_bridge_t* soma_jepa_bridge_create(nimcp_somatosensory_t* soma,
                                             uint32_t embed_dim);
void soma_jepa_bridge_destroy(soma_jepa_bridge_t* b);

/** Read current soma state, train JEPA on (prev → cur). Returns 0 on
 *  success, -1 if no prev snapshot yet / soma missing / allocation issue. */
int soma_jepa_bridge_train_step(soma_jepa_bridge_t* b, float* loss_out);

uint32_t soma_jepa_bridge_n_steps(const soma_jepa_bridge_t* b);
float    soma_jepa_bridge_last_loss(const soma_jepa_bridge_t* b);

#ifdef __cplusplus
}
#endif

#endif
