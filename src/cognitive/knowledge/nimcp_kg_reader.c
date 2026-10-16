/**
 * @file nimcp_kg_reader.c
 * @brief Knowledge Graph Reader Implementation for NIMCP Self-Awareness
 *
 * Parses .aim/memory-nimcp.jsonl and provides runtime access to system's
 * structural self-knowledge.
 *
 * JSONL FORMAT:
 * Each line is a JSON object, either:
 * - Entity: {"type":"entity","name":"...","entityType":"...","observations":[...]}
 * - Relation: {"type":"relation","from":"...","to":"...","relationType":"..."}
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-12-29
 */

#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "constants/nimcp_buffer_constants.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <time.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(kg_reader, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

/**
 * @brief Internal knowledge graph reader structure
 */
struct kg_reader {
    kg_entity_t* entities[KG_MAX_ENTITIES];     /**< Entity storage */
    uint32_t num_entities;                       /**< Number of entities */

    kg_relation_t* relations[KG_MAX_RELATIONS]; /**< Relation storage */
    uint32_t num_relations;                      /**< Number of relations */

    char file_path[NIMCP_METRICS_PATH_SIZE];                         /**< Loaded file path */
    time_t file_mtime;                           /**< File modification time at load */
    uint64_t load_time_us;                       /**< Load time in microseconds */
};

/* Thread-local error message */
static _Thread_local char kg_error_msg[NIMCP_ERROR_BUFFER_SIZE] = {0};

/* ============================================================================
 * HELPER FUNCTIONS
 * ============================================================================ */

/**
 * @brief Set error message
 */
static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(kg_error_msg, sizeof(kg_error_msg), fmt, args);
    va_end(args);
}

/**
 * @brief Skip whitespace in string
 */
static const char* skip_ws(const char* s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

/**
 * @brief Parse JSON string value (handles escape sequences)
 */
static char* parse_json_string(const char** pos) {
    const char* s = *pos;
    s = skip_ws(s);

    if (*s != '"') {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parse_json_string: validation failed");
        return NULL;
    }
    s++;

    /* Find end of string */
    const char* start = s;
    size_t len = 0;
    while (*s && *s != '"') {
        if (*s == '\\' && *(s+1)) {
            s += 2;
            len++;
        } else {
            s++;
            len++;
        }
    }

    if (*s != '"') {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parse_json_string: validation failed");
        return NULL;
    }

    /* Allocate and copy with escape handling */
    char* result = nimcp_malloc(len + 1);
    if (!result) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate result");

        return NULL;

    }

    const char* src = start;
    char* dst = result;
    while (src < s) {
        if (*src == '\\' && *(src+1)) {
            src++;
            switch (*src) {
                case 'n': *dst++ = '\n'; break;
                case 't': *dst++ = '\t'; break;
                case 'r': *dst++ = '\r'; break;
                case '"': *dst++ = '"'; break;
                case '\\': *dst++ = '\\'; break;
                default: *dst++ = *src; break;
            }
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';

    *pos = s + 1;  /* Skip closing quote */
    return result;
}

/**
 * @brief Find key in JSON object and return position after colon
 */
static const char* find_json_key(const char* json, const char* key) {
    char search[KG_MAX_NAME_LENGTH + 4];
    snprintf(search, sizeof(search), "\"%s\"", key);

    const char* pos = strstr(json, search);
    if (!pos) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pos is NULL");

        return NULL;

    }

    pos += strlen(search);
    pos = skip_ws(pos);
    if (*pos != ':') {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_json_key: validation failed");
        return NULL;
    }
    pos++;
    return skip_ws(pos);
}

/**
 * @brief Parse JSON string array
 */
static int parse_json_string_array(const char* json, char** out_strings, uint32_t max_strings, uint32_t* out_count) {
    const char* pos = skip_ws(json);
    if (*pos != '[') {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parse_json_string_array: validation failed");
        return -1;
    }
    pos++;

    uint32_t count = 0;
    while (count < max_strings) {
        pos = skip_ws(pos);
        if (*pos == ']') break;
        if (*pos == ',') { pos++; continue; }

        char* str = parse_json_string(&pos);
        if (!str) break;

        out_strings[count++] = str;
    }

    *out_count = count;
    return 0;
}

/**
 * @brief Parse entity from JSON line
 */
static kg_entity_t* parse_entity(const char* json) {
    kg_entity_t* entity = nimcp_calloc(1, sizeof(kg_entity_t));
    if (!entity) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate entity");

        return NULL;

    }

    /* Parse name */
    const char* name_pos = find_json_key(json, "name");
    if (name_pos) {
        char* name = parse_json_string(&name_pos);
        if (name) {
            strncpy(entity->name, name, KG_MAX_NAME_LENGTH - 1);
            nimcp_free(name);
            name = NULL;
        }
    }

    /* Parse entityType */
    const char* type_pos = find_json_key(json, "entityType");
    if (type_pos) {
        char* type = parse_json_string(&type_pos);
        if (type) {
            strncpy(entity->entity_type, type, KG_MAX_TYPE_LENGTH - 1);
            nimcp_free(type);
            type = NULL;
        }
    }

    /* Parse observations array */
    const char* obs_pos = find_json_key(json, "observations");
    if (obs_pos) {
        parse_json_string_array(obs_pos, entity->observations, KG_MAX_OBSERVATIONS, &entity->num_observations);
    }

    if (entity->name[0] == '\0') {
        /* Invalid entity - no name */
        for (uint32_t i = 0; i < entity->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && entity->num_observations > 256) {
                kg_reader_heartbeat("kg_reader_loop",
                                 (float)(i + 1) / (float)entity->num_observations);
            }

            nimcp_free(entity->observations[i]);
        }
        nimcp_free(entity);
        entity = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parse_entity: validation failed");
        return NULL;
    }

    return entity;
}

