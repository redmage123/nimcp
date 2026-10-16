/**
 * @file nimcp_discourse_manager.c
 * @brief Discourse manager implementation
 *
 * WHAT: Conversation tracking, anaphora resolution, and topic management
 * WHY:  Enable coherent multi-turn conversations
 * HOW:  Maintain discourse model with referents, topics, and turns
 *
 * @version Phase B4: Speech Enhancement
 * @date 2026-01-15
 */

#include "core/brain/regions/broca/nimcp_discourse_manager.h"
#include "cognitive/language/nimcp_w12_language_kg_events.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(discourse_manager, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

struct discourse_manager {
    discourse_config_t config;
    discourse_status_t status;
    discourse_error_t last_error;
    discourse_stats_t stats;

    /* Referents */
    discourse_referent_t* referents;
    uint32_t referent_count;
    uint32_t next_referent_id;

    /* Topics */
    discourse_topic_t* topics;
    uint32_t topic_count;
    uint32_t next_topic_id;
    uint32_t current_topic_id;

    /* Turns */
    discourse_turn_t* turns;
    uint32_t turn_count;
    uint32_t turn_head;
    uint32_t next_turn_id;

    /* Bio-async */
    bio_router_t* router;
    bool bio_registered;
};

/*=============================================================================
 * PRONOUN PATTERNS
 *===========================================================================*/

typedef struct {
    const char* pronoun;
    uint8_t gender;   /* 0=any, 1=masc, 2=fem, 3=neut */
    uint8_t number;   /* 0=any, 1=sing, 2=plur */
    uint8_t person;   /* 1, 2, or 3 */
    bool is_animate;  /* Typically animate */
} pronoun_info_t;

static const pronoun_info_t PRONOUNS[] = {
    {"he", 1, 1, 3, true},
    {"him", 1, 1, 3, true},
    {"his", 1, 1, 3, true},
    {"she", 2, 1, 3, true},
    {"her", 2, 1, 3, true},
    {"hers", 2, 1, 3, true},
    {"it", 3, 1, 3, false},
    {"its", 3, 1, 3, false},
    {"they", 0, 2, 3, true},
    {"them", 0, 2, 3, true},
    {"their", 0, 2, 3, true},
    {"this", 0, 1, 3, false},
    {"that", 0, 1, 3, false},
    {"these", 0, 2, 3, false},
    {"those", 0, 2, 3, false},
    {NULL, 0, 0, 0, false}
};

/*=============================================================================
 * HELPER FUNCTIONS
 *===========================================================================*/

/**
 * @brief Case-insensitive strstr
 */
static const char* strcasestr_local(const char* haystack, const char* needle) {
    if (!haystack || !needle) {
        return NULL;
    }
    if (!*needle) return haystack;

    size_t needle_len = strlen(needle);
    while (*haystack) {
        bool match = true;
        for (size_t i = 0; i < needle_len; i++) {
            if (!haystack[i] || tolower((unsigned char)haystack[i]) !=
                                tolower((unsigned char)needle[i])) {
                match = false;
                break;
            }
        }
        if (match) return haystack;
        haystack++;
    }
    return NULL; /* Not found - normal case, not an error */
}

static bool contains_word(const char* text, const char* word) {
    if (!text || !word) {
        return false;
    }

    const char* p = text;
    size_t word_len = strlen(word);

    while ((p = strcasestr_local(p, word)) != NULL) {
        /* Check word boundaries */
        bool start_ok = (p == text) || !isalnum((unsigned char)*(p - 1));
        bool end_ok = !isalnum((unsigned char)*(p + word_len));

        if (start_ok && end_ok) return true;
        p++;
    }
    return false;
}

static const pronoun_info_t* find_pronoun(const char* word) {
    if (!word) {
        return NULL;
    }

    /* Convert to lowercase for comparison */
    char lower[32];
    size_t i;
    for (i = 0; i < sizeof(lower) - 1 && word[i]; i++) {
        lower[i] = tolower((unsigned char)word[i]);
    }
    lower[i] = '\0';

    for (const pronoun_info_t* p = PRONOUNS; p->pronoun; p++) {
        if (strcmp(lower, p->pronoun) == 0) {
            return p;
        }
    }
    return NULL; /* Not a pronoun - normal case, not an error */
}

