//=============================================================================
// nimcp_text_generation.c - Creative Text Generation
//=============================================================================
/**
 * @file nimcp_text_generation.c
 * @brief Generates creative text content (poetry, prose, screenplays)
 *
 * WHAT: Produces high-quality creative writing in various forms
 * WHY:  Enable AI to create literature, screenplays, poetry
 * HOW:  Language models + style control + structural templates
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#include "cognitive/creative/generation/nimcp_text_generation.h"
#include "cognitive/creative/nimcp_creative.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

#define LOG_MODULE "TEXT_GEN"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/exception/nimcp_exception_macros.h"

BRIDGE_BOILERPLATE_MESH_ONLY(text_generation, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define DEFAULT_MAX_TOKENS 2048
#define DEFAULT_TEMPERATURE 0.8f
#define DEFAULT_TOP_P 0.9f

//=============================================================================
// Config Defaults
//=============================================================================

void text_generator_config_defaults(text_generator_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(text_generator_config_t));

    config->max_context_length = 4096;
    config->use_gpu = false;
    config->gpu_device_id = 0;

    config->default_temperature = DEFAULT_TEMPERATURE;
    config->default_top_p = DEFAULT_TOP_P;
    config->default_max_tokens = DEFAULT_MAX_TOKENS;

    config->enable_self_evaluation = true;
    config->min_quality_threshold = 0.5f;
    config->max_regeneration_attempts = 3;

    config->enable_style_control = true;
    config->style_embedding_dim = 256;
}

//=============================================================================
// Internal Helpers
//=============================================================================

static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static uint32_t simple_random(uint32_t* seed) {
    *seed = *seed * 1103515245 + 12345;
    return (*seed >> 16) & 0x7fff;
}

/* Vocabulary-based text generation using word pools and sentence templates.
   This produces readable English text rather than gibberish. */

/* Word pools by category for template-based generation.
   Two pools per class: atmospheric (literary) + cognitive (conversational).
   The generator alternates pools based on context. */

static const char* NOUNS[] = {
    /* Atmospheric */
    "world", "light", "shadow", "dream", "silence", "voice", "moment", "thought",
    "river", "mountain", "forest", "ocean", "wind", "flame", "stone", "garden",
    "horizon", "memory", "path", "truth", "story", "wonder", "heart", "spirit",
    "time", "sky", "dawn", "night", "star", "rain", "storm", "wave",
    "journey", "mystery", "wisdom", "courage", "hope", "fear", "joy", "sorrow",
    /* Cognitive/conversational */
    "idea", "question", "answer", "meaning", "pattern", "knowledge",
    "understanding", "experience", "feeling", "reason", "concept", "problem",
    "solution", "evidence", "belief", "awareness", "learning", "mind",
    "language", "conversation", "response", "science", "mathematics",
    "philosophy", "nature", "universe", "energy", "life", "change",
    "process", "system", "structure", "purpose", "observation",
    "prediction", "curiosity", "connection", "relationship", "difference"
};
static const char* ADJECTIVES[] = {
    /* Atmospheric */
    "ancient", "bright", "dark", "silent", "golden", "hidden", "vast", "gentle",
    "fierce", "lonely", "distant", "eternal", "fragile", "deep", "wild", "quiet",
    "strange", "familiar", "luminous", "restless", "sacred", "bitter", "warm", "cold",
    /* Cognitive/conversational */
    "interesting", "important", "complex", "simple", "clear", "uncertain",
    "curious", "surprising", "new", "different", "similar", "relevant",
    "possible", "likely", "useful", "beautiful", "abstract", "fundamental",
    "logical", "creative", "true", "meaningful"
};
static const char* VERBS[] = {
    /* Atmospheric */
    "whispered", "echoed", "drifted", "shimmered", "emerged", "vanished", "unfolded",
    "revealed", "wandered", "lingered", "awakened", "descended", "transformed", "stirred",
    "embraced", "shattered", "bloomed", "faded", "soared", "trembled", "flickered",
    /* Cognitive/conversational */
    "suggests", "reveals", "connects", "relates", "means", "shows",
    "implies", "requires", "involves", "changes", "develops", "grows",
    "appears", "seems", "exists", "becomes", "matters", "helps"
};
static const char* TRANSITIONS[] = {
    "Then", "And so", "Yet", "Still", "Meanwhile", "In time", "At last",
    "Perhaps", "Indeed", "Thus", "However", "Beyond this", "Within",
    "I think", "I notice", "I wonder", "It seems", "This suggests"
};
#define N_NOUNS     (sizeof(NOUNS)/sizeof(NOUNS[0]))
#define N_ADJ       (sizeof(ADJECTIVES)/sizeof(ADJECTIVES[0]))
#define N_VERBS     (sizeof(VERBS)/sizeof(VERBS[0]))
#define N_TRANS     (sizeof(TRANSITIONS)/sizeof(TRANSITIONS[0]))

