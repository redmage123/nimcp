//=============================================================================
// nimcp_language_parietal_bridge.h - Language-Parietal Spatial Bridge
//=============================================================================
/**
 * @file nimcp_language_parietal_bridge.h
 * @brief Bridge for spatial language, number processing, and visual attention
 *
 * BIOLOGICAL BASIS:
 * - Angular Gyrus (BA39): Word meaning, spatial metaphors, numbers
 * - Supramarginal Gyrus (BA40): Phonological working memory
 * - Intraparietal Sulcus: Number processing, spatial attention
 */

#ifndef NIMCP_LANGUAGE_PARIETAL_BRIDGE_H
#define NIMCP_LANGUAGE_PARIETAL_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/regions/parietal/nimcp_parietal_adapter.h"
#include "language/nimcp_language_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct language_parietal_bridge language_parietal_bridge_t;
typedef struct language_orchestrator language_orchestrator_t;

#define LANGUAGE_PARIETAL_BIO_MODULE_ID 0x0825

typedef struct {
    uint32_t update_interval_ms;
    bool enable_spatial_language;
    bool enable_number_processing;
    bool enable_attention_direction;
    bool enable_bio_async;
} language_parietal_config_t;

typedef enum {
    LP_STATE_IDLE = 0,
    LP_STATE_SPATIAL_PROCESSING,
    LP_STATE_NUMBER_PROCESSING,
    LP_STATE_ATTENTION_ACTIVE,
    LP_STATE_ERROR
} lp_parietal_state_t;

typedef struct {
    uint64_t spatial_queries;
    uint64_t number_conversions;
    uint64_t attention_shifts;
    lp_parietal_state_t state;
} language_parietal_stats_t;

language_parietal_config_t language_parietal_default_config(void);
language_parietal_bridge_t* language_parietal_bridge_create(language_orchestrator_t* language, parietal_adapter_t* parietal, const language_parietal_config_t* config);
void language_parietal_bridge_destroy(language_parietal_bridge_t* bridge);
int language_parietal_bridge_update(language_parietal_bridge_t* bridge, uint64_t timestamp_ms);
int language_parietal_process_spatial_word(language_parietal_bridge_t* bridge, const char* word, float* spatial_vector, uint32_t vec_size);
int language_parietal_number_to_word(language_parietal_bridge_t* bridge, float number, char* word, uint32_t max_len);
int language_parietal_word_to_number(language_parietal_bridge_t* bridge, const char* word, float* number);
int language_parietal_direct_attention(language_parietal_bridge_t* bridge, float x, float y);
int language_parietal_get_stats(const language_parietal_bridge_t* bridge, language_parietal_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LANGUAGE_PARIETAL_BRIDGE_H */