static double get_time_ms(void) {
    return 0.0;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

discourse_config_t discourse_default_config(void) {
    discourse_config_t config;
    memset(&config, 0, sizeof(config));

    config.max_turns = DISCOURSE_DEFAULT_MAX_TURNS;
    config.max_referents = DISCOURSE_DEFAULT_MAX_REFERENTS;
    config.max_topics = DISCOURSE_DEFAULT_MAX_TOPICS;
    config.history_depth = DISCOURSE_DEFAULT_HISTORY_DEPTH;

    config.salience_decay = 0.8f;
    config.resolution_threshold = 0.5f;
    config.enable_zero_anaphora = false;

    config.enable_topic_tracking = true;
    config.topic_shift_threshold = 0.4f;

    config.enable_bio_async = false;
    config.enable_working_memory = true;

    return config;
}

discourse_manager_t* discourse_create(const discourse_config_t* config) {
    discourse_manager_t* manager = (discourse_manager_t*)nimcp_calloc(1, sizeof(discourse_manager_t));
    if (!manager) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "discourse_create: failed to allocate manager");

        return NULL;

    }

    if (config) {
        manager->config = *config;
    } else {
        manager->config = discourse_default_config();
    }

    /* Allocate referents */
    manager->referents = (discourse_referent_t*)nimcp_calloc(
        manager->config.max_referents, sizeof(discourse_referent_t));
    if (!manager->referents) {
        nimcp_free(manager);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "discourse_create: manager->referents is NULL");
        return NULL;
    }

    /* Allocate topics */
    manager->topics = (discourse_topic_t*)nimcp_calloc(
        manager->config.max_topics, sizeof(discourse_topic_t));
    if (!manager->topics) {
        nimcp_free(manager->referents);
        nimcp_free(manager);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "discourse_create: manager->topics is NULL");
        return NULL;
    }

    /* Allocate turns */
    manager->turns = (discourse_turn_t*)nimcp_calloc(
        manager->config.max_turns, sizeof(discourse_turn_t));
    if (!manager->turns) {
        nimcp_free(manager->topics);
        nimcp_free(manager->referents);
        nimcp_free(manager);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "discourse_create: manager->turns is NULL");
        return NULL;
    }

    manager->status = DISCOURSE_STATUS_IDLE;
    manager->last_error = DISCOURSE_ERROR_NONE;
    manager->next_referent_id = 1;
    manager->next_topic_id = 1;
    manager->next_turn_id = 1;

    return manager;
}

void discourse_destroy(discourse_manager_t* manager) {
    if (!manager) return;

    nimcp_free(manager->turns);
    nimcp_free(manager->topics);
    nimcp_free(manager->referents);
    nimcp_free(manager);
}

bool discourse_reset(discourse_manager_t* manager) {
    if (!manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "discourse_reset: manager is NULL");
        return false;
    }

    manager->referent_count = 0;
    manager->topic_count = 0;
    manager->turn_count = 0;
    manager->turn_head = 0;
    manager->current_topic_id = 0;

    manager->status = DISCOURSE_STATUS_IDLE;
    manager->last_error = DISCOURSE_ERROR_NONE;

    memset(manager->referents, 0, manager->config.max_referents * sizeof(discourse_referent_t));
    memset(manager->topics, 0, manager->config.max_topics * sizeof(discourse_topic_t));
    memset(manager->turns, 0, manager->config.max_turns * sizeof(discourse_turn_t));

    return true;
}

/*=============================================================================
 * REFERENT MANAGEMENT
 *===========================================================================*/

uint32_t discourse_introduce_referent(
    discourse_manager_t* manager,
    const char* name,
    referent_type_t type,
    uint8_t gender,
    uint8_t number) {

    if (!manager || !name) return 0;
    if (manager->referent_count >= manager->config.max_referents) {
        manager->last_error = DISCOURSE_ERROR_BUFFER_FULL;
        return 0;
    }

    /* Check if referent already exists */
    for (uint32_t i = 0; i < manager->referent_count; i++) {
        if (strcmp(manager->referents[i].name, name) == 0) {
            /* Boost salience instead of creating duplicate */
            manager->referents[i].salience = 1.0f;
            manager->referents[i].last_mentioned_turn = manager->turn_count;
            manager->referents[i].mention_count++;
            return manager->referents[i].referent_id;
        }
    }

    /* Create new referent */
    discourse_referent_t* ref = &manager->referents[manager->referent_count++];
    ref->referent_id = manager->next_referent_id++;
    ref->type = type;
    strncpy(ref->name, name, DISCOURSE_MAX_ENTITY_NAME - 1);
    ref->name[DISCOURSE_MAX_ENTITY_NAME - 1] = '\0';
    ref->gender = gender;
    ref->number = number;
    ref->person = 3;  /* Default to third person */
    ref->is_animate = (type == REFERENT_TYPE_PERSON);
    ref->salience = 1.0f;
    ref->introduction_turn = manager->turn_count;
    ref->last_mentioned_turn = manager->turn_count;
    ref->mention_count = 1;

    manager->stats.referents_created++;
    return ref->referent_id;
}