/**
 * @brief Parse relation from JSON line
 */
static kg_relation_t* parse_relation(const char* json) {
    kg_relation_t* relation = nimcp_calloc(1, sizeof(kg_relation_t));
    if (!relation) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate relation");

        return NULL;

    }

    /* Parse from */
    const char* from_pos = find_json_key(json, "from");
    if (from_pos) {
        char* from = parse_json_string(&from_pos);
        if (from) {
            strncpy(relation->from, from, KG_MAX_NAME_LENGTH - 1);
            nimcp_free(from);
            from = NULL;
        }
    }

    /* Parse to */
    const char* to_pos = find_json_key(json, "to");
    if (to_pos) {
        char* to = parse_json_string(&to_pos);
        if (to) {
            strncpy(relation->to, to, KG_MAX_NAME_LENGTH - 1);
            nimcp_free(to);
            to = NULL;
        }
    }

    /* Parse relationType */
    const char* type_pos = find_json_key(json, "relationType");
    if (type_pos) {
        char* type = parse_json_string(&type_pos);
        if (type) {
            strncpy(relation->relation_type, type, KG_MAX_TYPE_LENGTH - 1);
            nimcp_free(type);
            type = NULL;
        }
    }

    if (relation->from[0] == '\0' || relation->to[0] == '\0') {
        nimcp_free(relation);
        relation = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parse_relation: validation failed");
        return NULL;
    }

    return relation;
}

/**
 * @brief Determine line type from JSON
 */
static const char* get_line_type(const char* json) {
    /* Check for entity markers */
    if (strstr(json, "\"entityType\"") || strstr(json, "\"observations\"")) {
        return "entity";
    }
    /* Check for relation markers */
    if (strstr(json, "\"relationType\"") && strstr(json, "\"from\"") && strstr(json, "\"to\"")) {
        return "relation";
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "get_line_type: validation failed");
    return NULL;
}

/**
 * @brief Case-insensitive string search
 */
static const char* strcasestr_local(const char* haystack, const char* needle) {
    if (!*needle) return haystack;

    for (; *haystack; haystack++) {
        const char* h = haystack;
        const char* n = needle;

        while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) {
            h++;
            n++;
        }

        if (!*n) return haystack;
    }

    return NULL;  // Not found — normal search result, not an error
}

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

kg_reader_t* kg_reader_create(void) {
    /* Phase 8: Heartbeat at operation start */
    kg_reader_heartbeat("kg_reader_create", 0.0f);


    kg_reader_t* reader = nimcp_calloc(1, sizeof(kg_reader_t));
    NIMCP_API_CHECK_ALLOC(reader, "Failed to allocate KG reader");
    return reader;
}