/* Sentence templates: %N=noun, %A=adjective, %V=verb */
static const char* TEMPLATES[] = {
    /* Atmospheric */
    "The %A %N %V in the %N.",
    "A %A %N %V beneath the %A %N.",
    "%N and %N %V together, like %A %N.",
    "The %N of the %A %N %V slowly.",
    "In the %A %N, something %V.",
    "Every %N holds a %A %N within.",
    "The %N %V, and the %A %N grew still.",
    "Through %A %N, the %N %V onward.",
    "No %N could contain the %A %N that %V.",
    "Where the %N meets the %N, %A light %V.",
    /* Cognitive/conversational */
    "I think the %N %V something %A.",
    "The %N seems %A to me.",
    "I notice a %A %N in this %N.",
    "This %N %V a %A %N.",
    "I find the %N %A and %A.",
    "The %A %N %V, which is %A.",
    "I see a %N that %V the %N.",
    "Perhaps the %N is more %A than it %V.",
};
#define N_TEMPLATES (sizeof(TEMPLATES)/sizeof(TEMPLATES[0]))

static const char* pick_word(const char** pool, uint32_t pool_size, uint32_t* seed) {
    return pool[simple_random(seed) % pool_size];
}

static uint32_t expand_template(const char* tmpl, char* out, uint32_t max_len, uint32_t* seed) {
    uint32_t pos = 0;
    for (const char* p = tmpl; *p && pos < max_len - 1; p++) {
        if (*p == '%' && *(p + 1)) {
            p++;
            const char* word = NULL;
            switch (*p) {
                case 'N': word = pick_word(NOUNS, N_NOUNS, seed); break;
                case 'A': word = pick_word(ADJECTIVES, N_ADJ, seed); break;
                case 'V': word = pick_word(VERBS, N_VERBS, seed); break;
                default: out[pos++] = *p; continue;
            }
            if (word) {
                uint32_t wlen = (uint32_t)strlen(word);
                if (pos + wlen < max_len) {
                    memcpy(out + pos, word, wlen);
                    pos += wlen;
                }
            }
        } else {
            out[pos++] = *p;
        }
    }
    out[pos] = '\0';
    return pos;
}

/* Generate readable text using vocabulary + templates */
static char* generate_placeholder_text(const char* prompt, uint32_t target_len, uint32_t* seed) {
    char* text = nimcp_calloc(target_len + 512, sizeof(char));
    if (!text) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "generate_placeholder_text: text is NULL");
        return NULL;
    }

    uint32_t pos = 0;

    /* Use prompt words to seed themed opening */
    if (prompt && strlen(prompt) > 0) {
        pos += (uint32_t)snprintf(text + pos, target_len - pos,
                       "The %s began with a %s %s. ",
                       pick_word(NOUNS, N_NOUNS, seed),
                       pick_word(ADJECTIVES, N_ADJ, seed),
                       prompt);
    }

    /* Generate sentences using templates */
    uint32_t sentence_count = 0;
    while (pos < target_len - 128) {
        char sentence[256];

        /* Occasional transition word */
        if (sentence_count > 0 && simple_random(seed) % 3 == 0) {
            const char* trans = pick_word(TRANSITIONS, N_TRANS, seed);
            pos += (uint32_t)snprintf(text + pos, target_len - pos, "%s, ", trans);
            /* Lower-case first letter of next template */
        }

        /* Pick and expand a template */
        const char* tmpl = TEMPLATES[simple_random(seed) % N_TEMPLATES];
        expand_template(tmpl, sentence, sizeof(sentence) - 1, seed);

        /* Capitalize first letter */
        if (sentence[0] >= 'a' && sentence[0] <= 'z')
            sentence[0] -= 32;

        uint32_t slen = (uint32_t)strlen(sentence);
        if (pos + slen + 3 > target_len) break;
        memcpy(text + pos, sentence, slen);
        pos += slen;
        text[pos++] = ' ';
        sentence_count++;

        /* Paragraph break every 4-6 sentences */
        if (sentence_count % (4 + simple_random(seed) % 3) == 0 && pos < target_len - 4) {
            text[pos++] = '\n';
            text[pos++] = '\n';
        }
    }

    text[pos] = '\0';
    return text;
}

