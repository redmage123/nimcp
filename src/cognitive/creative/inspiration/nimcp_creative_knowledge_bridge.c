//=============================================================================
// nimcp_creative_knowledge_bridge.c - Creative Knowledge Graph Integration
//=============================================================================
/**
 * @file nimcp_creative_knowledge_bridge.c
 * @brief Integrates creative system with knowledge graph for art knowledge
 *
 * WHAT: Provides access to structured art/culture knowledge
 * WHY:  Enable informed creative decisions based on art history
 * HOW:  Interface to brain's knowledge graph with art-specific queries
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#include "cognitive/creative/inspiration/nimcp_creative_knowledge_bridge.h"
#include "cognitive/creative/nimcp_creative.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include <string.h>
#include <strings.h>
#include <time.h>
#include <stdio.h>

#define LOG_MODULE "CREATIVE_KNOWLEDGE"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(creative_knowledge_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_creative_knowledge_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_creative_knowledge_bridge_mesh_registry = NULL;

nimcp_error_t creative_knowledge_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_creative_knowledge_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "creative_knowledge_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "creative_knowledge_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_creative_knowledge_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_creative_knowledge_bridge_mesh_registry = registry;
    return err;
}

void creative_knowledge_bridge_mesh_unregister(void) {
    if (g_creative_knowledge_bridge_mesh_registry && g_creative_knowledge_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_creative_knowledge_bridge_mesh_registry, g_creative_knowledge_bridge_mesh_id);
        g_creative_knowledge_bridge_mesh_id = 0;
        g_creative_knowledge_bridge_mesh_registry = NULL;
    }
}


#define DEFAULT_MAX_RESULTS 50
#define DEFAULT_CACHE_SIZE 100

//=============================================================================
// Helper Functions for Archetype Names
//=============================================================================

static const char* literary_archetype_name(literary_style_archetype_t id) {
    static const char* names[] = {
        "Hemingway", "Tolstoy", "Joyce", "Poe", "Austen", "Shakespeare",
        "Borges", "Kafka", "Marquez", "Dostoevsky", "Woolf", "Faulkner"
    };
    if (id >= 0 && id < STYLE_LIT_COUNT) return names[id];
    return "Unknown Literary";
}

static const char* musical_archetype_name(musical_style_archetype_t id) {
    static const char* names[] = {
        "Bach", "Beethoven", "Debussy", "John Williams", "Miles Davis", "Hans Zimmer",
        "Stravinsky", "Ennio Morricone", "Sakamoto", "Glass", "Copland", "Ravel"
    };
    if (id >= 0 && id < STYLE_MUSIC_COUNT) return names[id];
    return "Unknown Musical";
}

static const char* visual_archetype_name(visual_style_archetype_t id) {
    static const char* names[] = {
        "Van Gogh", "Monet", "Picasso", "Dali", "Warhol", "Rembrandt",
        "Klimt", "Escher", "Hokusai", "Basquiat", "Caravaggio", "Kandinsky"
    };
    if (id >= 0 && id < STYLE_VIS_COUNT) return names[id];
    return "Unknown Visual";
}

static const char* cinematic_archetype_name(cinematic_style_archetype_t id) {
    static const char* names[] = {
        "Kubrick", "Spielberg", "Tarantino", "Nolan", "Tarkovsky", "Miyazaki",
        "Hitchcock", "Welles", "Kurosawa", "Fincher", "Villeneuve", "Coppola"
    };
    if (id >= 0 && id < STYLE_CINEMA_COUNT) return names[id];
    return "Unknown Cinematic";
}

static const char* art_modality_name(art_modality_t modality) {
    if (art_modality_is_text(modality)) return "literary";
    if (art_modality_is_music(modality)) return "musical";
    if (art_modality_is_visual(modality)) return "visual";
    if (art_modality_is_video(modality)) return "cinematic";
    return "artistic";
}

//=============================================================================
// Static Knowledge Data (Seed Data)
//=============================================================================

/* Sample artist data */
static const art_artist_info_t seed_artists[] = {
    {1, "William Shakespeare", "1564", "1616", "English", ART_MODALITY_TEXT_PROSE,
     STYLE_LIT_SHAKESPEARE, "English playwright and poet, widely regarded as the greatest writer "
     "in the English language. Known for plays like Hamlet, Macbeth, and Romeo and Juliet.",
     NULL, NULL, 1.0f},
    {2, "Ernest Hemingway", "1899", "1961", "American", ART_MODALITY_TEXT_PROSE,
     STYLE_LIT_HEMINGWAY, "American novelist known for his economical, understated style. "
     "Nobel Prize in Literature 1954. Works include The Old Man and the Sea, A Farewell to Arms.",
     NULL, NULL, 0.9f},
    {3, "Johann Sebastian Bach", "1685", "1750", "German", ART_MODALITY_MUSIC_CLASSICAL,
     STYLE_MUSIC_BACH, "German composer and musician of the Baroque period. "
     "Known for instrumental compositions and religious works.",
     NULL, NULL, 1.0f},
    {4, "Ludwig van Beethoven", "1770", "1827", "German", ART_MODALITY_MUSIC_CLASSICAL,
     STYLE_MUSIC_BEETHOVEN, "German composer and pianist. Central figure in the transition "
     "between Classical and Romantic eras. Known for nine symphonies.",
     NULL, NULL, 1.0f},
    {5, "Vincent van Gogh", "1853", "1890", "Dutch", ART_MODALITY_VISUAL_PAINTING,
     STYLE_VIS_VAN_GOGH, "Dutch Post-Impressionist painter. Known for bold colors and "
     "expressive brushwork. Works include The Starry Night, Sunflowers.",
     NULL, NULL, 0.95f},
    {6, "Stanley Kubrick", "1928", "1999", "American", ART_MODALITY_VIDEO_CINEMA,
     STYLE_CINEMA_KUBRICK, "American film director known for his meticulous attention to detail "
     "and visual style. Works include 2001: A Space Odyssey, The Shining.",
     NULL, NULL, 0.95f}
};

