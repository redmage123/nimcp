/**
 * @file nimcp_parietal_numerical_language.c
 * @brief Numerical Language Processing Module Implementation
 * @version 1.0.0
 * @date 2025-01-31
 */

#include "cognitive/parietal/linguistics/nimcp_parietal_numerical_language.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/fuzzy/nimcp_fuzzy_types.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/logging/nimcp_logging.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stdio.h>
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * INTERNAL CONSTANTS
 * ============================================================================ */

/** Subitizing limit (instant recognition without counting) */
#define SUBITIZING_LIMIT 4

/* ============================================================================
 * NUMBER WORD TABLES
 * ============================================================================ */

/** Units (0-19) */
static const char* g_units[] = {
    "zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine",
    "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen",
    "seventeen", "eighteen", "nineteen"
};

/** Tens (20, 30, ..., 90) */
static const char* g_tens[] = {
    "", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"
};

/** Ordinal units */
static const char* g_ordinal_units[] = {
    "zeroth", "first", "second", "third", "fourth", "fifth", "sixth", "seventh",
    "eighth", "ninth", "tenth", "eleventh", "twelfth", "thirteenth", "fourteenth",
    "fifteenth", "sixteenth", "seventeenth", "eighteenth", "nineteenth"
};

/** Ordinal tens */
static const char* g_ordinal_tens[] = {
    "", "", "twentieth", "thirtieth", "fortieth", "fiftieth", "sixtieth",
    "seventieth", "eightieth", "ninetieth"
};

/** Magnitude words */
static const char* g_magnitudes[] = {
    "", "thousand", "million", "billion", "trillion"
};

/** Quantifier words */
static const char* g_quantifier_words[] = {
    [QUANTIFIER_UNIVERSAL] = "all",
    [QUANTIFIER_EXISTENTIAL] = "some",
    [QUANTIFIER_NEGATIVE] = "none",
    [QUANTIFIER_PROPORTIONAL] = "most"
};

/** Number word type names */
static const char* g_type_names[] = {
    [NUM_WORD_CARDINAL] = "cardinal",
    [NUM_WORD_ORDINAL] = "ordinal",
    [NUM_WORD_MULTIPLIER] = "multiplier",
    [NUM_WORD_FRACTION] = "fraction",
    [NUM_WORD_APPROXIMATE] = "approximate"
};

/* ============================================================================
 * INTERNAL TYPES
 * ============================================================================ */

/**
 * @brief Numerical language processor internal state
 */
struct numerical_language {
    /* Configuration */
    numerical_language_config_t config;

    /* Quantifier definitions */
    quantifier_definition_t quantifiers[LINGUISTICS_MAX_QUANTIFIERS];
    uint32_t num_quantifiers;

    /* Current state */
    float current_precision;
    float inflammation_level;
    float fatigue_level;

    /* Statistics */
    numerical_language_stats_t stats;
};

/* ============================================================================
 * THREAD-LOCAL ERROR STORAGE
 * ============================================================================ */

static _Thread_local char g_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_last_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_last_error, sizeof(g_last_error), fmt, args);
    va_end(args);
}

/* ============================================================================
 * INTERNAL HELPER FUNCTIONS
 * ============================================================================ */

/**
 * @brief Normalize string to lowercase
 */
static void normalize_word(const char* input, char* output, uint32_t max_len) {
    uint32_t i = 0;
    while (input[i] && i < max_len - 1) {
        output[i] = (char)tolower((unsigned char)input[i]);
        i++;
    }
    output[i] = '\0';
}

/**
 * @brief Skip whitespace and hyphens
 */
static const char* skip_separators(const char* s) {
    while (*s && (isspace((unsigned char)*s) || *s == '-')) {
        s++;
    }
    return s;
}

/**
 * @brief Check if string starts with word
 */
static bool starts_with(const char* str, const char* prefix, size_t* len) {
    size_t prefix_len = strlen(prefix);
    if (strncasecmp(str, prefix, prefix_len) == 0) {
        char next = str[prefix_len];
        if (next == '\0' || isspace((unsigned char)next) || next == '-') {
            if (len) *len = prefix_len;
            return true;
        }
    }
    return false;
}

/**
 * @brief Parse a simple number word (0-999)
 */
static int parse_simple_number(const char* word, float* value) {
    char normalized[NIMCP_ID_BUFFER_SIZE];
    normalize_word(word, normalized, sizeof(normalized));

    /* Check units (0-19) */
    for (int i = 0; i < 20; i++) {
        if (strcmp(normalized, g_units[i]) == 0) {
            *value = (float)i;
            return LING_ERR_OK;
        }
    }

    /* Check tens */
    for (int i = 2; i < 10; i++) {
        if (strcmp(normalized, g_tens[i]) == 0) {
            *value = (float)(i * 10);
            return LING_ERR_OK;
        }
    }

    /* Check hundred */
    if (strcmp(normalized, "hundred") == 0) {
        *value = 100.0f;
        return LING_ERR_OK;
    }

    return LING_ERR_UNKNOWN_NUMBER_WORD;
}

/**
 * @brief Convert number less than 1000 to words
 */