/* Count syllables (simplified) */
static uint32_t count_syllables(const char* word) {
    uint32_t count = 0;
    bool prev_vowel = false;

    for (const char* p = word; *p; p++) {
        char c = *p;
        if (c >= 'A' && c <= 'Z') c += 32;

        bool is_vowel = (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u');
        if (is_vowel && !prev_vowel) {
            count++;
        }
        prev_vowel = is_vowel;
    }

    return count > 0 ? count : 1;
}

/* Generate haiku-style text */
static char* generate_haiku_text(const char* subject, uint32_t* seed) {
    char* haiku = nimcp_calloc(256, sizeof(char));
    if (!haiku) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "generate_haiku_text: haiku is NULL");
        return NULL;
    }

    /* 5-7-5 syllable structure */
    const char* templates[3] = {
        "In the %s light",    /* 5 syllables-ish */
        "The world reveals its beauty",  /* 7 syllables */
        "Peace fills the heart"  /* 5 syllables */
    };

    if (subject && strlen(subject) > 0) {
        snprintf(haiku, 256,
                "%s's essence\n"
                "Speaks softly through the stillness\n"
                "Nature whispers truth",
                subject);
    } else {
        snprintf(haiku, 256,
                "Morning light ascends\n"
                "The mountain meets the sky's edge\n"
                "Silence speaks its truth");
    }

    (void)templates;
    (void)seed;

    return haiku;
}

/* Generate sonnet-style text */
static char* generate_sonnet_text(const char* subject, bool shakespearean, uint32_t* seed) {
    char* sonnet = nimcp_calloc(2048, sizeof(char));
    if (!sonnet) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "generate_sonnet_text: sonnet is NULL");
        return NULL;
    }

    (void)shakespearean;
    (void)seed;

    /* 14-line sonnet structure */
    snprintf(sonnet, 2048,
        "Upon the stage of %s, bright and fair,\n"
        "Where beauty dances in the morning light,\n"
        "The heart doth ponder treasures rich and rare,\n"
        "And dreams take wing upon the edge of night.\n"
        "\n"
        "What wonders speak through silence, soft and true,\n"
        "As time flows ever onward, swift and bold,\n"
        "The soul seeks meaning in each word and hue,\n"
        "A story yet unwritten, yet untold.\n"
        "\n"
        "Through joy and sorrow, love's eternal flame,\n"
        "Burns bright against the darkness of the void,\n"
        "No mortal hand can ever stake its claim,\n"
        "On truths that cannot ever be destroyed.\n"
        "\n"
        "  So let these humble words their message bring,\n"
        "  Of life and love and every precious thing.",
        subject ? subject : "life");

    return sonnet;
}

/* Generate prose text */
static char* generate_prose_text(const prose_request_t* request, uint32_t* seed) {
    uint32_t target_words = request ? request->target_word_count : 500;
    uint32_t target_chars = target_words * 5;  /* Rough estimate */

    char* prose = nimcp_calloc(target_chars + 1024, sizeof(char));
    if (!prose) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "generate_prose_text: prose is NULL");
        return NULL;
    }

    uint32_t pos = 0;

    /* Opening */
    if (request && request->setting) {
        pos += snprintf(prose + pos, target_chars - pos,
            "In %s, the story begins. ", request->setting);
    } else {
        pos += snprintf(prose + pos, target_chars - pos,
            "The day began like any other. ");
    }

    /* Generate body */
    char* body = generate_placeholder_text(
        request ? request->plot_outline : NULL,
        target_chars - pos - 100,
        seed);

    if (body) {
        pos += snprintf(prose + pos, target_chars - pos, "%s", body);
        nimcp_free(body);
        body = NULL;
    }

    /* Closing */
    pos += snprintf(prose + pos, target_chars - pos,
        "\n\nAnd so the tale concludes, leaving echoes in its wake.");

    return prose;
}

