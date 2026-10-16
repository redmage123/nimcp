/**
 * @file nimcp_layer_types.c
 * @brief Layer Types Utility Functions Implementation
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Implements utility functions for layer types
 * WHY:  Provides common operations on layer data structures
 * HOW:  Memory management, configuration defaults, message handling
 *
 * @author NIMCP Development Team
 */

#include "integration/core/nimcp_layer_types.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(layer_types)

//=============================================================================
// Layer Name Strings
//=============================================================================

static const char* layer_names[] = {
    "None",             /* 0: NIMCP_LAYER_NONE */
    "Physics",          /* 1: NIMCP_LAYER_PHYSICS */
    "Chemistry",        /* 2: NIMCP_LAYER_CHEMISTRY */
    "Biology",          /* 3: NIMCP_LAYER_BIOLOGY */
    "Neuromodulatory",  /* 4: NIMCP_LAYER_NEUROMODULATORY */
    "Sensory",          /* 5: NIMCP_LAYER_SENSORY */
    "Memory",           /* 6: NIMCP_LAYER_MEMORY */
    "Executive",        /* 7: NIMCP_LAYER_EXECUTIVE */
    "Integration",      /* 8: NIMCP_LAYER_INTEGRATION */
    "Superhuman"        /* 9: NIMCP_LAYER_SUPERHUMAN */
};

//=============================================================================
// Utility Functions
//=============================================================================

const char* nimcp_layer_id_to_string(nimcp_layer_id_t layer_id) {
    if (layer_id >= 0 && layer_id < NIMCP_LAYER_COUNT) {
        return layer_names[layer_id];
    }
    return "Unknown";
}

nimcp_layer_category_t nimcp_layer_get_category(nimcp_layer_id_t layer_id) {
    switch (layer_id) {
        case NIMCP_LAYER_PHYSICS:
        case NIMCP_LAYER_CHEMISTRY:
        case NIMCP_LAYER_BIOLOGY:
            return NIMCP_LAYER_CAT_SUBSTRATE;

        case NIMCP_LAYER_NEUROMODULATORY:
            return NIMCP_LAYER_CAT_MODULATORY;

        case NIMCP_LAYER_SENSORY:
        case NIMCP_LAYER_MEMORY:
        case NIMCP_LAYER_EXECUTIVE:
            return NIMCP_LAYER_CAT_PROCESSING;

        case NIMCP_LAYER_INTEGRATION:
            return NIMCP_LAYER_CAT_BINDING;

        case NIMCP_LAYER_SUPERHUMAN:
            return NIMCP_LAYER_CAT_ENHANCED;

        default:
            return NIMCP_LAYER_CAT_SUBSTRATE;
    }
}

bool nimcp_layers_are_adjacent(nimcp_layer_id_t a, nimcp_layer_id_t b) {
    /* Check if layers are adjacent in the hierarchy */
    int diff = (int)a - (int)b;
    if (diff < 0) diff = -diff;

    /* Direct adjacency */
    if (diff == 1) return true;

    /* Special cases for non-linear connections */
    /* Neuromodulatory connects to Sensory, Memory, and Executive */
    if (a == NIMCP_LAYER_NEUROMODULATORY) {
        return (b == NIMCP_LAYER_SENSORY ||
                b == NIMCP_LAYER_MEMORY ||
                b == NIMCP_LAYER_EXECUTIVE ||
                b == NIMCP_LAYER_BIOLOGY ||
                b == NIMCP_LAYER_SUPERHUMAN);
    }
    if (b == NIMCP_LAYER_NEUROMODULATORY) {
        return (a == NIMCP_LAYER_SENSORY ||
                a == NIMCP_LAYER_MEMORY ||
                a == NIMCP_LAYER_EXECUTIVE ||
                a == NIMCP_LAYER_BIOLOGY ||
                a == NIMCP_LAYER_SUPERHUMAN);
    }

    /* Sensory, Memory, Executive are all connected to Integration */
    if (a == NIMCP_LAYER_INTEGRATION) {
        return (b == NIMCP_LAYER_SENSORY ||
                b == NIMCP_LAYER_MEMORY ||
                b == NIMCP_LAYER_EXECUTIVE ||
                b == NIMCP_LAYER_SUPERHUMAN);
    }
    if (b == NIMCP_LAYER_INTEGRATION) {
        return (a == NIMCP_LAYER_SENSORY ||
                a == NIMCP_LAYER_MEMORY ||
                a == NIMCP_LAYER_EXECUTIVE ||
                a == NIMCP_LAYER_SUPERHUMAN);
    }

    /* Sensory connects to Memory and Executive */
    if (a == NIMCP_LAYER_SENSORY) {
        return (b == NIMCP_LAYER_MEMORY || b == NIMCP_LAYER_EXECUTIVE);
    }
    if (b == NIMCP_LAYER_SENSORY) {
        return (a == NIMCP_LAYER_MEMORY || a == NIMCP_LAYER_EXECUTIVE);
    }

    /* Memory connects to Executive */
    if ((a == NIMCP_LAYER_MEMORY && b == NIMCP_LAYER_EXECUTIVE) ||
        (a == NIMCP_LAYER_EXECUTIVE && b == NIMCP_LAYER_MEMORY)) {
        return true;
    }

    return false;  /* Layers are not adjacent */
}