static void number_to_words_under_1000(uint32_t n, char* buffer, uint32_t max_len, bool use_hyphen) {
    if (n == 0) {
        strncpy(buffer, "zero", max_len);
        return;
    }

    buffer[0] = '\0';
    char temp[NIMCP_LABEL_BUFFER_SIZE];

    if (n >= 100) {
        uint32_t hundreds = n / 100;
        snprintf(temp, sizeof(temp), "%s hundred", g_units[hundreds]);
        strncat(buffer, temp, max_len - strlen(buffer) - 1);
        n %= 100;
        if (n > 0) {
            strncat(buffer, " ", max_len - strlen(buffer) - 1);
        }
    }

    if (n >= 20) {
        uint32_t tens = n / 10;
        uint32_t units = n % 10;
        strncat(buffer, g_tens[tens], max_len - strlen(buffer) - 1);
        if (units > 0) {
            strncat(buffer, use_hyphen ? "-" : " ", max_len - strlen(buffer) - 1);
            strncat(buffer, g_units[units], max_len - strlen(buffer) - 1);
        }
    } else if (n > 0) {
        strncat(buffer, g_units[n], max_len - strlen(buffer) - 1);
    }
}

/**
 * @brief Convert number less than 1000 to ordinal words
 */
static void number_to_ordinal_under_1000(uint32_t n, char* buffer, uint32_t max_len, bool use_hyphen) {
    if (n == 0) {
        strncpy(buffer, "zeroth", max_len);
        return;
    }

    buffer[0] = '\0';
    char temp[NIMCP_LABEL_BUFFER_SIZE];

    if (n >= 100) {
        uint32_t hundreds = n / 100;
        uint32_t remainder = n % 100;

        if (remainder == 0) {
            snprintf(buffer, max_len, "%s hundredth", g_units[hundreds]);
            return;
        }

        snprintf(temp, sizeof(temp), "%s hundred ", g_units[hundreds]);
        strncat(buffer, temp, max_len - strlen(buffer) - 1);
        n = remainder;
    }

    if (n < 20) {
        strncat(buffer, g_ordinal_units[n], max_len - strlen(buffer) - 1);
    } else {
        uint32_t tens = n / 10;
        uint32_t units = n % 10;

        if (units == 0) {
            strncat(buffer, g_ordinal_tens[tens], max_len - strlen(buffer) - 1);
        } else {
            strncat(buffer, g_tens[tens], max_len - strlen(buffer) - 1);
            strncat(buffer, use_hyphen ? "-" : " ", max_len - strlen(buffer) - 1);
            strncat(buffer, g_ordinal_units[units], max_len - strlen(buffer) - 1);
        }
    }
}

/**
 * @brief Initialize default quantifier definitions
 */
static void init_default_quantifiers(numerical_language_t* nl) {
    /* "all" - universal quantifier */
    nl->quantifiers[0] = (quantifier_definition_t){
        .type = QUANTIFIER_UNIVERSAL,
        .word = "all",
        .proportion_mf = fuzzy_mf_singleton(1.0f)
    };

    /* "every" - universal quantifier variant */
    nl->quantifiers[1] = (quantifier_definition_t){
        .type = QUANTIFIER_UNIVERSAL,
        .word = "every",
        .proportion_mf = fuzzy_mf_singleton(1.0f)
    };

    /* "some" - existential quantifier */
    nl->quantifiers[2] = (quantifier_definition_t){
        .type = QUANTIFIER_EXISTENTIAL,
        .word = "some",
        .proportion_mf = fuzzy_mf_trapezoidal(0.01f, 0.1f, 0.9f, 0.99f)
    };

    /* "none" / "no" - negative quantifier */
    nl->quantifiers[3] = (quantifier_definition_t){
        .type = QUANTIFIER_NEGATIVE,
        .word = "none",
        .proportion_mf = fuzzy_mf_singleton(0.0f)
    };

    nl->quantifiers[4] = (quantifier_definition_t){
        .type = QUANTIFIER_NEGATIVE,
        .word = "no",
        .proportion_mf = fuzzy_mf_singleton(0.0f)
    };

    /* "most" - proportional quantifier (majority) */
    nl->quantifiers[5] = (quantifier_definition_t){
        .type = QUANTIFIER_PROPORTIONAL,
        .word = "most",
        .proportion_mf = fuzzy_mf_s_shaped(0.5f, 0.75f)
    };

    /* "few" - proportional quantifier (minority) */
    nl->quantifiers[6] = (quantifier_definition_t){
        .type = QUANTIFIER_PROPORTIONAL,
        .word = "few",
        .proportion_mf = fuzzy_mf_z_shaped(0.1f, 0.3f)
    };

    /* "many" - proportional quantifier */
    nl->quantifiers[7] = (quantifier_definition_t){
        .type = QUANTIFIER_PROPORTIONAL,
        .word = "many",
        .proportion_mf = fuzzy_mf_s_shaped(0.4f, 0.7f)
    };

    /* "several" - proportional quantifier */
    nl->quantifiers[8] = (quantifier_definition_t){
        .type = QUANTIFIER_PROPORTIONAL,
        .word = "several",
        .proportion_mf = fuzzy_mf_gaussian(0.3f, 0.15f)
    };

    /* "half" - proportional quantifier */
    nl->quantifiers[9] = (quantifier_definition_t){
        .type = QUANTIFIER_PROPORTIONAL,
        .word = "half",
        .proportion_mf = fuzzy_mf_gaussian(0.5f, 0.1f)
    };

    nl->num_quantifiers = 10;
}

