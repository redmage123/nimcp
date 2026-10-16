/**
 * @file nimcp_gustatory_adapter.h
 * @brief Adapter for Gustatory Cortex integration with NIMCP brain
 * @version Phase 6: Sensory Processing
 * @date 2026-01-12
 */

#ifndef NIMCP_GUSTATORY_ADAPTER_H
#define NIMCP_GUSTATORY_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/regions/gustatory/nimcp_gustatory.h"

typedef struct gust_adapter* gust_adapter_t;

typedef struct {
    gust_config_t gust_config;
    bool enable_bio_async;
    uint32_t message_queue_size;
    bool auto_register_handlers;
    bool enable_kg_registration;
} gust_adapter_config_t;

gust_adapter_config_t gust_adapter_default_config(void);
gust_adapter_t gust_adapter_create(const gust_adapter_config_t* config);
void gust_adapter_destroy(gust_adapter_t adapter);
int gust_adapter_init(gust_adapter_t adapter, void* brain_ctx);
int gust_adapter_start(gust_adapter_t adapter);
int gust_adapter_stop(gust_adapter_t adapter);
int gust_adapter_update(gust_adapter_t adapter, float dt);
int gust_adapter_register_handlers(gust_adapter_t adapter, void* router);
int gust_adapter_connect_hypothalamus(gust_adapter_t adapter, void* hypothalamus);
int gust_adapter_connect_olfactory(gust_adapter_t adapter, void* olfactory);
int gust_adapter_connect_ofc(gust_adapter_t adapter, void* ofc);
int gust_adapter_connect_insula(gust_adapter_t adapter, void* insula);
int gust_adapter_send_taste(gust_adapter_t adapter, const taste_stimulus_t* stimulus);
bool gust_adapter_is_ready(gust_adapter_t adapter);
nimcp_gustatory_t* gust_adapter_get_module(gust_adapter_t adapter);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GUSTATORY_ADAPTER_H */