bool discourse_get_referent(
    const discourse_manager_t* manager,
    uint32_t referent_id,
    discourse_referent_t* referent) {

    if (!manager || !referent || referent_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "discourse_reset: required parameter is NULL (manager, referent)");
        return false;
    }

    for (uint32_t i = 0; i < manager->referent_count; i++) {
        if (manager->referents[i].referent_id == referent_id) {
            *referent = manager->referents[i];
            return true;
        }
    }

    return false; /* Referent not found — normal lookup miss */
}

bool discourse_find_referent(
    const discourse_manager_t* manager,
    const char* name,
    discourse_referent_t* referent) {

    if (!manager || !name || !referent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "discourse_find_referent: required parameter is NULL (manager, name, referent)");
        return false;
    }

    for (uint32_t i = 0; i < manager->referent_count; i++) {
        if (strcmp(manager->referents[i].name, name) == 0) {
            *referent = manager->referents[i];
            return true;
        }
    }

    return false; /* Referent not found — normal lookup miss */
}

uint32_t discourse_get_salient_referents(
    const discourse_manager_t* manager,
    discourse_referent_t* referents,
    uint32_t max_count) {

    if (!manager || !referents || max_count == 0) return 0;
    if (manager->referent_count == 0) return 0;

    /* Simple bubble sort by salience (small arrays) */
    uint32_t indices[DISCOURSE_DEFAULT_MAX_REFERENTS];
    for (uint32_t i = 0; i < manager->referent_count; i++) {
        indices[i] = i;
    }

    for (uint32_t i = 0; i + 1 < manager->referent_count; i++) {
        for (uint32_t j = 0; j + 1 < manager->referent_count - i; j++) {
            if (manager->referents[indices[j]].salience <
                manager->referents[indices[j + 1]].salience) {
                uint32_t temp = indices[j];
                indices[j] = indices[j + 1];
                indices[j + 1] = temp;
            }
        }
    }

    uint32_t count = (manager->referent_count < max_count) ?
                     manager->referent_count : max_count;

    for (uint32_t i = 0; i < count; i++) {
        referents[i] = manager->referents[indices[i]];
    }

    return count;
}

bool discourse_boost_referent_salience(
    discourse_manager_t* manager,
    uint32_t referent_id,
    float salience_boost) {

    if (!manager || referent_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "discourse_reset: manager is NULL");
        return false;
    }

    for (uint32_t i = 0; i < manager->referent_count; i++) {
        if (manager->referents[i].referent_id == referent_id) {
            manager->referents[i].salience += salience_boost;
            if (manager->referents[i].salience > 1.0f) {
                manager->referents[i].salience = 1.0f;
            }
            manager->referents[i].last_mentioned_turn = manager->turn_count;
            manager->referents[i].mention_count++;
            return true;
        }
    }

    return false; /* Referent not found — normal lookup miss */
}

/*=============================================================================
 * ANAPHORA RESOLUTION
 *===========================================================================*/