void kg_reader_destroy(kg_reader_t* reader) {
    if (!reader) return;

    /* Free entities */
    /* Phase 8: Heartbeat at operation start */
    kg_reader_heartbeat("kg_reader_destroy", 0.0f);


    for (uint32_t i = 0; i < reader->num_entities; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && reader->num_entities > 256) {
            kg_reader_heartbeat("kg_reader_loop",
                             (float)(i + 1) / (float)reader->num_entities);
        }

        if (reader->entities[i]) {
            for (uint32_t j = 0; j < reader->entities[i]->num_observations; j++) {
                nimcp_free(reader->entities[i]->observations[j]);
            }
            nimcp_free(reader->entities[i]);
        }
    }

    /* Free relations */
    for (uint32_t i = 0; i < reader->num_relations; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && reader->num_relations > 256) {
            kg_reader_heartbeat("kg_reader_loop",
                             (float)(i + 1) / (float)reader->num_relations);
        }

        nimcp_free(reader->relations[i]);
    }

    nimcp_free(reader);
    reader = NULL;
}

int kg_reader_load(kg_reader_t* reader, const char* file_path) {
    /* Phase 8: Heartbeat at operation start */
    kg_reader_heartbeat("kg_reader_load", 0.0f);


    NIMCP_API_CHECK_NULL(reader, -1, "NULL reader in kg_reader_load");

    const char* path = file_path ? file_path : KG_DEFAULT_PATH;

    FILE* fp = fopen(path, "r");
    if (!fp) {
        set_error("Failed to open KG file: %s", path);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_reader_load: fp is NULL");
        return -1;
    }

    /* Get file modification time */
    struct stat st;
    if (stat(path, &st) == 0) {
        reader->file_mtime = st.st_mtime;
    }

    strncpy(reader->file_path, path, sizeof(reader->file_path) - 1);

    /* Clear existing data */
    for (uint32_t i = 0; i < reader->num_entities; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && reader->num_entities > 256) {
            kg_reader_heartbeat("kg_reader_loop",
                             (float)(i + 1) / (float)reader->num_entities);
        }

        if (reader->entities[i]) {
            for (uint32_t j = 0; j < reader->entities[i]->num_observations; j++) {
                nimcp_free(reader->entities[i]->observations[j]);
            }
            nimcp_free(reader->entities[i]);
            reader->entities[i] = NULL;
        }
    }
    for (uint32_t i = 0; i < reader->num_relations; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && reader->num_relations > 256) {
            kg_reader_heartbeat("kg_reader_loop",
                             (float)(i + 1) / (float)reader->num_relations);
        }

        nimcp_free(reader->relations[i]);
        reader->relations[i] = NULL;
    }
    reader->num_entities = 0;
    reader->num_relations = 0;

    /* Parse JSONL file */
    char line[8192];
    uint64_t start_time = (uint64_t)clock();

    while (fgets(line, sizeof(line), fp)) {
        /* Skip empty lines */
        const char* trimmed = skip_ws(line);
        if (*trimmed == '\0' || *trimmed == '\n') continue;

        const char* line_type = get_line_type(line);

        if (line_type && strcmp(line_type, "entity") == 0) {
            if (reader->num_entities < KG_MAX_ENTITIES) {
                kg_entity_t* entity = parse_entity(line);
                if (entity) {
                    reader->entities[reader->num_entities++] = entity;
                }
            }
        } else if (line_type && strcmp(line_type, "relation") == 0) {
            if (reader->num_relations < KG_MAX_RELATIONS) {
                kg_relation_t* relation = parse_relation(line);
                if (relation) {
                    reader->relations[reader->num_relations++] = relation;
                }
            }
        }
    }

    uint64_t end_time = (uint64_t)clock();
    reader->load_time_us = (end_time - start_time) * 1000000 / CLOCKS_PER_SEC;

    fclose(fp);
    return 0;
}

int kg_reader_reload(kg_reader_t* reader) {
    /* Phase 8: Heartbeat at operation start */
    kg_reader_heartbeat("kg_reader_reload", 0.0f);


    NIMCP_API_CHECK_NULL(reader, -1, "NULL reader in kg_reader_reload");
    NIMCP_API_CHECK(reader->file_path[0] != '\0', -1, "No file loaded to reload");
    return kg_reader_load(reader, reader->file_path);
}

bool kg_reader_is_modified(const kg_reader_t* reader) {
    if (!reader || reader->file_path[0] == '\0') return false;

    /* Phase 8: Heartbeat at operation start */
    kg_reader_heartbeat("kg_reader_is_modified", 0.0f);


    struct stat st;
    if (stat(reader->file_path, &st) != 0) {
        return false;
    }

    return st.st_mtime > reader->file_mtime;
}