static const uint32_t num_seed_artists = sizeof(seed_artists) / sizeof(seed_artists[0]);

/* Sample movement data */
static const art_movement_info_t seed_movements[] = {
    {1, "Impressionism", "1860-1890",
     "Art movement characterized by small, thin brush strokes, emphasis on light, "
     "ordinary subject matter, and unusual visual angles.",
     "Monet, Renoir, Degas, Pissarro", "Water Lilies, Impression Sunrise",
     "Light effects, visible brushstrokes, outdoor scenes",
     "Realism", "Post-Impressionism"},
    {2, "Romanticism", "1800-1850",
     "Artistic and intellectual movement emphasizing emotion, individualism, "
     "and glorification of nature.",
     "Delacroix, Turner, Friedrich", "Liberty Leading the People, The Wanderer",
     "Emotion, nature, individualism, sublime",
     "Neoclassicism", "Realism"},
    {3, "Baroque", "1600-1750",
     "Artistic style using exaggerated motion and clear detail to produce drama, "
     "tension, and grandeur.",
     "Caravaggio, Rembrandt, Vermeer, Bach", "The Night Watch, Girl with a Pearl Earring",
     "Drama, grandeur, chiaroscuro, religious themes",
     "Renaissance", "Rococo"}
};

static const uint32_t num_seed_movements = sizeof(seed_movements) / sizeof(seed_movements[0]);

//=============================================================================
// Config Defaults
//=============================================================================

void creative_knowledge_bridge_config_defaults(
    creative_knowledge_bridge_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(creative_knowledge_bridge_config_t));

    config->max_query_results = DEFAULT_MAX_RESULTS;
    config->min_relevance = 0.3f;
    config->cache_queries = true;
    config->cache_size = DEFAULT_CACHE_SIZE;

    config->use_external_kg = false;
    config->fetch_from_web = false;

    config->auto_learn = true;
    config->learn_confidence_threshold = 0.7f;
}

//=============================================================================
// Internal Cache (Simple)
//=============================================================================

typedef struct {
    char query_key[NIMCP_NAME_BUFFER_SIZE];
    art_entity_type_t type;
    void* results;
    uint32_t count;
    uint64_t timestamp;
} cache_entry_t;

typedef struct {
    cache_entry_t* entries;
    uint32_t count;
    uint32_t capacity;
} query_cache_t;

