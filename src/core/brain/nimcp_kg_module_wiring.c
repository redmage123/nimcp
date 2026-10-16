/**
 * @file nimcp_kg_module_wiring.c
 * @brief Module Wiring Descriptor for Knowledge Graph Self-Assembly - Implementation
 * @version 1.0.0
 * @date 2025-01-16
 *
 * Implementation of module wiring descriptors that enable brain self-awareness
 * of module topology, connections, inputs/outputs, and weights.
 */

#include "core/brain/nimcp_kg_module_wiring.h"
#include "core/brain/nimcp_kg_wiring_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <stdio.h>

/* Always include memory header for nimcp_malloc/calloc/free declarations */
#include "utils/memory/nimcp_memory.h"

/* Conditional logging support */
#ifdef NIMCP_LOGGING_ENABLED
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(kg_module_wiring, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define WIRING_LOG_DEBUG(fmt, ...) LOG_DEBUG("[KG_WIRING] " fmt, ##__VA_ARGS__)
#define WIRING_LOG_INFO(fmt, ...)  LOG_INFO("[KG_WIRING] " fmt, ##__VA_ARGS__)
#define WIRING_LOG_WARN(fmt, ...)  LOG_WARN("[KG_WIRING] " fmt, ##__VA_ARGS__)
#define WIRING_LOG_ERROR(fmt, ...) LOG_ERROR("[KG_WIRING] " fmt, ##__VA_ARGS__)
#else
#define WIRING_LOG_DEBUG(fmt, ...) ((void)0)
#define WIRING_LOG_INFO(fmt, ...)  ((void)0)
#define WIRING_LOG_WARN(fmt, ...)  ((void)0)
#define WIRING_LOG_ERROR(fmt, ...) ((void)0)
#endif

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in milliseconds since epoch
 */
static uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
    }
    return 0;
}

/**
 * @brief Safe string copy with null termination
 */
static void safe_strcpy(char* dest, const char* src, size_t dest_size) {
    if (!dest || dest_size == 0) {
        return;
    }
    if (!src) {
        dest[0] = '\0';
        return;
    }
    size_t src_len = strlen(src);
    size_t copy_len = (src_len < dest_size - 1) ? src_len : dest_size - 1;
    memcpy(dest, src, copy_len);
    dest[copy_len] = '\0';
}

/**
 * @brief Case-insensitive string comparison
 */