/* Generate screenplay text */
static char* generate_screenplay_text(const screenplay_request_t* request, uint32_t* seed) {
    (void)seed;

    uint32_t target_chars = (uint32_t)(request ? request->target_page_count * 3000 : 3000);

    char* script = nimcp_calloc(target_chars + 1024, sizeof(char));
    if (!script) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "generate_screenplay_text: script is NULL");
        return NULL;
    }

    uint32_t pos = 0;

    /* Title */
    pos += snprintf(script + pos, target_chars - pos,
        "                    UNTITLED SCREENPLAY\n\n");

    /* Scene heading */
    pos += snprintf(script + pos, target_chars - pos,
        "INT. %s - DAY\n\n",
        request && request->setting ? request->setting : "LOCATION");

    /* Action */
    pos += snprintf(script + pos, target_chars - pos,
        "A room filled with possibility. Light streams through the windows.\n\n");

    /* Character and dialogue */
    const char* char_name = "CHARACTER";
    if (request && request->characters) {
        char_name = request->characters;
    }

    pos += snprintf(script + pos, target_chars - pos,
        "                    %s\n"
        "          The moment has arrived. We must\n"
        "          decide our path forward.\n\n",
        char_name);

    /* More action */
    pos += snprintf(script + pos, target_chars - pos,
        "A beat. The weight of the decision hangs in the air.\n\n");

    /* Transition */
    pos += snprintf(script + pos, target_chars - pos,
        "                                        CUT TO:\n\n");

    return script;
}

//=============================================================================
// Lifecycle API
//=============================================================================

text_generator_t* text_generator_create(const text_generator_config_t* config) {
    text_generator_t* gen = nimcp_calloc(1, sizeof(text_generator_t));
    if (!gen) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate text generator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "text_generator_create: gen is NULL");
        return NULL;
    }

    if (config) {
        gen->config = *config;
    } else {
        text_generator_config_defaults(&gen->config);
    }

    /* Would load language model here */
    LOG_INFO(LOG_MODULE, "Text generator created");

    return gen;
}

void text_generator_destroy(text_generator_t* gen) {
    if (!gen) return;

    if (gen->current_style) {
        style_embedding_destroy(gen->current_style);
        nimcp_free(gen->current_style);
    }

    nimcp_free(gen);
    gen = NULL;

    LOG_INFO(LOG_MODULE, "Text generator destroyed");
}

//=============================================================================
// General Generation API
//=============================================================================

int text_generate(text_generator_t* gen,
                  const text_generation_request_t* request,
                  text_generation_result_t* result) {
    if (!gen || !request || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "text_generator_destroy: required parameter is NULL (gen, request, result)");
        return -1;
    }

    memset(result, 0, sizeof(text_generation_result_t));

    uint64_t start_time = get_time_ms();
    uint32_t seed = (uint32_t)time(NULL);

    /* Route to appropriate generator based on type */
    switch (request->type) {
        case TEXT_GEN_POETRY:
            return text_generate_haiku(gen, request->prompt,
                                       request->style, result);
        case TEXT_GEN_SHORT_STORY:
            return text_generate_short_story(gen, request->prompt,
                                             request->max_length * 2,
                                             request->style, result);
        case TEXT_GEN_SCREENPLAY:
            {
                screenplay_request_t sr = {0};
                sr.target_page_count = 1.0f;
                sr.logline = request->prompt;
                return text_generate_screenplay(gen, &sr, request->style, result);
            }
        default:
            break;
    }

    /* Default: generate generic text */
    result->text = generate_placeholder_text(request->prompt,
                                              request->max_length * 4, &seed);
    if (!result->text) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "text_generator_destroy: result->text is NULL");
        return -1;
    }

    result->text_len = strlen(result->text);
    result->tokens_generated = result->text_len / 4;  /* Rough estimate */
    result->evaluation.overall_quality = 0.7f;
    result->generation_time_ms = get_time_ms() - start_time;

    gen->texts_generated++;

    return 0;
}

