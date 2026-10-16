/**
 * @file nimcp_olfactory_adapter.h
 * @brief Adapter for Olfactory Cortex integration with NIMCP brain
 * @version Phase 6: Sensory Processing
 * @date 2026-01-12
 */

#ifndef NIMCP_OLFACTORY_ADAPTER_H
#define NIMCP_OLFACTORY_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/regions/olfactory/nimcp_olfactory.h"

typedef struct olfact_adapter* olfact_adapter_t;

typedef struct {
    olfact_config_t olfact_config;
    bool enable_bio_async;
    uint32_t message_queue_size;
    bool auto_register_handlers;
    bool enable_kg_registration;
} olfact_adapter_config_t;

olfact_adapter_config_t olfact_adapter_default_config(void);
olfact_adapter_t olfact_adapter_create(const olfact_adapter_config_t* config);
void olfact_adapter_destroy(olfact_adapter_t adapter);
int olfact_adapter_init(olfact_adapter_t adapter, void* brain_ctx);
int olfact_adapter_start(olfact_adapter_t adapter);
int olfact_adapter_stop(olfact_adapter_t adapter);
int olfact_adapter_update(olfact_adapter_t adapter, float dt);
int olfact_adapter_register_handlers(olfact_adapter_t adapter, void* router);
int olfact_adapter_connect_amygdala(olfact_adapter_t adapter, void* amygdala);
int olfact_adapter_connect_entorhinal(olfact_adapter_t adapter, void* entorhinal);
int olfact_adapter_connect_ofc(olfact_adapter_t adapter, void* ofc);
int olfact_adapter_send_odor(olfact_adapter_t adapter, const float* pattern, uint32_t dim, float concentration);
bool olfact_adapter_is_ready(olfact_adapter_t adapter);
nimcp_olfactory_t* olfact_adapter_get_module(olfact_adapter_t adapter);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OLFACTORY_ADAPTER_H */