/**
 * @brief Find quantifier by word
 */
static const quantifier_definition_t* find_quantifier(
    const numerical_language_t* nl,
    const char* word
) {
    char normalized[NIMCP_ID_BUFFER_SIZE];
    normalize_word(word, normalized, sizeof(normalized));

    for (uint32_t i = 0; i < nl->num_quantifiers; i++) {
        if (strcasecmp(normalized, nl->quantifiers[i].word) == 0) {
            return &nl->quantifiers[i];
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_quantifier: validation failed");
    return NULL;
}

/* ============================================================================
 * LIFECYCLE API IMPLEMENTATION
 * ============================================================================ */

numerical_language_config_t numerical_language_default_config(void) {
    numerical_language_config_t config = {
        .weber_fraction = NUMERICAL_LANG_DEFAULT_WEBER,
        .enable_approximate_mode = true,
        .use_hyphenation = true,
        .use_and_for_tens = false,
        .enable_ordinals = true,
        .enable_fractions = true,
        .enable_multipliers = true,
        .enable_bio_async = false,
        .enable_mesh_participation = true,
        .inflammation_sensitivity = 0.3f,
        .fatigue_sensitivity = 0.2f
    };
    return config;
}

bool numerical_language_validate_config(const numerical_language_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "numerical_language_validate_config: config is NULL");
        return false;
    }

    if (config->weber_fraction <= 0.0f || config->weber_fraction > 1.0f) {
        return false;
    }
    if (config->inflammation_sensitivity < 0.0f || config->inflammation_sensitivity > 1.0f) {
        return false;
    }
    if (config->fatigue_sensitivity < 0.0f || config->fatigue_sensitivity > 1.0f) {
        return false;
    }

    return true;
}

numerical_language_t* numerical_language_create(void) {
    return numerical_language_create_custom(NULL);
}

numerical_language_t* numerical_language_create_custom(const numerical_language_config_t* config) {
    numerical_language_t* nl = (numerical_language_t*)nimcp_calloc(1, sizeof(numerical_language_t));
    if (!nl) {
        set_last_error("Failed to allocate numerical language processor");
        NIMCP_THROW_TO_IMMUNE(LING_ERR_ALLOC_FAILED, "numerical_language_create_custom: allocation failed");
        return NULL;
    }

    /* Apply configuration */
    if (config && numerical_language_validate_config(config)) {
        nl->config = *config;
    } else {
        nl->config = numerical_language_default_config();
    }

    /* Initialize quantifiers */
    init_default_quantifiers(nl);

    /* Initialize state */
    nl->current_precision = 0.8f;
    nl->inflammation_level = 0.0f;
    nl->fatigue_level = 0.0f;

    /* Reset statistics */
    memset(&nl->stats, 0, sizeof(nl->stats));

    return nl;
}

void numerical_language_destroy(numerical_language_t* nl) {
    if (nl) {
        nimcp_free(nl);
        nl = NULL;
    }
}

/* ============================================================================
 * PARSING API IMPLEMENTATION
 * ============================================================================ */

int numerical_language_parse_word(
    numerical_language_t* nl,
    const char* word,
    numerical_semantics_t* out
) {
    NIMCP_CHECK_THROW_IMMUNE(nl != NULL, LING_ERR_NULL_POINTER,
        "numerical_language_parse_word: nl is NULL");
    NIMCP_CHECK_THROW_IMMUNE(word != NULL, LING_ERR_NULL_POINTER,
        "numerical_language_parse_word: word is NULL");
    NIMCP_CHECK_THROW_IMMUNE(out != NULL, LING_ERR_NULL_POINTER,
        "numerical_language_parse_word: out is NULL");

    memset(out, 0, sizeof(*out));
    out->type = NUM_WORD_CARDINAL;

    const char* p = word;
    float total = 0.0f;
    float current_group = 0.0f;  /* Current magnitude group (e.g., 123 of "123 thousand") */
    size_t len = 0;

    p = skip_separators(p);

    while (*p) {
        bool matched = false;

        /* Check units (0-19) */
        for (int i = 0; i < 20 && !matched; i++) {
            if (starts_with(p, g_units[i], &len)) {
                current_group += (float)i;
                p += len;
                matched = true;
            }
        }

        /* Check tens (20-90) */
        for (int i = 2; i < 10 && !matched; i++) {
            if (starts_with(p, g_tens[i], &len)) {
                current_group += (float)(i * 10);
                p += len;
                matched = true;
            }
        }

        /* Check hundred */
        if (!matched && starts_with(p, "hundred", &len)) {
            if (current_group == 0.0f) current_group = 1.0f;
            current_group *= 100.0f;
            p += len;
            matched = true;
        }

        /* Check magnitude words (thousand, million, billion, trillion) */
        for (int mag = 1; mag < 5 && !matched; mag++) {
            if (starts_with(p, g_magnitudes[mag], &len)) {
                if (current_group == 0.0f) current_group = 1.0f;
                float multiplier = 1.0f;
                for (int m = 0; m < mag; m++) multiplier *= 1000.0f;
                current_group *= multiplier;
                total += current_group;
                current_group = 0.0f;
                p += len;
                matched = true;
            }
        }

        /* Skip "and" */
        if (!matched && starts_with(p, "and", &len)) {
            p += len;
            matched = true;
        }

        if (!matched) {
            /* Unknown word */
            if (*p && !isspace((unsigned char)*p) && *p != '-') {
                set_last_error("Unknown number word at: %s", p);
                nl->stats.unknown_words++;
                return LING_ERR_UNKNOWN_NUMBER_WORD;
            }
        }

        p = skip_separators(p);
    }

    total += current_group;
    out->magnitude = total;

    /* Compute Weber-Fechner uncertainty */
    out->uncertainty = total * nl->config.weber_fraction;

    /* Compute confidence (affected by modulation) */
    float confidence = 0.9f;
    confidence -= nl->inflammation_level * nl->config.inflammation_sensitivity;
    confidence -= nl->fatigue_level * nl->config.fatigue_sensitivity;
    out->confidence = fmaxf(0.5f, confidence);

    out->is_approximate = false;

    /* Update statistics */
    nl->stats.words_parsed++;
    nl->stats.avg_confidence = (nl->stats.avg_confidence * (nl->stats.words_parsed - 1)
                               + out->confidence) / nl->stats.words_parsed;

    return LING_ERR_OK;
}