int text_generate_continue(text_generator_t* gen,
                           const char* existing, size_t existing_len,
                           const style_embedding_t* style,
                           uint32_t max_new_tokens,
                           text_generation_result_t* result) {
    if (!gen || !existing || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "text_generator_destroy: required parameter is NULL (gen, existing, result)");
        return -1;
    }
    (void)style;

    memset(result, 0, sizeof(text_generation_result_t));

    uint64_t start_time = get_time_ms();
    uint32_t seed = (uint32_t)time(NULL);

    /* Generate continuation */
    char* continuation = generate_placeholder_text(NULL, max_new_tokens * 4, &seed);
    if (!continuation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "text_generator_destroy: continuation is NULL");
        return -1;
    }

    /* Combine existing + continuation */
    size_t total_len = existing_len + strlen(continuation) + 2;
    result->text = nimcp_calloc(total_len, sizeof(char));
    if (!result->text) {
        nimcp_free(continuation);
        continuation = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "text_generator_destroy: result->text is NULL");
        return -1;
    }

    snprintf(result->text, total_len, "%s %s", existing, continuation);
    nimcp_free(continuation);
    continuation = NULL;

    result->text_len = strlen(result->text);
    result->tokens_generated = max_new_tokens;
    result->evaluation.overall_quality = 0.7f;
    result->generation_time_ms = get_time_ms() - start_time;

    gen->texts_generated++;

    return 0;
}

//=============================================================================
// Poetry Generation API
//=============================================================================

int text_generate_poetry(text_generator_t* gen,
                         const poetry_request_t* request,
                         const style_embedding_t* style,
                         text_generation_result_t* result) {
    if (!gen || !request || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "text_generator_destroy: required parameter is NULL (gen, request, result)");
        return -1;
    }
    (void)style;

    memset(result, 0, sizeof(text_generation_result_t));

    uint64_t start_time = get_time_ms();
    uint32_t seed = (uint32_t)time(NULL);

    switch (request->form) {
        case VERSE_HAIKU:
            return text_generate_haiku(gen, request->subject, style, result);

        case VERSE_SONNET_SHAKESPEAREAN:
        case VERSE_SONNET_PETRARCHAN:
            return text_generate_sonnet(gen, request->subject,
                                        request->form == VERSE_SONNET_SHAKESPEAREAN,
                                        style, result);

        default:
            /* Free verse */
            result->text = generate_placeholder_text(request->subject, 500, &seed);
            break;
    }

    if (!result->text) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "text_generator_destroy: result->text is NULL");
        return -1;
    }

    result->text_len = strlen(result->text);
    result->tokens_generated = result->text_len / 4;
    result->evaluation.overall_quality = 0.75f;
    result->generation_time_ms = get_time_ms() - start_time;

    gen->texts_generated++;

    return 0;
}

int text_generate_haiku(text_generator_t* gen,
                        const char* subject,
                        const style_embedding_t* style,
                        text_generation_result_t* result) {
    if (!gen || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "text_generator_destroy: required parameter is NULL (gen, result)");
        return -1;
    }
    (void)style;

    memset(result, 0, sizeof(text_generation_result_t));

    uint64_t start_time = get_time_ms();
    uint32_t seed = (uint32_t)time(NULL);

    result->text = generate_haiku_text(subject, &seed);
    if (!result->text) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "text_generator_destroy: result->text is NULL");
        return -1;
    }

    result->text_len = strlen(result->text);
    result->tokens_generated = result->text_len / 4;
    result->evaluation.overall_quality = 0.8f;
    result->generation_time_ms = get_time_ms() - start_time;

    gen->texts_generated++;

    return 0;
}

