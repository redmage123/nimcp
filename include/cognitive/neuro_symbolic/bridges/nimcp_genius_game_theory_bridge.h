/**
 * @file nimcp_genius_game_theory_bridge.h
 * @brief Bridge between Genius Module and Game Theory
 *
 * @author NIMCP Team
 * @version 2.6.3
 */

#ifndef NIMCP_GENIUS_GAME_THEORY_BRIDGE_H
#define NIMCP_GENIUS_GAME_THEORY_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/bridge/nimcp_bridge_base.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_GENIUS_GT_BRIDGE  0x039C

typedef struct genius_gt_bridge {
    bridge_base_t base;
    bool enable_nash_equilibrium;
    bool enable_shapley_attribution;
    uint64_t games_analyzed;
} genius_gt_bridge_t;

NIMCP_API genius_gt_bridge_t* genius_gt_bridge_create(void);
NIMCP_API void genius_gt_bridge_destroy(genius_gt_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GENIUS_GAME_THEORY_BRIDGE_H */