int numerical_language_parse_ordinal(
    numerical_language_t* nl,
    const char* word,
    numerical_semantics_t* out
) {
    NIMCP_CHECK_THROW_IMMUNE(nl != NULL, LING_ERR_NULL_POINTER,
        "numerical_language_parse_ordinal: nl is NULL");
    NIMCP_CHECK_THROW_IMMUNE(word != NULL, LING_ERR_NULL_POINTER,
        "numerical_language_parse_ordinal: word is NULL");
    NIMCP_CHECK_THROW_IMMUNE(out != NULL, LING_ERR_NULL_POINTER,
        "numerical_language_parse_ordinal: out is NULL");

    if (!nl->config.enable_ordinals) {
        set_last_error("Ordinal parsing disabled");
        return LING_ERR_INVALID_PARAM;
    }

    memset(out, 0, sizeof(*out));
    out->type = NUM_WORD_ORDINAL;

    char normalized[NIMCP_ID_BUFFER_SIZE];
    normalize_word(word, normalized, sizeof(normalized));

    /* Check ordinal units (1st-19th) */
    for (int i = 0; i < 20; i++) {
        if (strcmp(normalized, g_ordinal_units[i]) == 0) {
            out->ordinal_position = (uint32_t)i;
            out->magnitude = (float)i;
            out->confidence = 0.95f;
            nl->stats.ordinals_parsed++;
            return LING_ERR_OK;
        }
    }

    /* Check ordinal tens */
    for (int i = 2; i < 10; i++) {
        if (strcmp(normalized, g_ordinal_tens[i]) == 0) {
            out->ordinal_position = (uint32_t)(i * 10);
            out->magnitude = (float)(i * 10);
            out->confidence = 0.95f;
            nl->stats.ordinals_parsed++;
            return LING_ERR_OK;
        }
    }

    /* Check for compound ordinals (e.g., "twenty-third") */
    for (int tens = 2; tens < 10; tens++) {
        for (int units = 1; units < 10; units++) {
            char compound[NIMCP_ID_BUFFER_SIZE];
            snprintf(compound, sizeof(compound), "%s-%s", g_tens[tens], g_ordinal_units[units]);
            if (strcmp(normalized, compound) == 0) {
                out->ordinal_position = (uint32_t)(tens * 10 + units);
                out->magnitude = (float)(tens * 10 + units);
                out->confidence = 0.9f;
                nl->stats.ordinals_parsed++;
                return LING_ERR_OK;
            }
        }
    }

    set_last_error("Unknown ordinal: %s", word);
    nl->stats.unknown_words++;
    return LING_ERR_UNKNOWN_NUMBER_WORD;
}