/* ============================================================================
 * ENTITY QUERY API
 * ============================================================================ */

const kg_entity_t* kg_reader_get_entity(const kg_reader_t* reader, const char* name) {
    if (!reader || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_reader_get_entity: required parameter is NULL (reader, name)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    kg_reader_heartbeat("kg_reader_get_entity", 0.0f);


    for (uint32_t i = 0; i < reader->num_entities; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && reader->num_entities > 256) {
            kg_reader_heartbeat("kg_reader_loop",
                             (float)(i + 1) / (float)reader->num_entities);
        }

        if (reader->entities[i] && strcmp(reader->entities[i]->name, name) == 0) {
            return reader->entities[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_reader_get_entity: validation failed");
    return NULL;
}

kg_entity_list_t* kg_reader_get_entities_by_type(const kg_reader_t* reader, const char* entity_type) {
    if (!reader || !entity_type) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_reader_get_entities_by_type: required parameter is NULL (reader, entity_type)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    kg_reader_heartbeat("kg_reader_get_entities_by_type", 0.0f);


    kg_entity_list_t* list = nimcp_calloc(1, sizeof(kg_entity_list_t));
    if (!list) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate list");

        return NULL;

    }

    list->capacity = 64;
    list->entities = nimcp_calloc(list->capacity, sizeof(kg_entity_t*));
    if (!list->entities) {
        nimcp_free(list);
        list = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_reader_get_entities_by_type: list->entities is NULL");
        return NULL;
    }

    for (uint32_t i = 0; i < reader->num_entities; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && reader->num_entities > 256) {
            kg_reader_heartbeat("kg_reader_loop",
                             (float)(i + 1) / (float)reader->num_entities);
        }

        if (reader->entities[i] && strcmp(reader->entities[i]->entity_type, entity_type) == 0) {
            if (list->count >= list->capacity) {
                list->capacity *= 2;
                kg_entity_t** new_entities = nimcp_realloc(list->entities, list->capacity * sizeof(kg_entity_t*));
                if (!new_entities) break;
                list->entities = new_entities;
            }
            list->entities[list->count++] = reader->entities[i];
        }
    }

    return list;
}

kg_entity_list_t* kg_reader_get_all_entities(const kg_reader_t* reader) {
    if (!reader) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reader is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    kg_reader_heartbeat("kg_reader_get_all_entities", 0.0f);


    kg_entity_list_t* list = nimcp_calloc(1, sizeof(kg_entity_list_t));
    if (!list) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate list");

        return NULL;

    }

    list->capacity = reader->num_entities;
    list->entities = nimcp_calloc(list->capacity, sizeof(kg_entity_t*));
    if (!list->entities) {
        nimcp_free(list);
        list = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_reader_get_all_entities: list->entities is NULL");
        return NULL;
    }

    for (uint32_t i = 0; i < reader->num_entities; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && reader->num_entities > 256) {
            kg_reader_heartbeat("kg_reader_loop",
                             (float)(i + 1) / (float)reader->num_entities);
        }

        if (reader->entities[i]) {
            list->entities[list->count++] = reader->entities[i];
        }
    }

    return list;
}

kg_entity_list_t* kg_reader_search_entities(const kg_reader_t* reader, const char* search_text) {
    if (!reader || !search_text) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_reader_search_entities: required parameter is NULL (reader, search_text)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    kg_reader_heartbeat("kg_reader_search_entities", 0.0f);


    kg_entity_list_t* list = nimcp_calloc(1, sizeof(kg_entity_list_t));
    if (!list) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate list");

        return NULL;

    }

    list->capacity = 64;
    list->entities = nimcp_calloc(list->capacity, sizeof(kg_entity_t*));
    if (!list->entities) {
        nimcp_free(list);
        list = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_reader_search_entities: list->entities is NULL");
        return NULL;
    }

    for (uint32_t i = 0; i < reader->num_entities; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && reader->num_entities > 256) {
            kg_reader_heartbeat("kg_reader_loop",
                             (float)(i + 1) / (float)reader->num_entities);
        }

        kg_entity_t* e = reader->entities[i];
        if (!e) continue;

        bool found = false;

        /* Search in name */
        if (strcasestr_local(e->name, search_text)) found = true;

        /* Search in type */
        if (!found && strcasestr_local(e->entity_type, search_text)) found = true;

        /* Search in observations */
        if (!found) {
            for (uint32_t j = 0; j < e->num_observations; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && e->num_observations > 256) {
                    kg_reader_heartbeat("kg_reader_loop",
                                     (float)(j + 1) / (float)e->num_observations);
                }

                if (e->observations[j] && strcasestr_local(e->observations[j], search_text)) {
                    found = true;
                    break;
                }
            }
        }

        if (found) {
            if (list->count >= list->capacity) {
                list->capacity *= 2;
                kg_entity_t** new_entities = nimcp_realloc(list->entities, list->capacity * sizeof(kg_entity_t*));
                if (!new_entities) break;
                list->entities = new_entities;
            }
            list->entities[list->count++] = e;
        }
    }

    return list;
}