nimcp_layer_config_t nimcp_layer_default_config(nimcp_layer_id_t layer_id) {
    nimcp_layer_config_t config;
    memset(&config, 0, sizeof(config));

    config.layer_id = layer_id;
    config.category = nimcp_layer_get_category(layer_id);
    config.max_modules = 16;
    config.sync_interval_ms = 10;
    config.enable_logging = false;
    config.enable_metrics = true;

    /* Set layer name */
    const char* name = nimcp_layer_id_to_string(layer_id);
    strncpy(config.name, name, NIMCP_LAYER_NAME_MAX - 1);
    config.name[NIMCP_LAYER_NAME_MAX - 1] = '\0';

    return config;
}

//=============================================================================
// Message Functions
//=============================================================================

static uint64_t get_timestamp_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static uint32_t next_sequence_num = 0;

nimcp_layer_msg_t* nimcp_layer_msg_create(
    uint32_t msg_type,
    nimcp_layer_id_t source,
    nimcp_layer_id_t target,
    void* payload,
    uint32_t payload_size
) {
    nimcp_layer_msg_t* msg = (nimcp_layer_msg_t*)nimcp_calloc(1, sizeof(nimcp_layer_msg_t));
    NIMCP_API_CHECK_ALLOC(msg, "Failed to allocate layer message");

    msg->header.msg_type = msg_type;
    msg->header.source_layer = source;
    msg->header.target_layer = target;
    msg->header.source_module = 0;
    msg->header.target_module = 0;
    msg->header.direction = (source < target) ? NIMCP_MSG_DIR_BOTTOM_UP : NIMCP_MSG_DIR_TOP_DOWN;
    msg->header.priority = NIMCP_MSG_PRIORITY_NORMAL;
    msg->header.timestamp_ns = get_timestamp_ns();
    msg->header.sequence_num = __sync_fetch_and_add(&next_sequence_num, 1);
    msg->header.payload_size = payload_size;

    if (payload && payload_size > 0) {
        msg->payload = nimcp_malloc(payload_size);
        if (!msg->payload) {
            nimcp_free(msg);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_layer_msg_create: msg->payload is NULL");
            return NULL;
        }
        memcpy(msg->payload, payload, payload_size);
        msg->payload_owned = true;
    } else {
        msg->payload = NULL;
        msg->payload_owned = false;
    }

    return msg;
}

void nimcp_layer_msg_destroy(nimcp_layer_msg_t* msg) {
    if (!msg) return;

    if (msg->payload_owned && msg->payload) {
        nimcp_free(msg->payload);
    }
    nimcp_free(msg);
}

nimcp_layer_msg_t* nimcp_layer_msg_clone(const nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL_RET_NULL(msg, "Source message is NULL in clone");

    nimcp_layer_msg_t* clone = (nimcp_layer_msg_t*)nimcp_calloc(1, sizeof(nimcp_layer_msg_t));
    NIMCP_API_CHECK_ALLOC(clone, "Failed to allocate cloned layer message");

    /* Copy header */
    clone->header = msg->header;
    clone->header.timestamp_ns = get_timestamp_ns();
    clone->header.sequence_num = __sync_fetch_and_add(&next_sequence_num, 1);

    /* Clone payload if present */
    if (msg->payload && msg->header.payload_size > 0) {
        clone->payload = nimcp_malloc(msg->header.payload_size);
        if (!clone->payload) {
            nimcp_free(clone);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_layer_msg_clone: clone->payload is NULL");
            return NULL;
        }
        memcpy(clone->payload, msg->payload, msg->header.payload_size);
        clone->payload_owned = true;
    } else {
        clone->payload = NULL;
        clone->payload_owned = false;
    }

    return clone;
}