int numerical_language_parse_fraction(
    numerical_language_t* nl,
    const char* word,
    numerical_semantics_t* out
) {
    NIMCP_CHECK_THROW_IMMUNE(nl != NULL, LING_ERR_NULL_POINTER,
        "numerical_language_parse_fraction: nl is NULL");
    NIMCP_CHECK_THROW_IMMUNE(word != NULL, LING_ERR_NULL_POINTER,
        "numerical_language_parse_fraction: word is NULL");
    NIMCP_CHECK_THROW_IMMUNE(out != NULL, LING_ERR_NULL_POINTER,
        "numerical_language_parse_fraction: out is NULL");

    if (!nl->config.enable_fractions) {
        set_last_error("Fraction parsing disabled");
        return LING_ERR_INVALID_PARAM;
    }

    memset(out, 0, sizeof(*out));
    out->type = NUM_WORD_FRACTION;

    char normalized[NIMCP_ID_BUFFER_SIZE];
    normalize_word(word, normalized, sizeof(normalized));

    /* Common fractions */
    if (strcmp(normalized, "half") == 0 || strcmp(normalized, "one-half") == 0) {
        out->numerator = 1;
        out->denominator = 2;
        out->magnitude = 0.5f;
        out->confidence = 0.95f;
        nl->stats.fractions_parsed++;
        return LING_ERR_OK;
    }

    if (strcmp(normalized, "third") == 0 || strcmp(normalized, "one-third") == 0) {
        out->numerator = 1;
        out->denominator = 3;
        out->magnitude = 1.0f / 3.0f;
        out->confidence = 0.95f;
        nl->stats.fractions_parsed++;
        return LING_ERR_OK;
    }

    if (strcmp(normalized, "quarter") == 0 || strcmp(normalized, "one-quarter") == 0 ||
        strcmp(normalized, "fourth") == 0 || strcmp(normalized, "one-fourth") == 0) {
        out->numerator = 1;
        out->denominator = 4;
        out->magnitude = 0.25f;
        out->confidence = 0.95f;
        nl->stats.fractions_parsed++;
        return LING_ERR_OK;
    }

    if (strcmp(normalized, "three-quarters") == 0 || strcmp(normalized, "three-fourths") == 0) {
        out->numerator = 3;
        out->denominator = 4;
        out->magnitude = 0.75f;
        out->confidence = 0.95f;
        nl->stats.fractions_parsed++;
        return LING_ERR_OK;
    }

    if (strcmp(normalized, "two-thirds") == 0) {
        out->numerator = 2;
        out->denominator = 3;
        out->magnitude = 2.0f / 3.0f;
        out->confidence = 0.95f;
        nl->stats.fractions_parsed++;
        return LING_ERR_OK;
    }

    set_last_error("Unknown fraction: %s", word);
    nl->stats.unknown_words++;
    return LING_ERR_UNKNOWN_NUMBER_WORD;
}

int numerical_language_parse_quantifier(
    numerical_language_t* nl,
    const char* word,
    numerical_semantics_t* out
) {
    NIMCP_CHECK_THROW_IMMUNE(nl != NULL, LING_ERR_NULL_POINTER,
        "numerical_language_parse_quantifier: nl is NULL");
    NIMCP_CHECK_THROW_IMMUNE(word != NULL, LING_ERR_NULL_POINTER,
        "numerical_language_parse_quantifier: word is NULL");
    NIMCP_CHECK_THROW_IMMUNE(out != NULL, LING_ERR_NULL_POINTER,
        "numerical_language_parse_quantifier: out is NULL");

    memset(out, 0, sizeof(*out));

    const quantifier_definition_t* qdef = find_quantifier(nl, word);
    if (!qdef) {
        set_last_error("Unknown quantifier: %s", word);
        nl->stats.unknown_words++;
        return LING_ERR_INVALID_QUANTIFIER;
    }

    out->type = NUM_WORD_APPROXIMATE;
    out->quantifier = qdef->type;
    out->is_approximate = true;

    /* For proportional quantifiers, set approximate magnitude */
    switch (qdef->type) {
        case QUANTIFIER_UNIVERSAL:
            out->magnitude = 1.0f;
            break;
        case QUANTIFIER_NEGATIVE:
            out->magnitude = 0.0f;
            break;
        case QUANTIFIER_EXISTENTIAL:
            out->magnitude = 0.5f;  /* Vague */
            break;
        case QUANTIFIER_PROPORTIONAL:
            /* Use center of MF */
            if (qdef->proportion_mf.type == FUZZY_MF_GAUSSIAN) {
                out->magnitude = qdef->proportion_mf.params[0];
            } else if (qdef->proportion_mf.type == FUZZY_MF_S_SHAPED) {
                out->magnitude = (qdef->proportion_mf.params[0] + qdef->proportion_mf.params[1]) / 2.0f;
            } else if (qdef->proportion_mf.type == FUZZY_MF_Z_SHAPED) {
                out->magnitude = (qdef->proportion_mf.params[0] + qdef->proportion_mf.params[1]) / 2.0f;
            } else {
                out->magnitude = 0.5f;
            }
            break;
        default:
            out->magnitude = 0.5f;
            break;
    }

    out->uncertainty = 0.2f;  /* High uncertainty for vague quantifiers */
    out->confidence = 0.8f;

    nl->stats.quantifiers_parsed++;

    return LING_ERR_OK;
}

bool numerical_language_is_number_word(
    const numerical_language_t* nl,
    const char* word
) {
    if (!nl || !word) {
        return false;
    }

    numerical_semantics_t dummy;

    /* Try cardinal */
    if (numerical_language_parse_word((numerical_language_t*)nl, word, &dummy) == LING_ERR_OK) {
        return true;
    }

    /* Try ordinal */
    if (numerical_language_parse_ordinal((numerical_language_t*)nl, word, &dummy) == LING_ERR_OK) {
        return true;
    }

    /* Try quantifier */
    if (find_quantifier(nl, word) != NULL) {
        return true;
    }

    return false;
}