bool discourse_resolve_anaphora(
    discourse_manager_t* manager,
    const char* expression,
    const char* context,
    anaphora_resolution_t* result) {

    if (!manager || !expression || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "discourse_resolve_anaphora: required parameter is NULL (manager, expression, result)");
        return false;
    }
    (void)context;  /* Context used in more sophisticated implementation */

    manager->status = DISCOURSE_STATUS_RESOLVING;
    memset(result, 0, sizeof(anaphora_resolution_t));
    strncpy(result->expression, expression, sizeof(result->expression) - 1);

    /* Find pronoun info */
    const pronoun_info_t* pinfo = find_pronoun(expression);
    if (!pinfo) {
        /* Check for demonstratives */
        if (strcmp(expression, "this") == 0 || strcmp(expression, "that") == 0) {
            result->type = ANAPHORA_TYPE_DEMONSTRATIVE;
        } else {
            manager->status = DISCOURSE_STATUS_READY;
            return false; /* Not a resolvable expression — normal condition */
        }
    } else {
        result->type = ANAPHORA_TYPE_PRONOUN;
    }

    /* Find best matching referent */
    float best_score = 0.0f;
    uint32_t best_id = 0;

    for (uint32_t i = 0; i < manager->referent_count; i++) {
        discourse_referent_t* ref = &manager->referents[i];
        float score = ref->salience;

        if (pinfo) {
            /* Check gender match */
            if (pinfo->gender != 0 && ref->gender != 0 &&
                pinfo->gender != ref->gender) {
                continue;
            }

            /* Check number match */
            if (pinfo->number != 0 && ref->number != 0 &&
                pinfo->number != ref->number) {
                continue;
            }

            /* Check animacy */
            if (pinfo->is_animate && !ref->is_animate &&
                ref->type != REFERENT_TYPE_PERSON) {
                score *= 0.5f;  /* Reduce but don't exclude */
            }
        }

        /* Recency bonus */
        uint32_t turns_ago = manager->turn_count - ref->last_mentioned_turn;
        if (turns_ago < manager->config.history_depth) {
            score *= (1.0f - 0.1f * turns_ago);
        }

        if (score > best_score) {
            best_score = score;
            best_id = ref->referent_id;
        }
    }

    if (best_score >= manager->config.resolution_threshold) {
        result->antecedent_id = best_id;
        result->resolution_confidence = best_score;
        result->is_resolved = true;
        manager->stats.anaphora_resolved++;
    } else {
        result->is_resolved = false;
        manager->stats.anaphora_failed++;
    }

    manager->status = DISCOURSE_STATUS_READY;
    return result->is_resolved;
}

uint32_t discourse_resolve_all_anaphora(
    discourse_manager_t* manager,
    const char* utterance,
    anaphora_resolution_t* resolutions,
    uint32_t max_resolutions) {

    if (!manager || !utterance || !resolutions || max_resolutions == 0) return 0;

    uint32_t count = 0;

    /* Check for each known pronoun */
    for (const pronoun_info_t* p = PRONOUNS; p->pronoun && count < max_resolutions; p++) {
        if (contains_word(utterance, p->pronoun)) {
            anaphora_resolution_t result;
            if (discourse_resolve_anaphora(manager, p->pronoun, utterance, &result)) {
                resolutions[count++] = result;
            }
        }
    }

    return count;
}

uint32_t discourse_get_antecedent_candidates(
    const discourse_manager_t* manager,
    anaphora_type_t anaphor_type,
    uint8_t gender,
    uint8_t number,
    discourse_referent_t* candidates,
    uint32_t max_candidates) {

    if (!manager || !candidates || max_candidates == 0) return 0;
    (void)anaphor_type;  /* Could filter based on type */

    uint32_t count = 0;

    for (uint32_t i = 0; i < manager->referent_count && count < max_candidates; i++) {
        discourse_referent_t* ref = &manager->referents[i];

        /* Filter by gender if specified */
        if (gender != 0 && ref->gender != 0 && gender != ref->gender) {
            continue;
        }

        /* Filter by number if specified */
        if (number != 0 && ref->number != 0 && number != ref->number) {
            continue;
        }

        candidates[count++] = *ref;
    }

    return count;
}

/*=============================================================================
 * TOPIC MANAGEMENT
 *===========================================================================*/

uint32_t discourse_introduce_topic(
    discourse_manager_t* manager,
    const char* name) {

    if (!manager || !name) return 0;
    if (manager->topic_count >= manager->config.max_topics) {
        manager->last_error = DISCOURSE_ERROR_BUFFER_FULL;
        return 0;
    }

    /* Check if topic already exists */
    for (uint32_t i = 0; i < manager->topic_count; i++) {
        if (strcmp(manager->topics[i].name, name) == 0) {
            manager->topics[i].salience = 1.0f;
            manager->topics[i].last_active_turn = manager->turn_count;
            return manager->topics[i].topic_id;
        }
    }

    /* Create new topic */
    discourse_topic_t* topic = &manager->topics[manager->topic_count++];
    topic->topic_id = manager->next_topic_id++;
    strncpy(topic->name, name, DISCOURSE_MAX_TOPIC_NAME - 1);
    topic->name[DISCOURSE_MAX_TOPIC_NAME - 1] = '\0';
    topic->salience = 1.0f;
    topic->introduction_turn = manager->turn_count;
    topic->last_active_turn = manager->turn_count;
    topic->is_current = (manager->current_topic_id == 0);

    if (topic->is_current) {
        manager->current_topic_id = topic->topic_id;
    }

    return topic->topic_id;
}