void kg_entity_list_destroy(kg_entity_list_t* list) {
    if (!list) return;
    /* Phase 8: Heartbeat at operation start */
    kg_reader_heartbeat("kg_reader_kg_entity_list_destr", 0.0f);


    nimcp_free(list->entities);
    nimcp_free(list);
    list = NULL;
}

/* ============================================================================
 * RELATION QUERY API
 * ============================================================================ */

kg_relation_list_t* kg_reader_get_relations_from(const kg_reader_t* reader, const char* from_entity) {
    if (!reader || !from_entity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_reader_get_relations_from: required parameter is NULL (reader, from_entity)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    kg_reader_heartbeat("kg_reader_get_relations_from", 0.0f);


    kg_relation_list_t* list = nimcp_calloc(1, sizeof(kg_relation_list_t));
    if (!list) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate list");

        return NULL;

    }

    list->capacity = 32;
    list->relations = nimcp_calloc(list->capacity, sizeof(kg_relation_t*));
    if (!list->relations) {
        nimcp_free(list);
        list = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_reader_get_relations_from: list->relations is NULL");
        return NULL;
    }

    for (uint32_t i = 0; i < reader->num_relations; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && reader->num_relations > 256) {
            kg_reader_heartbeat("kg_reader_loop",
                             (float)(i + 1) / (float)reader->num_relations);
        }

        if (reader->relations[i] && strcmp(reader->relations[i]->from, from_entity) == 0) {
            if (list->count >= list->capacity) {
                list->capacity *= 2;
                kg_relation_t** new_rels = nimcp_realloc(list->relations, list->capacity * sizeof(kg_relation_t*));
                if (!new_rels) break;
                list->relations = new_rels;
            }
            list->relations[list->count++] = reader->relations[i];
        }
    }

    return list;
}

kg_relation_list_t* kg_reader_get_relations_to(const kg_reader_t* reader, const char* to_entity) {
    if (!reader || !to_entity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_reader_get_relations_to: required parameter is NULL (reader, to_entity)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    kg_reader_heartbeat("kg_reader_get_relations_to", 0.0f);


    kg_relation_list_t* list = nimcp_calloc(1, sizeof(kg_relation_list_t));
    if (!list) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate list");

        return NULL;

    }

    list->capacity = 32;
    list->relations = nimcp_calloc(list->capacity, sizeof(kg_relation_t*));
    if (!list->relations) {
        nimcp_free(list);
        list = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_reader_get_relations_to: list->relations is NULL");
        return NULL;
    }

    for (uint32_t i = 0; i < reader->num_relations; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && reader->num_relations > 256) {
            kg_reader_heartbeat("kg_reader_loop",
                             (float)(i + 1) / (float)reader->num_relations);
        }

        if (reader->relations[i] && strcmp(reader->relations[i]->to, to_entity) == 0) {
            if (list->count >= list->capacity) {
                list->capacity *= 2;
                kg_relation_t** new_rels = nimcp_realloc(list->relations, list->capacity * sizeof(kg_relation_t*));
                if (!new_rels) break;
                list->relations = new_rels;
            }
            list->relations[list->count++] = reader->relations[i];
        }
    }

    return list;
}