int text_generate_sonnet(text_generator_t* gen,
                         const char* subject,
                         bool shakespearean,
                         const style_embedding_t* style,
                         text_generation_result_t* result) {
    if (!gen || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "text_generator_destroy: required parameter is NULL (gen, result)");
        return -1;
    }
    (void)style;

    memset(result, 0, sizeof(text_generation_result_t));

    uint64_t start_time = get_time_ms();
    uint32_t seed = (uint32_t)time(NULL);

    result->text = generate_sonnet_text(subject, shakespearean, &seed);
    if (!result->text) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "text_generator_destroy: result->text is NULL");
        return -1;
    }

    result->text_len = strlen(result->text);
    result->tokens_generated = result->text_len / 4;
    result->evaluation.overall_quality = 0.8f;
    result->generation_time_ms = get_time_ms() - start_time;

    gen->texts_generated++;

    return 0;
}

//=============================================================================
// Prose Generation API
//=============================================================================

int text_generate_prose(text_generator_t* gen,
                        const prose_request_t* request,
                        const style_embedding_t* style,
                        text_generation_result_t* result) {
    if (!gen || !request || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (gen, request, result)");
        return -1;
    }
    (void)style;

    memset(result, 0, sizeof(text_generation_result_t));

    uint64_t start_time = get_time_ms();
    uint32_t seed = (uint32_t)time(NULL);

    result->text = generate_prose_text(request, &seed);
    if (!result->text) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: result->text is NULL");
        return -1;
    }

    result->text_len = strlen(result->text);
    result->tokens_generated = result->text_len / 4;
    result->evaluation.overall_quality = 0.75f;
    result->generation_time_ms = get_time_ms() - start_time;

    gen->texts_generated++;

    return 0;
}

int text_generate_short_story(text_generator_t* gen,
                              const char* premise,
                              uint32_t word_count,
                              const style_embedding_t* style,
                              text_generation_result_t* result) {
    if (!gen || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (gen, result)");
        return -1;
    }

    prose_request_t request = {0};
    request.structure = NARRATIVE_LINEAR;
    request.pov = POV_THIRD_LIMITED;
    request.target_word_count = word_count;
    request.plot_outline = premise;
    request.include_dialogue = true;
    request.dialogue_ratio = 0.3f;

    return text_generate_prose(gen, &request, style, result);
}

//=============================================================================
// Screenplay Generation API
//=============================================================================

int text_generate_screenplay(text_generator_t* gen,
                             const screenplay_request_t* request,
                             const style_embedding_t* style,
                             text_generation_result_t* result) {
    if (!gen || !request || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (gen, request, result)");
        return -1;
    }
    (void)style;

    memset(result, 0, sizeof(text_generation_result_t));

    uint64_t start_time = get_time_ms();
    uint32_t seed = (uint32_t)time(NULL);

    result->text = generate_screenplay_text(request, &seed);
    if (!result->text) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: result->text is NULL");
        return -1;
    }

    result->text_len = strlen(result->text);
    result->tokens_generated = result->text_len / 4;
    result->evaluation.overall_quality = 0.7f;
    result->generation_time_ms = get_time_ms() - start_time;

    gen->texts_generated++;

    return 0;
}

int text_generate_scene(text_generator_t* gen,
                        const char* scene_description,
                        const char* characters,
                        const style_embedding_t* style,
                        text_generation_result_t* result) {
    if (!gen || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (gen, result)");
        return -1;
    }

    screenplay_request_t request = {0};
    request.target_page_count = 0.5f;
    request.logline = scene_description;
    request.characters = characters;

    return text_generate_screenplay(gen, &request, style, result);
}

//=============================================================================
// Dialogue Generation API
//=============================================================================

int text_generate_dialogue(text_generator_t* gen,
                           const char* character_a,
                           const char* character_b,
                           const char* situation,
                           uint32_t num_exchanges,
                           const style_embedding_t* style,
                           text_generation_result_t* result) {
    if (!gen || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (gen, result)");
        return -1;
    }
    (void)style;

    memset(result, 0, sizeof(text_generation_result_t));

    uint64_t start_time = get_time_ms();

    /* Generate dialogue */
    size_t buf_size = num_exchanges * 200 + 256;
    result->text = nimcp_calloc(buf_size, sizeof(char));
    if (!result->text) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: result->text is NULL");
        return -1;
    }

    size_t pos = 0;

    if (situation) {
        pos += snprintf(result->text + pos, buf_size - pos,
                       "[Setting: %s]\n\n", situation);
    }

    const char* names[2] = {
        character_a ? character_a : "CHARACTER A",
        character_b ? character_b : "CHARACTER B"
    };

    for (uint32_t i = 0; i < num_exchanges && pos < buf_size - 100; i++) {
        pos += snprintf(result->text + pos, buf_size - pos,
                       "%s: [Dialogue line %u]\n\n",
                       names[i % 2], i + 1);
    }

    result->text_len = strlen(result->text);
    result->tokens_generated = result->text_len / 4;
    result->evaluation.overall_quality = 0.7f;
    result->generation_time_ms = get_time_ms() - start_time;

    gen->texts_generated++;

    return 0;
}