static query_cache_t* cache_create(uint32_t capacity) {
    query_cache_t* cache = nimcp_calloc(1, sizeof(query_cache_t));
    if (!cache) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cache_create: cache is NULL");
        return NULL;
    }

    cache->entries = nimcp_calloc(capacity, sizeof(cache_entry_t));
    if (!cache->entries) {
        nimcp_free(cache);
        cache = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cache_create: cache->entries is NULL");
        return NULL;
    }
    cache->capacity = capacity;
    return cache;
}

static void cache_destroy(query_cache_t* cache) {
    if (!cache) return;
    if (cache->entries) {
        for (uint32_t i = 0; i < cache->count; i++) {
            if (cache->entries[i].results) {
                nimcp_free(cache->entries[i].results);
            }
        }
        nimcp_free(cache->entries);
    }
    nimcp_free(cache);
    cache = NULL;
}

//=============================================================================
// Internal Helpers
//=============================================================================

static bool artist_matches_name(const art_artist_info_t* artist, const char* name) {
    if (!artist || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "artist_matches_name: required parameter is NULL (artist, name)");
        return false;
    }
    return strcasestr(artist->name, name) != NULL;
}

static bool artist_matches_style(const art_artist_info_t* artist,
                                  art_modality_t modality, int32_t archetype) {
    if (!artist) return false;
    if (artist->primary_modality != modality) return false;
    if (archetype >= 0 && artist->primary_archetype != archetype) return false;
    return true;
}

//=============================================================================
// Lifecycle API
//=============================================================================

creative_knowledge_bridge_t* creative_knowledge_bridge_create(
    const creative_knowledge_bridge_config_t* config) {

    creative_knowledge_bridge_t* bridge = nimcp_calloc(1, sizeof(creative_knowledge_bridge_t));
    if (!bridge) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate knowledge bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "artist_matches_name: bridge is NULL");
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        creative_knowledge_bridge_config_defaults(&bridge->config);
    }

    /* Create cache if enabled */
    if (bridge->config.cache_queries) {
        bridge->query_cache = cache_create(bridge->config.cache_size);
    }

    LOG_INFO(LOG_MODULE, "Creative knowledge bridge created");

    return bridge;
}

void creative_knowledge_bridge_destroy(creative_knowledge_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->query_cache) {
        cache_destroy((query_cache_t*)bridge->query_cache);
    }

    nimcp_free(bridge);
    bridge = NULL;

    LOG_INFO(LOG_MODULE, "Creative knowledge bridge destroyed");
}

void creative_knowledge_set_brain_kg(creative_knowledge_bridge_t* bridge,
                                      void* brain_kg) {
    if (!bridge) return;
    bridge->brain_kg = brain_kg;
}

//=============================================================================
// Artist Query API
//=============================================================================

int creative_knowledge_get_artist(creative_knowledge_bridge_t* bridge,
                                   const char* name,
                                   art_artist_info_t* out) {
    if (!bridge || !name || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_knowledge_bridge_destroy: required parameter is NULL (bridge, name, out)");
        return -1;
    }

    bridge->queries_performed++;

    /* Search seed data */
    for (uint32_t i = 0; i < num_seed_artists; i++) {
        if (artist_matches_name(&seed_artists[i], name)) {
            *out = seed_artists[i];
            return 0;
        }
    }

    /* Would query external KG here */
    if (bridge->brain_kg) {
        LOG_DEBUG(LOG_MODULE, "Would query external KG for artist: %s", name);
    }

    return -1;  /* Not found — normal condition, not an immune event */
}

uint32_t creative_knowledge_artists_by_style(creative_knowledge_bridge_t* bridge,
                                              art_modality_t modality,
                                              int32_t archetype_id,
                                              uint32_t max_results,
                                              art_artist_info_t* results) {
    if (!bridge || !results) return 0;

    bridge->queries_performed++;
    uint32_t count = 0;

    for (uint32_t i = 0; i < num_seed_artists && count < max_results; i++) {
        if (artist_matches_style(&seed_artists[i], modality, archetype_id)) {
            results[count++] = seed_artists[i];
        }
    }

    return count;
}