kg_relation_list_t* kg_reader_get_relations_by_type(const kg_reader_t* reader, const char* relation_type) {
    if (!reader || !relation_type) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_reader_get_relations_by_type: required parameter is NULL (reader, relation_type)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    kg_reader_heartbeat("kg_reader_get_relations_by_typ", 0.0f);


    kg_relation_list_t* list = nimcp_calloc(1, sizeof(kg_relation_list_t));
    if (!list) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate list");

        return NULL;

    }

    list->capacity = 32;
    list->relations = nimcp_calloc(list->capacity, sizeof(kg_relation_t*));
    if (!list->relations) {
        nimcp_free(list);
        list = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_reader_get_relations_by_type: list->relations is NULL");
        return NULL;
    }

    for (uint32_t i = 0; i < reader->num_relations; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && reader->num_relations > 256) {
            kg_reader_heartbeat("kg_reader_loop",
                             (float)(i + 1) / (float)reader->num_relations);
        }

        if (reader->relations[i] && strcmp(reader->relations[i]->relation_type, relation_type) == 0) {
            if (list->count >= list->capacity) {
                list->capacity *= 2;
                kg_relation_t** new_rels = nimcp_realloc(list->relations, list->capacity * sizeof(kg_relation_t*));
                if (!new_rels) break;
                list->relations = new_rels;
            }
            list->relations[list->count++] = reader->relations[i];
        }
    }

    return list;
}

const char* kg_reader_are_connected(const kg_reader_t* reader, const char* from_entity, const char* to_entity) {
    if (!reader || !from_entity || !to_entity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_reader_are_connected: required parameter is NULL (reader, from_entity, to_entity)");
        return NULL;
    }

    for (uint32_t i = 0; i < reader->num_relations; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && reader->num_relations > 256) {
            kg_reader_heartbeat("kg_reader_loop",
                             (float)(i + 1) / (float)reader->num_relations);
        }

        kg_relation_t* r = reader->relations[i];
        if (r && strcmp(r->from, from_entity) == 0 && strcmp(r->to, to_entity) == 0) {
            return r->relation_type;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_reader_are_connected: validation failed");
    return NULL;
}

void kg_relation_list_destroy(kg_relation_list_t* list) {
    if (!list) return;
    /* Phase 8: Heartbeat at operation start */
    kg_reader_heartbeat("kg_reader_kg_relation_list_des", 0.0f);


    nimcp_free(list->relations);
    nimcp_free(list);
    list = NULL;
}

/* ============================================================================
 * OBSERVATION QUERY API
 * ============================================================================ */

const char* kg_reader_get_observation(const kg_reader_t* reader, const char* entity_name, const char* keyword) {
    const kg_entity_t* entity = kg_reader_get_entity(reader, entity_name);
    if (!entity || !keyword) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_reader_get_observation: required parameter is NULL (entity, keyword)");
        return NULL;
    }

    for (uint32_t i = 0; i < entity->num_observations; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && entity->num_observations > 256) {
            kg_reader_heartbeat("kg_reader_loop",
                             (float)(i + 1) / (float)entity->num_observations);
        }

        if (entity->observations[i] && strcasestr_local(entity->observations[i], keyword)) {
            return entity->observations[i];
        }
    }
    return NULL;  // Observation not found — normal search result
}

const char* const* kg_reader_get_observations(const kg_reader_t* reader, const char* entity_name, uint32_t* out_count) {
    const kg_entity_t* entity = kg_reader_get_entity(reader, entity_name);
    if (!entity) {
        if (out_count) *out_count = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_reader_get_observations: validation failed");
        return NULL;
    }

    if (out_count) *out_count = entity->num_observations;
    return (const char* const*)entity->observations;
}

/* ============================================================================
 * INTROSPECTION API
 * ============================================================================ */

const char** kg_reader_get_module_names(const kg_reader_t* reader, uint32_t* out_count) {
    kg_entity_list_t* modules = kg_reader_get_entities_by_type(reader, "Module");
    if (!modules) {
        if (out_count) *out_count = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_reader_get_module_names: validation failed");
        return NULL;
    }

    const char** names = nimcp_calloc(modules->count, sizeof(char*));
    if (!names) {
        kg_entity_list_destroy(modules);
        if (out_count) *out_count = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_reader_get_module_names: validation failed");
        return NULL;
    }

    for (uint32_t i = 0; i < modules->count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && modules->count > 256) {
            kg_reader_heartbeat("kg_reader_loop",
                             (float)(i + 1) / (float)modules->count);
        }

        names[i] = modules->entities[i]->name;
    }

    if (out_count) *out_count = modules->count;
    kg_entity_list_destroy(modules);
    return names;
}