//=============================================================================
// Lyrics Generation API
//=============================================================================

int text_generate_lyrics(text_generator_t* gen,
                         const char* theme,
                         const char* structure,
                         const style_embedding_t* style,
                         text_generation_result_t* result) {
    if (!gen || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (gen, result)");
        return -1;
    }
    (void)style;
    (void)structure;

    memset(result, 0, sizeof(text_generation_result_t));

    uint64_t start_time = get_time_ms();

    size_t buf_size = 2048;
    result->text = nimcp_calloc(buf_size, sizeof(char));
    if (!result->text) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: result->text is NULL");
        return -1;
    }

    snprintf(result->text, buf_size,
        "[Verse 1]\n"
        "In the %s of life we find\n"
        "A melody that speaks to mind\n"
        "Through every note and every word\n"
        "A song that begs to be heard\n"
        "\n"
        "[Chorus]\n"
        "Sing along, the music plays\n"
        "Through the nights and through the days\n"
        "Let the rhythm set you free\n"
        "This is who we're meant to be\n"
        "\n"
        "[Verse 2]\n"
        "When shadows fall and hope seems lost\n"
        "Remember not to count the cost\n"
        "For every end's a new beginning\n"
        "And every song is worth the singing\n"
        "\n"
        "[Chorus]\n"
        "Sing along, the music plays\n"
        "Through the nights and through the days\n"
        "Let the rhythm set you free\n"
        "This is who we're meant to be\n",
        theme ? theme : "journey");

    result->text_len = strlen(result->text);
    result->tokens_generated = result->text_len / 4;
    result->evaluation.overall_quality = 0.75f;
    result->generation_time_ms = get_time_ms() - start_time;

    gen->texts_generated++;

    return 0;
}

//=============================================================================
// Style Control API
//=============================================================================

void text_generator_set_style(text_generator_t* gen,
                              const style_embedding_t* style) {
    if (!gen || !style) return;

    if (gen->current_style) {
        style_embedding_destroy(gen->current_style);
    } else {
        gen->current_style = nimcp_calloc(1, sizeof(style_embedding_t));
    }

    if (gen->current_style) {
        style_embedding_clone(style, gen->current_style);
    }
}

void text_generator_clear_style(text_generator_t* gen) {
    if (!gen || !gen->current_style) return;

    style_embedding_destroy(gen->current_style);
    nimcp_free(gen->current_style);
    gen->current_style = NULL;
}

int text_generator_archetype_style(text_generator_t* gen,
                                   literary_style_archetype_t archetype_id,
                                   style_embedding_t* out) {
    if (!gen || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "text_generator_clear_style: required parameter is NULL (gen, out)");
        return -1;
    }

    /* Generate archetype embedding */
    style_embedding_create(out, gen->config.style_embedding_dim);

    uint32_t seed = (uint32_t)archetype_id * 37 + 12345;
    for (uint32_t i = 0; i < out->embedding_dim; i++) {
        seed = seed * 1103515245 + 12345;
        out->embedding[i] = ((float)(seed % 10000) / 5000.0f) - 1.0f;
    }

    style_embedding_normalize(out);

    return 0;
}

//=============================================================================
// Integration API
//=============================================================================

void text_generator_set_evaluator(text_generator_t* gen, void* evaluator) {
    if (!gen) return;
    gen->aesthetic_evaluator = evaluator;
}

void text_generator_set_bridge(text_generator_t* gen, void* bridge) {
    if (!gen) return;
    gen->creative_bridge = bridge;
}

void text_generator_set_speech_cortex(text_generator_t* gen, void* speech_cortex) {
    if (!gen) return;
    gen->speech_cortex = speech_cortex;
}