bool discourse_get_current_topic(
    const discourse_manager_t* manager,
    discourse_topic_t* topic) {

    if (!manager || !topic || manager->current_topic_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (manager, topic)");
        return false;
    }

    for (uint32_t i = 0; i < manager->topic_count; i++) {
        if (manager->topics[i].topic_id == manager->current_topic_id) {
            *topic = manager->topics[i];
            return true;
        }
    }

    return false; /* Topic not found — normal lookup miss */
}

bool discourse_set_current_topic(
    discourse_manager_t* manager,
    uint32_t topic_id) {

    if (!manager || topic_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "discourse_set_current_topic: manager is NULL or topic_id is zero");
        return false;
    }

    /* Clear current flags */
    for (uint32_t i = 0; i < manager->topic_count; i++) {
        manager->topics[i].is_current = false;
    }

    /* Set new current */
    for (uint32_t i = 0; i < manager->topic_count; i++) {
        if (manager->topics[i].topic_id == topic_id) {
            manager->topics[i].is_current = true;
            manager->topics[i].last_active_turn = manager->turn_count;
            manager->current_topic_id = topic_id;
            return true;
        }
    }

    return false; /* Topic not found — normal lookup miss */
}

float discourse_detect_topic_shift(
    discourse_manager_t* manager,
    const char* utterance,
    topic_shift_t* shift_type) {

    if (!manager || !utterance || !shift_type) return 0.0f;

    if (!manager->config.enable_topic_tracking) {
        *shift_type = TOPIC_SHIFT_NONE;
        return 0.0f;
    }

    /* Simple heuristic: look for topic shift markers */
    if (contains_word(utterance, "anyway") ||
        contains_word(utterance, "by the way") ||
        contains_word(utterance, "speaking of")) {
        *shift_type = TOPIC_SHIFT_DIGRESSION;
        manager->stats.topic_shifts_detected++;
        return 0.7f;
    }

    if (contains_word(utterance, "back to") ||
        contains_word(utterance, "as I was saying")) {
        *shift_type = TOPIC_SHIFT_RETURN;
        manager->stats.topic_shifts_detected++;
        return 0.8f;
    }

    if (contains_word(utterance, "but") ||
        contains_word(utterance, "however")) {
        *shift_type = TOPIC_SHIFT_CHANGE;
        return 0.4f;
    }

    *shift_type = TOPIC_SHIFT_CONTINUATION;
    return 0.2f;
}

uint32_t discourse_get_recent_topics(
    const discourse_manager_t* manager,
    discourse_topic_t* topics,
    uint32_t max_topics) {

    if (!manager || !topics || max_topics == 0) return 0;

    uint32_t count = (manager->topic_count < max_topics) ?
                     manager->topic_count : max_topics;

    /* Return most recently active first */
    uint32_t indices[DISCOURSE_DEFAULT_MAX_TOPICS];
    for (uint32_t i = 0; i < manager->topic_count; i++) {
        indices[i] = i;
    }

    /* Sort by last_active_turn descending */
    for (uint32_t i = 0; i < manager->topic_count - 1; i++) {
        for (uint32_t j = 0; j < manager->topic_count - 1 - i; j++) {
            if (manager->topics[indices[j]].last_active_turn <
                manager->topics[indices[j + 1]].last_active_turn) {
                uint32_t temp = indices[j];
                indices[j] = indices[j + 1];
                indices[j + 1] = temp;
            }
        }
    }

    for (uint32_t i = 0; i < count; i++) {
        topics[i] = manager->topics[indices[i]];
    }

    return count;
}

/*=============================================================================
 * TURN MANAGEMENT
 *===========================================================================*/

