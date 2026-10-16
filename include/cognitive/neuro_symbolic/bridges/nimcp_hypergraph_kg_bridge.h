/**
 * @file nimcp_hypergraph_kg_bridge.h
 * @brief Bridge between Hypergraph and Knowledge Graph
 *
 * @author NIMCP Team
 * @version 2.6.3
 */

#ifndef NIMCP_HYPERGRAPH_KG_BRIDGE_H
#define NIMCP_HYPERGRAPH_KG_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/bridge/nimcp_bridge_base.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_HYPERGRAPH_KG_BRIDGE  0x039B

typedef struct hypergraph_kg_bridge {
    bridge_base_t base;
    bool enable_bidirectional_sync;
    uint64_t syncs_performed;
} hypergraph_kg_bridge_t;

NIMCP_API hypergraph_kg_bridge_t* hypergraph_kg_bridge_create(void);
NIMCP_API void hypergraph_kg_bridge_destroy(hypergraph_kg_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPERGRAPH_KG_BRIDGE_H */