const char* kg_reader_get_module_capabilities(const kg_reader_t* reader, const char* module_name) {
    const char* cap = kg_reader_get_observation(reader, module_name, "capabilit");
    if (cap) return cap;

    cap = kg_reader_get_observation(reader, module_name, "provides");
    if (cap) return cap;

    cap = kg_reader_get_observation(reader, module_name, "Core");
    return cap;
}

const char* kg_reader_get_module_location(const kg_reader_t* reader, const char* module_name) {
    const char* loc = kg_reader_get_observation(reader, module_name, "Located at");
    if (loc) return loc;

    loc = kg_reader_get_observation(reader, module_name, "include/");
    if (loc) return loc;

    loc = kg_reader_get_observation(reader, module_name, "src/");
    return loc;
}

kg_relation_list_t* kg_reader_get_module_integrations(const kg_reader_t* reader, const char* module_name) {
    /* Phase 8: Heartbeat at operation start */
    kg_reader_heartbeat("kg_reader_get_module_integrati", 0.0f);


    return kg_reader_get_relations_from(reader, module_name);
}

int kg_reader_generate_self_description(const kg_reader_t* reader, char* buffer, size_t buffer_size) {
    if (!reader || !buffer || buffer_size == 0) return 0;

    /* Phase 8: Heartbeat at operation start */
    kg_reader_heartbeat("kg_reader_generate_self_descri", 0.0f);


    int written = 0;
    int n = 0;

    n = snprintf(buffer + written, buffer_size - written,
                 "I am NIMCP (Neuro-Inspired Model of Cognitive Processing).\n\n");
    if (n > 0) written += n;

    /* List modules */
    uint32_t mod_count = 0;
    const char** modules = kg_reader_get_module_names(reader, &mod_count);

    if (modules && mod_count > 0) {
        n = snprintf(buffer + written, buffer_size - written,
                     "I have %u modules:\n", mod_count);
        if (n > 0) written += n;

        for (uint32_t i = 0; i < mod_count && (size_t)written < buffer_size - 100; i++) {
            const char* cap = kg_reader_get_module_capabilities(reader, modules[i]);
            n = snprintf(buffer + written, buffer_size - written,
                         "  - %s: %s\n", modules[i], cap ? cap : "(no description)");
            if (n > 0) written += n;
        }

        nimcp_free((void*)modules);
    }

    /* Count integrations */
    n = snprintf(buffer + written, buffer_size - written,
                 "\nI have %u integration relationships connecting my subsystems.\n",
                 reader->num_relations);
    if (n > 0) written += n;

    return written;
}

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

int kg_reader_get_stats(const kg_reader_t* reader, kg_reader_stats_t* stats) {
    if (!reader || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_reader_get_stats: required parameter is NULL (reader, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    kg_reader_heartbeat("kg_reader_get_stats", 0.0f);


    memset(stats, 0, sizeof(kg_reader_stats_t));
    stats->total_entities = reader->num_entities;
    stats->total_relations = reader->num_relations;
    stats->load_time_us = reader->load_time_us;
    stats->last_reload_time = reader->file_mtime;
    strncpy(stats->file_path, reader->file_path, sizeof(stats->file_path) - 1);

    /* Count total observations */
    for (uint32_t i = 0; i < reader->num_entities; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && reader->num_entities > 256) {
            kg_reader_heartbeat("kg_reader_loop",
                             (float)(i + 1) / (float)reader->num_entities);
        }

        if (reader->entities[i]) {
            stats->total_observations += reader->entities[i]->num_observations;
        }
    }

    return 0;
}

const char* kg_reader_get_last_error(void) {
    return kg_error_msg;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void kg_reader_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_kg_reader_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int kg_reader_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "kg_reader_training_begin: NULL argument");
        return -1;
    }
    kg_reader_heartbeat_instance(NULL, "kg_reader_training_begin", 0.0f);
    (void)(struct kg_reader*)instance; /* Module state available for reset */
    return 0;
}

int kg_reader_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "kg_reader_training_end: NULL argument");
        return -1;
    }
    kg_reader_heartbeat_instance(NULL, "kg_reader_training_end", 1.0f);
    (void)(struct kg_reader*)instance; /* Module state available for finalization */
    return 0;
}

int kg_reader_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "kg_reader_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    kg_reader_heartbeat_instance(NULL, "kg_reader_training_step", progress);
    (void)(struct kg_reader*)instance; /* Module state available for step adaptation */
    return 0;
}