uint32_t discourse_add_turn(
    discourse_manager_t* manager,
    uint32_t speaker_id,
    const char* content,
    uint64_t timestamp_ms) {

    if (!manager || !content) return 0;

    /* Get slot in circular buffer */
    uint32_t slot = manager->turn_head;

    discourse_turn_t* turn = &manager->turns[slot];
    memset(turn, 0, sizeof(discourse_turn_t));

    turn->turn_id = manager->next_turn_id++;
    turn->speaker_id = speaker_id;
    turn->timestamp_ms = timestamp_ms;
    strncpy(turn->content, content, sizeof(turn->content) - 1);
    turn->content[sizeof(turn->content) - 1] = '\0';
    turn->topic_id = manager->current_topic_id;

    /* Detect topic shift */
    turn->topic_shift = TOPIC_SHIFT_CONTINUATION;
    discourse_detect_topic_shift(manager, content, &turn->topic_shift);

    /* Default coherence */
    turn->coherence = COHERENCE_RELATION_ELABORATION;
    turn->coherence_score = 0.7f;

    /* Update circular buffer */
    manager->turn_head = (manager->turn_head + 1) % manager->config.max_turns;
    if (manager->turn_count < manager->config.max_turns) {
        manager->turn_count++;
    }

    /* Decay salience of referents */
    for (uint32_t i = 0; i < manager->referent_count; i++) {
        manager->referents[i].salience *= manager->config.salience_decay;
    }

    manager->stats.turns_processed++;

    /* W12 KG emit: one node per discourse turn (utterance-level). */
    w12_emit_discourse_turn_auto(turn->turn_id, speaker_id,
                                 (int)turn->topic_shift,
                                 turn->coherence_score);
    return turn->turn_id;
}

bool discourse_get_turn(
    const discourse_manager_t* manager,
    uint32_t turn_id,
    discourse_turn_t* turn) {

    if (!manager || !turn || turn_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (manager, turn)");
        return false;
    }

    for (uint32_t i = 0; i < manager->turn_count; i++) {
        uint32_t idx = (manager->turn_head + manager->config.max_turns - 1 - i)
                       % manager->config.max_turns;
        if (manager->turns[idx].turn_id == turn_id) {
            *turn = manager->turns[idx];
            return true;
        }
    }

    return false; /* Turn not found — normal lookup miss */
}

uint32_t discourse_get_recent_turns(
    const discourse_manager_t* manager,
    discourse_turn_t* turns,
    uint32_t max_turns) {

    if (!manager || !turns || max_turns == 0) return 0;

    uint32_t count = (manager->turn_count < max_turns) ?
                     manager->turn_count : max_turns;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t idx = (manager->turn_head + manager->config.max_turns - 1 - i)
                       % manager->config.max_turns;
        turns[i] = manager->turns[idx];
    }

    return count;
}

uint32_t discourse_get_turn_count(const discourse_manager_t* manager) {
    if (!manager) return 0;
    return manager->turn_count;
}

/*=============================================================================
 * COHERENCE ANALYSIS
 *===========================================================================*/

float discourse_analyze_coherence(
    const discourse_manager_t* manager,
    uint32_t turn1_id,
    uint32_t turn2_id,
    coherence_relation_t* relation) {

    if (!manager || !relation) return 0.0f;

    discourse_turn_t turn1, turn2;
    if (!discourse_get_turn(manager, turn1_id, &turn1) ||
        !discourse_get_turn(manager, turn2_id, &turn2)) {
        return 0.0f;
    }

    /* Simple heuristic-based coherence */
    float score = 0.5f;  /* Base score */

    /* Same topic increases coherence */
    if (turn1.topic_id == turn2.topic_id && turn1.topic_id != 0) {
        score += 0.2f;
    }

    /* Same speaker may indicate continuation */
    if (turn1.speaker_id == turn2.speaker_id) {
        score += 0.1f;
    }

    /* Check for contrast markers */
    if (contains_word(turn2.content, "but") ||
        contains_word(turn2.content, "however")) {
        *relation = COHERENCE_RELATION_CONTRAST;
    }
    /* Check for explanation */
    else if (contains_word(turn2.content, "because") ||
             contains_word(turn2.content, "since")) {
        *relation = COHERENCE_RELATION_EXPLANATION;
        score += 0.1f;
    }
    /* Check for result */
    else if (contains_word(turn2.content, "so") ||
             contains_word(turn2.content, "therefore")) {
        *relation = COHERENCE_RELATION_RESULT;
        score += 0.1f;
    }
    else {
        *relation = COHERENCE_RELATION_ELABORATION;
    }

    if (score > 1.0f) score = 1.0f;
    return score;
}