/* ============================================================================
 * GENERATION API IMPLEMENTATION
 * ============================================================================ */

int numerical_language_generate_word(
    numerical_language_t* nl,
    float number,
    char* word,
    uint32_t max_len
) {
    NIMCP_CHECK_THROW_IMMUNE(nl != NULL, LING_ERR_NULL_POINTER,
        "numerical_language_generate_word: nl is NULL");
    NIMCP_CHECK_THROW_IMMUNE(word != NULL, LING_ERR_NULL_POINTER,
        "numerical_language_generate_word: word is NULL");
    NIMCP_CHECK_THROW_IMMUNE(max_len > 0, LING_ERR_INVALID_PARAM,
        "numerical_language_generate_word: max_len is 0");

    if (number < 0.0f || number > NUMERICAL_LANG_MAX_VALUE) {
        set_last_error("Number out of range: %f", number);
        return LING_ERR_INVALID_PARAM;
    }

    word[0] = '\0';

    /* Round to integer */
    uint64_t n = (uint64_t)(number + 0.5f);

    if (n == 0) {
        strncpy(word, "zero", max_len);
        nl->stats.words_generated++;
        return LING_ERR_OK;
    }

    char temp[NIMCP_ERROR_BUFFER_SIZE] = {0};
    char group_word[NIMCP_ID_BUFFER_SIZE];

    /* Process each magnitude group */
    int magnitude = 4;  /* trillion */
    uint64_t divisor = 1000000000000ULL;

    while (magnitude >= 0) {
        uint64_t group = n / (divisor > 0 ? divisor : 1);
        n %= divisor;

        if (group > 0) {
            number_to_words_under_1000((uint32_t)group, group_word, sizeof(group_word),
                                       nl->config.use_hyphenation);

            if (temp[0]) {
                strncat(temp, " ", sizeof(temp) - strlen(temp) - 1);
            }
            strncat(temp, group_word, sizeof(temp) - strlen(temp) - 1);

            if (magnitude > 0) {
                strncat(temp, " ", sizeof(temp) - strlen(temp) - 1);
                strncat(temp, g_magnitudes[magnitude], sizeof(temp) - strlen(temp) - 1);
            }
        }

        divisor /= 1000;
        magnitude--;
    }

    strncpy(word, temp, max_len - 1);
    word[max_len - 1] = '\0';

    nl->stats.words_generated++;

    return LING_ERR_OK;
}

int numerical_language_generate_ordinal(
    numerical_language_t* nl,
    uint32_t position,
    char* word,
    uint32_t max_len
) {
    NIMCP_CHECK_THROW_IMMUNE(nl != NULL, LING_ERR_NULL_POINTER,
        "numerical_language_generate_ordinal: nl is NULL");
    NIMCP_CHECK_THROW_IMMUNE(word != NULL, LING_ERR_NULL_POINTER,
        "numerical_language_generate_ordinal: word is NULL");
    NIMCP_CHECK_THROW_IMMUNE(max_len > 0, LING_ERR_INVALID_PARAM,
        "numerical_language_generate_ordinal: max_len is 0");

    if (position == 0 || position > 999) {
        /* For larger numbers, just append "th" to cardinal */
        int result = numerical_language_generate_word(nl, (float)position, word, max_len - 2);
        if (result == LING_ERR_OK && position > 999) {
            strncat(word, "th", max_len - strlen(word) - 1);
        }
        return result;
    }

    number_to_ordinal_under_1000(position, word, max_len, nl->config.use_hyphenation);

    return LING_ERR_OK;
}

/* ============================================================================
 * FUZZY QUANTIFIER API IMPLEMENTATION
 * ============================================================================ */

float numerical_language_quantifier_evaluate(
    const numerical_language_t* nl,
    linguistic_quantifier_t quantifier,
    float proportion
) {
    if (!nl) return 0.0f;

    if (proportion < 0.0f) proportion = 0.0f;
    if (proportion > 1.0f) proportion = 1.0f;

    /* Find matching quantifier */
    for (uint32_t i = 0; i < nl->num_quantifiers; i++) {
        if (nl->quantifiers[i].type == quantifier) {
            return fuzzy_mf_evaluate(&nl->quantifiers[i].proportion_mf, proportion);
        }
    }

    return 0.0f;
}

int numerical_language_get_quantifier_mf(
    const numerical_language_t* nl,
    linguistic_quantifier_t quantifier,
    fuzzy_mf_t* out
) {
    NIMCP_CHECK_THROW_IMMUNE(nl != NULL, LING_ERR_NULL_POINTER,
        "numerical_language_get_quantifier_mf: nl is NULL");
    NIMCP_CHECK_THROW_IMMUNE(out != NULL, LING_ERR_NULL_POINTER,
        "numerical_language_get_quantifier_mf: out is NULL");

    for (uint32_t i = 0; i < nl->num_quantifiers; i++) {
        if (nl->quantifiers[i].type == quantifier) {
            *out = nl->quantifiers[i].proportion_mf;
            return LING_ERR_OK;
        }
    }

    return LING_ERR_INVALID_QUANTIFIER;
}