uint32_t creative_knowledge_influenced_by(creative_knowledge_bridge_t* bridge,
                                           const char* artist_name,
                                           uint32_t max_results,
                                           art_artist_info_t* results) {
    if (!bridge || !artist_name || !results) return 0;

    bridge->queries_performed++;

    /* Simplified: return artists of same modality */
    art_artist_info_t source;
    if (creative_knowledge_get_artist(bridge, artist_name, &source) < 0) {
        return 0;
    }

    uint32_t count = 0;
    for (uint32_t i = 0; i < num_seed_artists && count < max_results; i++) {
        if (seed_artists[i].primary_modality == source.primary_modality &&
            strcasecmp(seed_artists[i].name, artist_name) != 0) {
            results[count++] = seed_artists[i];
        }
    }

    return count;
}

//=============================================================================
// Work Query API
//=============================================================================

int creative_knowledge_get_work(creative_knowledge_bridge_t* bridge,
                                 const char* title,
                                 art_work_info_t* out) {
    if (!bridge || !title || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_knowledge_bridge_destroy: required parameter is NULL (bridge, title, out)");
        return -1;
    }

    bridge->queries_performed++;

    /* Would have work database; for now return placeholder */
    memset(out, 0, sizeof(art_work_info_t));
    out->entity_id = 0;
    strncpy(out->title, title, sizeof(out->title) - 1);
    strncpy(out->description, "Work information not in database", sizeof(out->description) - 1);

    return -1;  /* Not found — normal condition, not an immune event */
}

uint32_t creative_knowledge_works_by_artist(creative_knowledge_bridge_t* bridge,
                                             const char* artist_name,
                                             uint32_t max_results,
                                             art_work_info_t* results) {
    if (!bridge || !artist_name || !results) return 0;
    (void)max_results;

    bridge->queries_performed++;

    /* Would query works database */
    return 0;
}

uint32_t creative_knowledge_key_works(creative_knowledge_bridge_t* bridge,
                                       const char* movement_name,
                                       uint32_t max_results,
                                       art_work_info_t* results) {
    if (!bridge || !movement_name || !results) return 0;
    (void)max_results;

    bridge->queries_performed++;

    /* Would query key works from movement */
    return 0;
}

//=============================================================================
// Movement Query API
//=============================================================================

int creative_knowledge_get_movement(creative_knowledge_bridge_t* bridge,
                                     const char* name,
                                     art_movement_info_t* out) {
    if (!bridge || !name || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_knowledge_bridge_destroy: required parameter is NULL (bridge, name, out)");
        return -1;
    }

    bridge->queries_performed++;

    for (uint32_t i = 0; i < num_seed_movements; i++) {
        if (strcasestr(seed_movements[i].name, name)) {
            *out = seed_movements[i];
            return 0;
        }
    }

    return -1;  /* Not found — normal condition, not an immune event */
}

uint32_t creative_knowledge_movements_by_period(creative_knowledge_bridge_t* bridge,
                                                 int32_t start_year,
                                                 int32_t end_year,
                                                 int32_t modality,
                                                 uint32_t max_results,
                                                 art_movement_info_t* results) {
    if (!bridge || !results) return 0;
    (void)start_year;
    (void)end_year;
    (void)modality;

    bridge->queries_performed++;

    /* Return all seed movements for now */
    uint32_t count = 0;
    for (uint32_t i = 0; i < num_seed_movements && count < max_results; i++) {
        results[count++] = seed_movements[i];
    }

    return count;
}

void creative_knowledge_movement_lineage(creative_knowledge_bridge_t* bridge,
                                          const char* movement_name,
                                          art_movement_info_t* precursors,
                                          uint32_t max_precursors,
                                          art_movement_info_t* successors,
                                          uint32_t max_successors,
                                          uint32_t* num_precursors,
                                          uint32_t* num_successors) {
    if (!bridge || !movement_name) return;

    if (num_precursors) *num_precursors = 0;
    if (num_successors) *num_successors = 0;

    bridge->queries_performed++;

    art_movement_info_t movement;
    if (creative_knowledge_get_movement(bridge, movement_name, &movement) < 0) {
        return;
    }

    /* Find precursor */
    if (precursors && max_precursors > 0 && movement.precursor[0]) {
        art_movement_info_t pre;
        if (creative_knowledge_get_movement(bridge, movement.precursor, &pre) == 0) {
            precursors[0] = pre;
            if (num_precursors) *num_precursors = 1;
        }
    }

    /* Find successor */
    if (successors && max_successors > 0 && movement.successor[0]) {
        art_movement_info_t suc;
        if (creative_knowledge_get_movement(bridge, movement.successor, &suc) == 0) {
            successors[0] = suc;
            if (num_successors) *num_successors = 1;
        }
    }
}