float discourse_get_overall_coherence(const discourse_manager_t* manager) {
    if (!manager || manager->turn_count < 2) return 1.0f;

    return (float)manager->stats.avg_coherence_score;
}

/*=============================================================================
 * FULL ANALYSIS
 *===========================================================================*/

bool discourse_analyze(
    discourse_manager_t* manager,
    uint32_t speaker_id,
    const char* utterance,
    uint64_t timestamp_ms,
    discourse_analysis_t* analysis) {

    if (!manager || !utterance || !analysis) {
        if (manager) manager->last_error = DISCOURSE_ERROR_INVALID_INPUT;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "discourse_get_overall_coherence: validation failed");
        return false;
    }

    double start_time = get_time_ms();
    manager->status = DISCOURSE_STATUS_PROCESSING;
    memset(analysis, 0, sizeof(discourse_analysis_t));

    /* 1. Resolve anaphora */
    analysis->resolution_count = discourse_resolve_all_anaphora(
        manager, utterance, analysis->resolutions, 8);

    /* 2. Detect topic shift */
    analysis->topic_shift = TOPIC_SHIFT_CONTINUATION;
    discourse_detect_topic_shift(manager, utterance, &analysis->topic_shift);
    analysis->current_topic_id = manager->current_topic_id;

    /* 3. Analyze coherence with previous turn */
    if (manager->turn_count > 0) {
        uint32_t prev_idx = (manager->turn_head + manager->config.max_turns - 1)
                           % manager->config.max_turns;
        uint32_t prev_turn_id = manager->turns[prev_idx].turn_id;

        /* We'll use heuristics since we don't have the new turn ID yet */
        coherence_relation_t rel;
        float coherence = 0.7f;

        if (contains_word(utterance, "but") || contains_word(utterance, "however")) {
            rel = COHERENCE_RELATION_CONTRAST;
        } else if (contains_word(utterance, "because")) {
            rel = COHERENCE_RELATION_EXPLANATION;
            coherence = 0.8f;
        } else {
            rel = COHERENCE_RELATION_ELABORATION;
        }

        analysis->coherence_relation = rel;
        analysis->coherence_score = coherence;
        (void)prev_turn_id;
    }

    /* 4. Add turn to discourse */
    discourse_add_turn(manager, speaker_id, utterance, timestamp_ms);

    /* Update stats */
    double elapsed = get_time_ms() - start_time;
    analysis->processing_time_ms = elapsed;

    manager->stats.avg_coherence_score =
        (manager->stats.avg_coherence_score * (manager->stats.turns_processed - 1) +
         analysis->coherence_score) / manager->stats.turns_processed;

    manager->status = DISCOURSE_STATUS_READY;
    return true;
}

/*=============================================================================
 * STATUS AND STATISTICS
 *===========================================================================*/

discourse_status_t discourse_get_status(const discourse_manager_t* manager) {
    if (!manager) return DISCOURSE_STATUS_ERROR;
    return manager->status;
}

discourse_error_t discourse_get_last_error(const discourse_manager_t* manager) {
    if (!manager) return DISCOURSE_ERROR_INTERNAL;
    return manager->last_error;
}

bool discourse_get_stats(const discourse_manager_t* manager, discourse_stats_t* stats) {
    if (!manager || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "discourse_get_stats: required parameter is NULL (manager, stats)");
        return false;
    }
    *stats = manager->stats;
    return true;
}

void discourse_reset_stats(discourse_manager_t* manager) {
    if (!manager) return;
    memset(&manager->stats, 0, sizeof(discourse_stats_t));
}

bool discourse_get_config(const discourse_manager_t* manager, discourse_config_t* config) {
    if (!manager || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "discourse_get_config: required parameter is NULL (manager, config)");
        return false;
    }
    *config = manager->config;
    return true;
}

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

bool discourse_register_bio_handler(
    discourse_manager_t* manager,
    bio_router_t* router) {

    if (!manager || !router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "discourse_get_config: required parameter is NULL (manager, router)");
        return false;
    }

    manager->router = router;
    manager->bio_registered = true;
    return true;
}