int numerical_language_select_quantifier(
    const numerical_language_t* nl,
    float proportion,
    linguistic_quantifier_t* out
) {
    NIMCP_CHECK_THROW_IMMUNE(nl != NULL, LING_ERR_NULL_POINTER,
        "numerical_language_select_quantifier: nl is NULL");
    NIMCP_CHECK_THROW_IMMUNE(out != NULL, LING_ERR_NULL_POINTER,
        "numerical_language_select_quantifier: out is NULL");

    if (proportion < 0.0f) proportion = 0.0f;
    if (proportion > 1.0f) proportion = 1.0f;

    float best_membership = -1.0f;
    linguistic_quantifier_t best_quantifier = QUANTIFIER_EXISTENTIAL;

    for (uint32_t i = 0; i < nl->num_quantifiers; i++) {
        float membership = fuzzy_mf_evaluate(&nl->quantifiers[i].proportion_mf, proportion);
        if (membership > best_membership) {
            best_membership = membership;
            best_quantifier = nl->quantifiers[i].type;
        }
    }

    *out = best_quantifier;
    return LING_ERR_OK;
}

/* ============================================================================
 * NUMBER SENSE INTEGRATION API IMPLEMENTATION
 * ============================================================================ */

float numerical_language_get_uncertainty(
    const numerical_language_t* nl,
    float magnitude
) {
    if (!nl) return 0.0f;
    if (magnitude < 0.0f) magnitude = -magnitude;
    return magnitude * nl->config.weber_fraction;
}

bool numerical_language_is_subitizable(
    const numerical_language_t* nl,
    float magnitude
) {
    (void)nl;
    return magnitude >= 1.0f && magnitude <= (float)SUBITIZING_LIMIT;
}

/* ============================================================================
 * MESH INTEGRATION API IMPLEMENTATION
 * ============================================================================ */

int numerical_language_mesh_process(
    numerical_language_t* nl,
    const linguistics_request_t* request,
    linguistics_belief_t* belief
) {
    NIMCP_CHECK_THROW_IMMUNE(nl != NULL, LING_ERR_NULL_POINTER,
        "numerical_language_mesh_process: nl is NULL");
    NIMCP_CHECK_THROW_IMMUNE(request != NULL, LING_ERR_NULL_POINTER,
        "numerical_language_mesh_process: request is NULL");
    NIMCP_CHECK_THROW_IMMUNE(belief != NULL, LING_ERR_NULL_POINTER,
        "numerical_language_mesh_process: belief is NULL");

    if (request->type != LING_REQUEST_PARSE_NUMBER &&
        request->type != LING_REQUEST_GENERATE_NUMBER_WORD) {
        return LING_ERR_INVALID_PARAM;
    }

    numerical_semantics_t semantics;
    int result = 0;

    if (request->type == LING_REQUEST_PARSE_NUMBER) {
        result = numerical_language_parse_word(nl, request->input_word, &semantics);
    } else {
        char word[NIMCP_LABEL_BUFFER_SIZE];
        result = numerical_language_generate_word(nl, request->input_magnitude, word, sizeof(word));
        if (result == LING_ERR_OK) {
            semantics.magnitude = request->input_magnitude;
            semantics.confidence = 0.95f;
        }
    }

    /* Populate belief */
    belief->belief_id = (uint32_t)(request->request_id & 0xFFFFFFFF);
    belief->source_module_id = BIO_MODULE_NUMERICAL_LANGUAGE;
    snprintf(belief->topic, sizeof(belief->topic),
             "numerical_%s", request->input_word);

    if (result == LING_ERR_OK) {
        belief->certainty = semantics.confidence;
        belief->precision = nl->current_precision;

        /* Encode semantics into belief vector */
        belief->vector_dim = 6;
        belief->belief_vector[0] = semantics.magnitude / NUMERICAL_LANG_MAX_VALUE;
        belief->belief_vector[1] = semantics.uncertainty / NUMERICAL_LANG_MAX_VALUE;
        belief->belief_vector[2] = (float)semantics.type / (NUM_WORD_TYPE_COUNT > 0 ? NUM_WORD_TYPE_COUNT : 1);
        belief->belief_vector[3] = semantics.is_approximate ? 1.0f : 0.0f;
        belief->belief_vector[4] = (float)semantics.ordinal_position / 1000.0f;
        belief->belief_vector[5] = semantics.confidence;
    } else {
        belief->certainty = 0.0f;
        belief->precision = LINGUISTICS_PRECISION_FLOOR;
        belief->vector_dim = 0;
    }

    belief->timestamp_ms = request->timestamp_ms;

    return result;
}