static int strcasecmp_local(const char* s1, const char* s2) {
    if (!s1 || !s2) {
        return s1 == s2 ? 0 : (s1 ? 1 : -1);
    }
    while (*s1 && *s2) {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);
        if (c1 != c2) {
            return c1 - c2;
        }
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

kg_module_wiring_t* kg_module_wiring_create(const char* name, const char* type) {
    if (!name || !type) {
        WIRING_LOG_ERROR("create: NULL parameters (name=%p, type=%p)",
                         (const void*)name, (const void*)type);
        NIMCP_THROW_KG_WIRING_NULL(NULL, "create", name ? "type" : "name");
        return NULL;
    }

    /* Validate name length */
    if (strlen(name) == 0 || strlen(name) >= KG_WIRING_MAX_NAME_LEN) {
        WIRING_LOG_ERROR("create: invalid name length (len=%zu, max=%d)",
                         strlen(name), KG_WIRING_MAX_NAME_LEN);
        NIMCP_THROW_KG_WIRING(NIMCP_ERROR_KG_WIRING_INVALID_NAME, name, "create",
                              "Invalid name length: %zu (max %d)", strlen(name), KG_WIRING_MAX_NAME_LEN);
        return NULL;
    }

    /* Validate type length */
    if (strlen(type) == 0 || strlen(type) >= KG_WIRING_MAX_TYPE_LEN) {
        WIRING_LOG_ERROR("create: invalid type length (len=%zu, max=%d)",
                         strlen(type), KG_WIRING_MAX_TYPE_LEN);
        NIMCP_THROW_KG_WIRING(NIMCP_ERROR_KG_WIRING_INVALID_TYPE, name, "create",
                              "Invalid type length: %zu (max %d)", strlen(type), KG_WIRING_MAX_TYPE_LEN);
        return NULL;
    }

    kg_module_wiring_t* wiring = nimcp_calloc(1, sizeof(kg_module_wiring_t));
    if (!wiring) {
        WIRING_LOG_ERROR("create: allocation failed for module '%s'", name);
        NIMCP_THROW_KG_WIRING(NIMCP_ERROR_KG_WIRING_CREATE, name, "create",
                              "Memory allocation failed for wiring descriptor");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wiring is NULL");

        return NULL;
    }

    /* Set module identification */
    safe_strcpy(wiring->module_name, name, KG_WIRING_MAX_NAME_LEN);
    safe_strcpy(wiring->module_type, type, KG_WIRING_MAX_TYPE_LEN);

    /* Initialize defaults */
    wiring->target_layer = 0;  /* Unspecified */
    wiring->hemisphere_affinity = 0;  /* Unspecified / bilateral */

    /* Initialize counts */
    wiring->input_count = 0;
    wiring->output_count = 0;
    wiring->handler_count = 0;

    /* Initialize weights */
    wiring->network_type = KG_WEIGHT_NONE;
    wiring->initial_weights = NULL;
    wiring->weights_size = 0;

    /* Initialize metadata */
    memset(&wiring->metadata, 0, sizeof(kg_module_metadata_t));

    /* Set creation timestamp */
    wiring->creation_timestamp = get_timestamp_ms();
    wiring->version = 1;

    WIRING_LOG_INFO("created wiring descriptor: module='%s' type='%s'", name, type);
    return wiring;
}

void kg_module_wiring_destroy(kg_module_wiring_t* wiring) {
    if (!wiring) {
        return;
    }

    WIRING_LOG_DEBUG("destroying wiring descriptor: module='%s'", wiring->module_name);

    /* Free copied weight data if present */
    if (wiring->initial_weights) {
        nimcp_free(wiring->initial_weights);
        wiring->initial_weights = NULL;
    }

    nimcp_free(wiring);
}

/* ============================================================================
 * Input/Output Registration API Implementation
 * ============================================================================ */

int kg_module_wiring_add_input(
    kg_module_wiring_t* w,
    const char* source,
    const char* msg_type,
    bool required
) {
    if (!w || !source || !msg_type) {
        WIRING_LOG_ERROR("add_input: NULL parameters");
        NIMCP_THROW_KG_WIRING_NULL(w ? w->module_name : NULL, "add_input",
                                   !w ? "wiring" : (!source ? "source" : "msg_type"));
        return -1;
    }

    /* Check if array is full */
    if (w->input_count >= KG_WIRING_MAX_INPUTS) {
        WIRING_LOG_WARN("add_input: module '%s' reached max inputs (%d)",
                        w->module_name, KG_WIRING_MAX_INPUTS);
        NIMCP_THROW_KG_WIRING_CAPACITY(NIMCP_ERROR_KG_WIRING_INPUTS_FULL,
                                       w->module_name, "add_input", KG_WIRING_MAX_INPUTS);
        return -1;
    }

    /* Validate string lengths */
    if (strlen(source) >= KG_WIRING_MAX_NAME_LEN ||
        strlen(msg_type) >= KG_WIRING_MAX_MSG_TYPE_LEN) {
        WIRING_LOG_ERROR("add_input: string too long (source='%s', msg_type='%s')",
                         source, msg_type);
        NIMCP_THROW_KG_WIRING_STRING(NIMCP_ERROR_KG_WIRING_STRING_TOO_LONG,
                                     w->module_name, "add_input",
                                     strlen(source) >= KG_WIRING_MAX_NAME_LEN ? strlen(source) : strlen(msg_type),
                                     strlen(source) >= KG_WIRING_MAX_NAME_LEN ? KG_WIRING_MAX_NAME_LEN : KG_WIRING_MAX_MSG_TYPE_LEN);
        return -1;
    }

    /* Check for duplicate */
    for (uint32_t i = 0; i < w->input_count; i++) {
        if (strcmp(w->inputs[i].source_module, source) == 0 &&
            strcmp(w->inputs[i].message_type, msg_type) == 0) {
            /* Already exists - update required flag */
            w->inputs[i].required = required;
            return 0;
        }
    }

    /* Add new input */
    kg_input_connection_t* input = &w->inputs[w->input_count];
    safe_strcpy(input->source_module, source, KG_WIRING_MAX_NAME_LEN);
    safe_strcpy(input->message_type, msg_type, KG_WIRING_MAX_MSG_TYPE_LEN);
    input->required = required;

    w->input_count++;
    WIRING_LOG_DEBUG("module '%s': added input from '%s' (type=%s, required=%s)",
                     w->module_name, source, msg_type, required ? "yes" : "no");
    return 0;
}

int kg_module_wiring_add_output(
    kg_module_wiring_t* w,
    const char* msg_type,
    const char* description
) {
    if (!w || !msg_type) {
        WIRING_LOG_ERROR("add_output: NULL parameters");
        NIMCP_THROW_KG_WIRING_NULL(w ? w->module_name : NULL, "add_output",
                                   !w ? "wiring" : "msg_type");
        return -1;
    }

    /* Check if array is full */
    if (w->output_count >= KG_WIRING_MAX_OUTPUTS) {
        WIRING_LOG_WARN("add_output: module '%s' reached max outputs (%d)",
                        w->module_name, KG_WIRING_MAX_OUTPUTS);
        NIMCP_THROW_KG_WIRING_CAPACITY(NIMCP_ERROR_KG_WIRING_OUTPUTS_FULL,
                                       w->module_name, "add_output", KG_WIRING_MAX_OUTPUTS);
        return -1;
    }

    /* Validate string lengths */
    if (strlen(msg_type) >= KG_WIRING_MAX_MSG_TYPE_LEN) {
        WIRING_LOG_ERROR("add_output: msg_type too long ('%s')", msg_type);
        NIMCP_THROW_KG_WIRING_STRING(NIMCP_ERROR_KG_WIRING_STRING_TOO_LONG,
                                     w->module_name, "add_output",
                                     strlen(msg_type), KG_WIRING_MAX_MSG_TYPE_LEN);
        return -1;
    }

    /* Check for duplicate */
    for (uint32_t i = 0; i < w->output_count; i++) {
        if (strcmp(w->outputs[i].message_type, msg_type) == 0) {
            /* Already exists - update description */
            if (description) {
                safe_strcpy(w->outputs[i].description, description, KG_WIRING_MAX_DESC_LEN);
            }
            return 0;
        }
    }

    /* Add new output */
    kg_output_connection_t* output = &w->outputs[w->output_count];
    safe_strcpy(output->message_type, msg_type, KG_WIRING_MAX_MSG_TYPE_LEN);
    if (description) {
        safe_strcpy(output->description, description, KG_WIRING_MAX_DESC_LEN);
    } else {
        output->description[0] = '\0';
    }

    w->output_count++;
    WIRING_LOG_DEBUG("module '%s': added output type=%s", w->module_name, msg_type);
    return 0;
}

int kg_module_wiring_add_handler(
    kg_module_wiring_t* w,
    const char* msg_type,
    uint32_t priority
) {
    if (!w || !msg_type) {
        WIRING_LOG_ERROR("add_handler: NULL parameters");
        NIMCP_THROW_KG_WIRING_NULL(w ? w->module_name : NULL, "add_handler",
                                   !w ? "wiring" : "msg_type");
        return -1;
    }

    /* Check if array is full */
    if (w->handler_count >= KG_WIRING_MAX_HANDLERS) {
        WIRING_LOG_WARN("add_handler: module '%s' reached max handlers (%d)",
                        w->module_name, KG_WIRING_MAX_HANDLERS);
        NIMCP_THROW_KG_WIRING_CAPACITY(NIMCP_ERROR_KG_WIRING_HANDLERS_FULL,
                                       w->module_name, "add_handler", KG_WIRING_MAX_HANDLERS);
        return -1;
    }

    /* Validate string length */
    if (strlen(msg_type) >= KG_WIRING_MAX_MSG_TYPE_LEN) {
        WIRING_LOG_ERROR("add_handler: msg_type too long ('%s')", msg_type);
        NIMCP_THROW_KG_WIRING_STRING(NIMCP_ERROR_KG_WIRING_STRING_TOO_LONG,
                                     w->module_name, "add_handler",
                                     strlen(msg_type), KG_WIRING_MAX_MSG_TYPE_LEN);
        return -1;
    }

    /* Check for duplicate - update priority if exists */
    for (uint32_t i = 0; i < w->handler_count; i++) {
        if (strcmp(w->handlers[i].message_type, msg_type) == 0) {
            w->handlers[i].priority = priority;
            return 0;
        }
    }

    /* Add new handler */
    kg_handler_registration_t* handler = &w->handlers[w->handler_count];
    safe_strcpy(handler->message_type, msg_type, KG_WIRING_MAX_MSG_TYPE_LEN);
    handler->priority = priority;

    w->handler_count++;
    WIRING_LOG_DEBUG("module '%s': added handler type=%s priority=%u",
                     w->module_name, msg_type, priority);
    return 0;
}

/* ============================================================================
 * Weight State API Implementation
 * ============================================================================ */

int kg_module_wiring_set_weights(
    kg_module_wiring_t* w,
    kg_weight_type_t type,
    void* weights,
    size_t size
) {
    if (!w) {
        NIMCP_THROW_KG_WIRING_NULL(NULL, "set_weights", "wiring");
        return -1;
    }

    /* Free existing weight data */
    if (w->initial_weights) {
        nimcp_free(w->initial_weights);
        w->initial_weights = NULL;
        w->weights_size = 0;
    }

    w->network_type = type;

    /* Validate: if weights is NULL, size must be 0 */
    if (!weights && size > 0) {
        NIMCP_THROW_KG_WIRING(NIMCP_ERROR_KG_WIRING_WEIGHT_INVALID, w->module_name, "set_weights",
                              "NULL weights pointer with non-zero size (%zu)", size);
        return -1;  /* Invalid: NULL data with non-zero size */
    }

    /* If no weights provided, just set type */
    if (!weights || size == 0) {
        return 0;
    }

    /* Copy weight data */
    w->initial_weights = nimcp_malloc(size);
    if (!w->initial_weights) {
        NIMCP_THROW_KG_WIRING(NIMCP_ERROR_KG_WIRING_WEIGHT_ALLOC, w->module_name, "set_weights",
                              "Failed to allocate %zu bytes for weights", size);
        return -1;
    }

    memcpy(w->initial_weights, weights, size);
    w->weights_size = size;

    return 0;
}

/* ============================================================================
 * Metadata API Implementation
 * ============================================================================ */

int kg_module_wiring_set_metadata(
    kg_module_wiring_t* w,
    const char* author,
    const char* category,
    const char* description
) {
    if (!w) {
        NIMCP_THROW_KG_WIRING_NULL(NULL, "set_metadata", "wiring");
        return -1;
    }

    if (author) {
        safe_strcpy(w->metadata.author, author, KG_WIRING_MAX_NAME_LEN);
    }

    if (category) {
        safe_strcpy(w->metadata.category, category, KG_WIRING_MAX_TYPE_LEN);
    }

    if (description) {
        safe_strcpy(w->metadata.description, description, KG_WIRING_MAX_DESC_LEN);
    }

    return 0;
}

int kg_module_wiring_add_metadata_entry(
    kg_module_wiring_t* w,
    const char* key,
    const char* value
) {
    if (!w || !key || !value) {
        NIMCP_THROW_KG_WIRING_NULL(w ? w->module_name : NULL, "add_metadata_entry",
                                   !w ? "wiring" : (!key ? "key" : "value"));
        return -1;
    }

    /* Check if array is full */
    if (w->metadata.entry_count >= KG_WIRING_MAX_METADATA) {
        NIMCP_THROW_KG_WIRING_CAPACITY(NIMCP_ERROR_KG_WIRING_METADATA_FULL,
                                       w->module_name, "add_metadata_entry", KG_WIRING_MAX_METADATA);
        return -1;
    }

    /* Validate string lengths */
    if (strlen(key) >= KG_WIRING_MAX_META_KEY_LEN ||
        strlen(value) >= KG_WIRING_MAX_META_VALUE_LEN) {
        NIMCP_THROW_KG_WIRING_STRING(NIMCP_ERROR_KG_WIRING_STRING_TOO_LONG,
                                     w->module_name, "add_metadata_entry",
                                     strlen(key) >= KG_WIRING_MAX_META_KEY_LEN ? strlen(key) : strlen(value),
                                     strlen(key) >= KG_WIRING_MAX_META_KEY_LEN ? KG_WIRING_MAX_META_KEY_LEN : KG_WIRING_MAX_META_VALUE_LEN);
        return -1;
    }

    /* Check for existing key - update value if found */
    for (uint32_t i = 0; i < w->metadata.entry_count; i++) {
        if (strcmp(w->metadata.entries[i].key, key) == 0) {
            safe_strcpy(w->metadata.entries[i].value, value, KG_WIRING_MAX_META_VALUE_LEN);
            return 0;
        }
    }

    /* Add new entry */
    kg_wiring_metadata_entry_t* entry = &w->metadata.entries[w->metadata.entry_count];
    safe_strcpy(entry->key, key, KG_WIRING_MAX_META_KEY_LEN);
    safe_strcpy(entry->value, value, KG_WIRING_MAX_META_VALUE_LEN);

    w->metadata.entry_count++;
    return 0;
}

int kg_module_wiring_set_version(
    kg_module_wiring_t* w,
    uint32_t major,
    uint32_t minor,
    uint32_t patch
) {
    if (!w) {
        NIMCP_THROW_KG_WIRING_NULL(NULL, "set_version", "wiring");
        return -1;
    }

    w->metadata.version_major = major;
    w->metadata.version_minor = minor;
    w->metadata.version_patch = patch;

    /* Encode into version field: major.minor.patch */
    w->version = ((uint64_t)major << 32) | ((uint64_t)minor << 16) | (uint64_t)patch;

    return 0;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

bool kg_module_wiring_has_input(
    const kg_module_wiring_t* w,
    const char* source,
    const char* msg_type
) {
    if (!w || !source) {
        return false;
    }

    for (uint32_t i = 0; i < w->input_count; i++) {
        if (strcmp(w->inputs[i].source_module, source) == 0) {
            /* Source matches - check message type if specified */
            if (!msg_type || strcmp(w->inputs[i].message_type, msg_type) == 0) {
                return true;
            }
        }
    }

    return false;  /* Not found is normal behavior, not an error */
}

bool kg_module_wiring_has_output(
    const kg_module_wiring_t* w,
    const char* msg_type
) {
    if (!w || !msg_type) {
        return false;
    }

    for (uint32_t i = 0; i < w->output_count; i++) {
        if (strcmp(w->outputs[i].message_type, msg_type) == 0) {
            return true;
        }
    }

    return false;  /* Not found is normal behavior, not an error */
}

bool kg_module_wiring_has_handler(
    const kg_module_wiring_t* w,
    const char* msg_type
) {
    if (!w || !msg_type) {
        return false;
    }

    for (uint32_t i = 0; i < w->handler_count; i++) {
        if (strcmp(w->handlers[i].message_type, msg_type) == 0) {
            return true;
        }
    }

    return false;  /* Not found is normal behavior, not an error */
}

uint32_t kg_module_wiring_get_handler_priority(
    const kg_module_wiring_t* w,
    const char* msg_type
) {
    if (!w || !msg_type) {
        return 0;
    }

    for (uint32_t i = 0; i < w->handler_count; i++) {
        if (strcmp(w->handlers[i].message_type, msg_type) == 0) {
            return w->handlers[i].priority;
        }
    }

    return 0;
}

/* ============================================================================
 * Validation API Implementation
 * ============================================================================ */

int kg_module_wiring_validate(
    const kg_module_wiring_t* w,
    char* error_buf,
    size_t error_buf_size
) {
    if (!w) {
        WIRING_LOG_ERROR("validate: wiring descriptor is NULL");
        if (error_buf && error_buf_size > 0) {
            safe_strcpy(error_buf, "Wiring descriptor is NULL", error_buf_size);
        }
        NIMCP_THROW_KG_WIRING_NULL(NULL, "validate", "wiring");
        return -1;
    }

    /* Validate module name */
    if (strlen(w->module_name) == 0) {
        WIRING_LOG_ERROR("validate: module name is empty");
        if (error_buf && error_buf_size > 0) {
            safe_strcpy(error_buf, "Module name is empty", error_buf_size);
        }
        NIMCP_THROW_KG_WIRING_VALIDATION("unknown", "Module name is empty");
        return -1;
    }

    /* Validate module type */
    if (strlen(w->module_type) == 0) {
        WIRING_LOG_ERROR("validate: module '%s' has empty type", w->module_name);
        if (error_buf && error_buf_size > 0) {
            safe_strcpy(error_buf, "Module type is empty", error_buf_size);
        }
        NIMCP_THROW_KG_WIRING_VALIDATION(w->module_name, "Module type is empty");
        return -1;
    }

    /* Check for duplicate handlers */
    for (uint32_t i = 0; i < w->handler_count; i++) {
        for (uint32_t j = i + 1; j < w->handler_count; j++) {
            if (strcmp(w->handlers[i].message_type, w->handlers[j].message_type) == 0) {
                WIRING_LOG_ERROR("validate: module '%s' has duplicate handler for '%s'",
                                 w->module_name, w->handlers[i].message_type);
                if (error_buf && error_buf_size > 0) {
                    snprintf(error_buf, error_buf_size,
                             "Duplicate handler for message type: %s",
                             w->handlers[i].message_type);
                }
                NIMCP_THROW_KG_WIRING(NIMCP_ERROR_KG_WIRING_DUPLICATE, w->module_name, "validate",
                                      "Duplicate handler for: %s", w->handlers[i].message_type);
                return -1;
            }
        }
    }

    /* Check for duplicate outputs */
    for (uint32_t i = 0; i < w->output_count; i++) {
        for (uint32_t j = i + 1; j < w->output_count; j++) {
            if (strcmp(w->outputs[i].message_type, w->outputs[j].message_type) == 0) {
                WIRING_LOG_ERROR("validate: module '%s' has duplicate output '%s'",
                                 w->module_name, w->outputs[i].message_type);
                if (error_buf && error_buf_size > 0) {
                    snprintf(error_buf, error_buf_size,
                             "Duplicate output message type: %s",
                             w->outputs[i].message_type);
                }
                NIMCP_THROW_KG_WIRING(NIMCP_ERROR_KG_WIRING_DUPLICATE, w->module_name, "validate",
                                      "Duplicate output: %s", w->outputs[i].message_type);
                return -1;
            }
        }
    }

    /* Check for duplicate inputs from same source with same message type */
    for (uint32_t i = 0; i < w->input_count; i++) {
        for (uint32_t j = i + 1; j < w->input_count; j++) {
            if (strcmp(w->inputs[i].source_module, w->inputs[j].source_module) == 0 &&
                strcmp(w->inputs[i].message_type, w->inputs[j].message_type) == 0) {
                WIRING_LOG_ERROR("validate: module '%s' has duplicate input from '%s' type '%s'",
                                 w->module_name, w->inputs[i].source_module,
                                 w->inputs[i].message_type);
                if (error_buf && error_buf_size > 0) {
                    snprintf(error_buf, error_buf_size,
                             "Duplicate input from %s with type %s",
                             w->inputs[i].source_module,
                             w->inputs[i].message_type);
                }
                NIMCP_THROW_KG_WIRING(NIMCP_ERROR_KG_WIRING_DUPLICATE, w->module_name, "validate",
                                      "Duplicate input from %s with type %s",
                                      w->inputs[i].source_module, w->inputs[i].message_type);
                return -1;
            }
        }
    }

    /* Validate weight data consistency */
    if (w->network_type != KG_WEIGHT_NONE) {
        if (w->initial_weights && w->weights_size == 0) {
            WIRING_LOG_ERROR("validate: module '%s' has weights pointer but size=0",
                             w->module_name);
            if (error_buf && error_buf_size > 0) {
                safe_strcpy(error_buf, "Weights pointer set but size is zero", error_buf_size);
            }
            return -1;
        }
        if (!w->initial_weights && w->weights_size > 0) {
            WIRING_LOG_ERROR("validate: module '%s' has weights size=%zu but NULL pointer",
                             w->module_name, w->weights_size);
            if (error_buf && error_buf_size > 0) {
                safe_strcpy(error_buf, "Weights size set but pointer is NULL", error_buf_size);
            }
            return -1;
        }
    }

    /* Validate metadata entries */
    for (uint32_t i = 0; i < w->metadata.entry_count; i++) {
        if (strlen(w->metadata.entries[i].key) == 0) {
            WIRING_LOG_ERROR("validate: module '%s' metadata entry %u has empty key",
                             w->module_name, i);
            if (error_buf && error_buf_size > 0) {
                snprintf(error_buf, error_buf_size,
                         "Metadata entry %u has empty key", i);
            }
            return -1;
        }
    }

    /* Check for duplicate metadata keys */
    for (uint32_t i = 0; i < w->metadata.entry_count; i++) {
        for (uint32_t j = i + 1; j < w->metadata.entry_count; j++) {
            if (strcmp(w->metadata.entries[i].key, w->metadata.entries[j].key) == 0) {
                WIRING_LOG_ERROR("validate: module '%s' has duplicate metadata key '%s'",
                                 w->module_name, w->metadata.entries[i].key);
                if (error_buf && error_buf_size > 0) {
                    snprintf(error_buf, error_buf_size,
                             "Duplicate metadata key: %s",
                             w->metadata.entries[i].key);
                }
                return -1;
            }
        }
    }

    WIRING_LOG_DEBUG("validate: module '%s' passed validation (%u inputs, %u outputs, %u handlers)",
                     w->module_name, w->input_count, w->output_count, w->handler_count);
    return 0;
}

/* ============================================================================
 * String Conversion Utilities Implementation
 * ============================================================================ */

const char* kg_weight_type_to_string(kg_weight_type_t type) {
    switch (type) {
        case KG_WEIGHT_NONE:
            return "NONE";
        case KG_WEIGHT_SNN:
            return "SNN";
        case KG_WEIGHT_LNN:
            return "LNN";
        case KG_WEIGHT_CNN:
            return "CNN";
        case KG_WEIGHT_RNN:
            return "RNN";
        case KG_WEIGHT_TRANSFORMER:
            return "TRANSFORMER";
        case KG_WEIGHT_GNN:
            return "GNN";
        case KG_WEIGHT_HYBRID:
            return "HYBRID";
        case KG_WEIGHT_CUSTOM:
            return "CUSTOM";
        default:
            return "UNKNOWN";
    }
}

kg_weight_type_t kg_weight_type_from_string(const char* str) {
    if (!str) {
        return KG_WEIGHT_NONE;
    }

    if (strcasecmp_local(str, "NONE") == 0) {
        return KG_WEIGHT_NONE;
    }
    if (strcasecmp_local(str, "SNN") == 0) {
        return KG_WEIGHT_SNN;
    }
    if (strcasecmp_local(str, "LNN") == 0) {
        return KG_WEIGHT_LNN;
    }
    if (strcasecmp_local(str, "CNN") == 0) {
        return KG_WEIGHT_CNN;
    }
    if (strcasecmp_local(str, "RNN") == 0) {
        return KG_WEIGHT_RNN;
    }
    if (strcasecmp_local(str, "TRANSFORMER") == 0) {
        return KG_WEIGHT_TRANSFORMER;
    }
    if (strcasecmp_local(str, "GNN") == 0) {
        return KG_WEIGHT_GNN;
    }
    if (strcasecmp_local(str, "HYBRID") == 0) {
        return KG_WEIGHT_HYBRID;
    }
    if (strcasecmp_local(str, "CUSTOM") == 0) {
        return KG_WEIGHT_CUSTOM;
    }

    return KG_WEIGHT_NONE;
}