//=============================================================================
// Context Query API
//=============================================================================

int creative_knowledge_historical_context(creative_knowledge_bridge_t* bridge,
                                           const art_work_info_t* work,
                                           char* context) {
    if (!bridge || !work || !context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, work, context)");
        return -1;
    }

    bridge->queries_performed++;

    /* Generate basic context */
    snprintf(context, 1024,
             "The work '%s' by %s was created in %s. %s It belongs to the %s tradition "
             "and reflects the artistic sensibilities of its time.",
             work->title,
             work->creator[0] ? work->creator : "an unknown artist",
             work->year[0] ? work->year : "an unknown period",
             work->description[0] ? work->description : "",
             work->movement[0] ? work->movement : "classical");

    return 0;
}

int creative_knowledge_style_significance(creative_knowledge_bridge_t* bridge,
                                           art_modality_t modality,
                                           int32_t archetype_id,
                                           char* significance) {
    if (!bridge || !significance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, significance)");
        return -1;
    }

    bridge->queries_performed++;

    const char* archetype_name = literary_archetype_name((literary_style_archetype_t)archetype_id);
    if (art_modality_is_music(modality)) {
        archetype_name = musical_archetype_name((musical_style_archetype_t)archetype_id);
    } else if (art_modality_is_visual(modality)) {
        archetype_name = visual_archetype_name((visual_style_archetype_t)archetype_id);
    } else if (art_modality_is_video(modality)) {
        archetype_name = cinematic_archetype_name((cinematic_style_archetype_t)archetype_id);
    }

    snprintf(significance, 1024,
             "The %s style represents a significant contribution to %s art. "
             "This approach has influenced countless artists and continues to shape "
             "contemporary creative expression. The style is characterized by distinctive "
             "techniques and aesthetic principles that have stood the test of time.",
             archetype_name,
             art_modality_name(modality));

    return 0;
}

//=============================================================================
// Learning API
//=============================================================================

uint64_t creative_knowledge_add_artist(creative_knowledge_bridge_t* bridge,
                                        const art_artist_info_t* info) {
    if (!bridge || !info) return 0;

    if (!bridge->config.auto_learn) {
        LOG_DEBUG(LOG_MODULE, "Auto-learn disabled, not adding artist");
        return 0;
    }

    /* Would add to internal or external KG */
    LOG_DEBUG(LOG_MODULE, "Would add artist: %s", info->name);

    return 0;  /* Would return new entity ID */
}

uint64_t creative_knowledge_add_work(creative_knowledge_bridge_t* bridge,
                                      const art_work_info_t* info) {
    if (!bridge || !info) return 0;

    if (!bridge->config.auto_learn) {
        LOG_DEBUG(LOG_MODULE, "Auto-learn disabled, not adding work");
        return 0;
    }

    /* Would add to internal or external KG */
    LOG_DEBUG(LOG_MODULE, "Would add work: %s", info->title);

    return 0;
}

int creative_knowledge_add_relation(creative_knowledge_bridge_t* bridge,
                                     uint64_t from_id,
                                     uint64_t to_id,
                                     art_relation_type_t relation_type) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: bridge is NULL");
        return -1;
    }
    (void)from_id;
    (void)to_id;
    (void)relation_type;

    /* Would add relation to KG */
    LOG_DEBUG(LOG_MODULE, "Would add relation type %d between %lu and %lu",
              relation_type, from_id, to_id);

    return 0;
}

//=============================================================================
// Cleanup
//=============================================================================

void art_knowledge_result_free(art_knowledge_result_t* result) {
    if (!result) return;

    if (result->entities) {
        nimcp_free(result->entities);
        result->entities = NULL;
    }
    result->count = 0;
}