int numerical_language_mesh_update(
    numerical_language_t* nl,
    const linguistics_belief_t* neighbor_beliefs,
    uint32_t neighbor_count,
    linguistics_belief_t* updated_belief
) {
    NIMCP_CHECK_THROW_IMMUNE(nl != NULL, LING_ERR_NULL_POINTER,
        "numerical_language_mesh_update: nl is NULL");
    NIMCP_CHECK_THROW_IMMUNE(updated_belief != NULL, LING_ERR_NULL_POINTER,
        "numerical_language_mesh_update: updated_belief is NULL");

    if (neighbor_count == 0 || !neighbor_beliefs) {
        return LING_ERR_OK;
    }

    float lr = LINGUISTICS_FEP_LEARNING_RATE;

    for (uint32_t i = 0; i < neighbor_count; i++) {
        const linguistics_belief_t* neighbor = &neighbor_beliefs[i];

        if (neighbor->vector_dim == 0 ||
            neighbor->vector_dim != updated_belief->vector_dim) {
            continue;
        }

        float precision = fminf(neighbor->precision, LINGUISTICS_PRECISION_CEILING);
        precision = fmaxf(precision, LINGUISTICS_PRECISION_FLOOR);

        for (uint32_t j = 0; j < updated_belief->vector_dim; j++) {
            float error = neighbor->belief_vector[j] - updated_belief->belief_vector[j];
            float delta = lr * precision * error;
            updated_belief->belief_vector[j] += delta;
        }

        float weight = precision / (precision + updated_belief->precision);
        updated_belief->certainty = (1.0f - weight) * updated_belief->certainty +
                                     weight * neighbor->certainty;
    }

    return LING_ERR_OK;
}

float numerical_language_get_precision(const numerical_language_t* nl) {
    if (!nl) return LINGUISTICS_PRECISION_FLOOR;

    float precision = nl->current_precision;
    precision -= nl->inflammation_level * nl->config.inflammation_sensitivity;
    precision -= nl->fatigue_level * nl->config.fatigue_sensitivity;

    precision = fmaxf(precision, LINGUISTICS_PRECISION_FLOOR);
    precision = fminf(precision, LINGUISTICS_PRECISION_CEILING);

    return precision;
}

int numerical_language_get_mesh_handler(
    numerical_language_t* nl,
    linguistics_mesh_handler_t* handler
) {
    NIMCP_CHECK_THROW_IMMUNE(nl != NULL, LING_ERR_NULL_POINTER,
        "numerical_language_get_mesh_handler: nl is NULL");
    NIMCP_CHECK_THROW_IMMUNE(handler != NULL, LING_ERR_NULL_POINTER,
        "numerical_language_get_mesh_handler: handler is NULL");

    handler->process = (int (*)(void*, const linguistics_request_t*, linguistics_belief_t*))
                       numerical_language_mesh_process;
    handler->update = (int (*)(void*, const linguistics_belief_t*, uint32_t, linguistics_belief_t*))
                      numerical_language_mesh_update;
    handler->get_precision = (float (*)(void*))numerical_language_get_precision;
    handler->ctx = nl;

    return LING_ERR_OK;
}

/* ============================================================================
 * MODULATION API IMPLEMENTATION
 * ============================================================================ */

int numerical_language_set_inflammation(
    numerical_language_t* nl,
    float level
) {
    NIMCP_CHECK_THROW_IMMUNE(nl != NULL, LING_ERR_NULL_POINTER,
        "numerical_language_set_inflammation: nl is NULL");

    if (level < 0.0f || level > 1.0f) {
        set_last_error("Inflammation level must be in [0,1]: %f", level);
        return LING_ERR_INVALID_PARAM;
    }

    nl->inflammation_level = level;
    return LING_ERR_OK;
}

int numerical_language_set_fatigue(
    numerical_language_t* nl,
    float level
) {
    NIMCP_CHECK_THROW_IMMUNE(nl != NULL, LING_ERR_NULL_POINTER,
        "numerical_language_set_fatigue: nl is NULL");

    if (level < 0.0f || level > 1.0f) {
        set_last_error("Fatigue level must be in [0,1]: %f", level);
        return LING_ERR_INVALID_PARAM;
    }

    nl->fatigue_level = level;
    return LING_ERR_OK;
}

/* ============================================================================
 * STATISTICS API IMPLEMENTATION
 * ============================================================================ */

int numerical_language_get_stats(
    const numerical_language_t* nl,
    numerical_language_stats_t* stats
) {
    NIMCP_CHECK_THROW_IMMUNE(nl != NULL, LING_ERR_NULL_POINTER,
        "numerical_language_get_stats: nl is NULL");
    NIMCP_CHECK_THROW_IMMUNE(stats != NULL, LING_ERR_NULL_POINTER,
        "numerical_language_get_stats: stats is NULL");

    *stats = nl->stats;
    return LING_ERR_OK;
}

void numerical_language_reset_stats(numerical_language_t* nl) {
    if (nl) {
        memset(&nl->stats, 0, sizeof(nl->stats));
    }
}

const char* numerical_language_get_last_error(void) {
    return g_last_error;
}

/* ============================================================================
 * UTILITY API IMPLEMENTATION
 * ============================================================================ */

const char* numerical_language_type_name(number_word_type_t type) {
    if (type >= NUM_WORD_TYPE_COUNT) {
        return "unknown";
    }
    return g_type_names[type];
}

const char* numerical_language_quantifier_name(linguistic_quantifier_t quantifier) {
    if (quantifier >= QUANTIFIER_COUNT) {
        return "unknown";
    }
    return g_quantifier_words[quantifier];
}
