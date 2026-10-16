/**
 * @file nimcp_toxicity_response.c
 * @brief Counterclaim generator — see header for design.
 */
#include "security/nimcp_toxicity_response.h"

#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"

#include <ctype.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG_MODULE "toxicity_response"

#define TR_CATEGORY_LEN  32
#define TR_TEMPLATE_LEN  512
#define TR_TOKEN_LEN     64

typedef struct {
    char category[TR_CATEGORY_LEN];   /* "*" matches any */
    int  stage;
    char template_text[TR_TEMPLATE_LEN];
} tr_template_t;

typedef struct {
    char toxic_word[TR_TOKEN_LEN];   /* lowercased, whitespace-trimmed */
    char counter_word[TR_TOKEN_LEN];
    int  multi_word;                 /* 1 if toxic_word contains spaces */
} tr_antiframe_t;

struct toxicity_response {
    tr_template_t*   templates;
    size_t           num_templates;
    size_t           cap_templates;

    tr_antiframe_t*  antiframes;
    size_t           num_antiframes;
    size_t           cap_antiframes;

    nimcp_mutex_t*   reload_mutex;

    _Atomic uint64_t total_generations;
    _Atomic uint64_t total_template_hits;
    _Atomic uint64_t total_antiframe_hits;
    _Atomic uint64_t total_no_match;
};

/* ---------- helpers --------------------------------------------------- */

static char* trim(char* s) {
    if (!s) return s;
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s) return s;
    char* end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) { *end = '\0'; end--; }
    return s;
}

static void str_tolower(char* s) {
    if (!s) return;
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

/* Strip surrounding quotes (one pair). */
static void unquote_inplace(char* s) {
    if (!s || !*s) return;
    size_t L = strlen(s);
    if (L >= 2 && ((s[0] == '"' && s[L-1] == '"') ||
                   (s[0] == '\'' && s[L-1] == '\''))) {
        s[L-1] = '\0';
        memmove(s, s + 1, L - 1);
    }
}

static int ensure_template_cap(toxicity_response_t* tr, size_t need) {
    if (need <= tr->cap_templates) return 0;
    size_t nc = tr->cap_templates ? tr->cap_templates : 16;
    while (nc < need) nc *= 2;
    tr_template_t* np = (tr_template_t*)nimcp_calloc(nc, sizeof(*np));
    if (!np) return -1;
    if (tr->templates) {
        memcpy(np, tr->templates, tr->cap_templates * sizeof(*np));
        nimcp_free(tr->templates);
    }
    tr->templates = np;
    tr->cap_templates = nc;
    return 0;
}

static int ensure_antiframe_cap(toxicity_response_t* tr, size_t need) {
    if (need <= tr->cap_antiframes) return 0;
    size_t nc = tr->cap_antiframes ? tr->cap_antiframes : 64;
    while (nc < need) nc *= 2;
    tr_antiframe_t* np = (tr_antiframe_t*)nimcp_calloc(nc, sizeof(*np));
    if (!np) return -1;
    if (tr->antiframes) {
        memcpy(np, tr->antiframes, tr->cap_antiframes * sizeof(*np));
        nimcp_free(tr->antiframes);
    }
    tr->antiframes = np;
    tr->cap_antiframes = nc;
    return 0;
}

/* Parse one TSV line (category \t stage \t template \t notes-ignored).
 * Returns 0 on success, -1 on malformed. */
static int parse_template_line(char* line, toxicity_response_t* tr) {
    /* Skip blanks and comments. */
    char* p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (!*p || *p == '\n' || *p == '#') return 0;

    /* Strip trailing newline. */
    size_t L = strlen(line);
    while (L > 0 && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = '\0';
    if (L == 0) return 0;

    /* Split on tabs. We need at least 3 fields; field 4 (notes) optional. */
    char* fields[4] = {NULL, NULL, NULL, NULL};
    int n = 0;
    char* cur = line;
    fields[n++] = cur;
    while (*cur && n < 4) {
        if (*cur == '\t') {
            *cur = '\0'; cur++;
            if (n < 4) fields[n++] = cur;
            continue;
        }
        cur++;
    }
    if (n < 3) return -1;

    if (ensure_template_cap(tr, tr->num_templates + 1) != 0) return -1;
    tr_template_t* t = &tr->templates[tr->num_templates];
    memset(t, 0, sizeof(*t));
    strncpy(t->category, fields[0], sizeof(t->category) - 1);
    t->stage = atoi(fields[1]);
    if (t->stage < 0) t->stage = 0;
    strncpy(t->template_text, fields[2], sizeof(t->template_text) - 1);
    tr->num_templates++;
    return 0;
}

static int parse_antiframe_line(char* line, toxicity_response_t* tr) {
    char* p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (!*p || *p == '\n' || *p == '#') return 0;

    size_t L = strlen(line);
    while (L > 0 && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = '\0';
    if (L == 0) return 0;

    char* tab = strchr(line, '\t');
    if (!tab) return -1;
    *tab = '\0';
    char* tw = trim(line);
    char* cw = trim(tab + 1);
    if (!*tw || !*cw) return -1;

    /* Allow second column to have a trailing comment column (third tab). */
    char* third_tab = strchr(cw, '\t');
    if (third_tab) *third_tab = '\0';
    cw = trim(cw);

    unquote_inplace(tw);
    unquote_inplace(cw);
    if (!*tw || !*cw) return -1;

    if (ensure_antiframe_cap(tr, tr->num_antiframes + 1) != 0) return -1;
    tr_antiframe_t* a = &tr->antiframes[tr->num_antiframes];
    memset(a, 0, sizeof(*a));
    strncpy(a->toxic_word, tw, sizeof(a->toxic_word) - 1);
    strncpy(a->counter_word, cw, sizeof(a->counter_word) - 1);
    str_tolower(a->toxic_word);
    /* leave counter_word in original case so capitalization survives */
    a->multi_word = (strchr(a->toxic_word, ' ') != NULL) ? 1 : 0;
    tr->num_antiframes++;
    return 0;
}

static int load_templates_file(const char* path, toxicity_response_t* tr) {
    if (!path) return 0;
    FILE* f = fopen(path, "r");
    if (!f) {
        LOG_MODULE_WARN("toxicity_response",
                        "counterclaims file %s missing — template tier disabled",
                        path);
        return 0;
    }
    char line[1024];
    int n = 0, errs = 0;
    while (fgets(line, sizeof(line), f)) {
        if (parse_template_line(line, tr) == 0) {
            n++;
        } else {
            errs++;
        }
    }
    fclose(f);
    LOG_MODULE_INFO("toxicity_response",
                    "loaded %zu template(s) from %s (%d parse warning(s))",
                    tr->num_templates, path, errs);
    (void)n;
    return 0;
}

static int load_antiframes_file(const char* path, toxicity_response_t* tr) {
    if (!path) return 0;
    FILE* f = fopen(path, "r");
    if (!f) {
        LOG_MODULE_WARN("toxicity_response",
                        "antiframes file %s missing — anti-frame tier disabled",
                        path);
        return 0;
    }
    char line[1024];
    int errs = 0;
    while (fgets(line, sizeof(line), f)) {
        if (parse_antiframe_line(line, tr) != 0) errs++;
    }
    fclose(f);
    LOG_MODULE_INFO("toxicity_response",
                    "loaded %zu antiframe(s) from %s (%d parse warning(s))",
                    tr->num_antiframes, path, errs);
    return 0;
}

/* Extract demographic group tokens from the toxic text. Returns 0..N
 * lowercased group nouns found. group_out is space-separated, capped at
 * out_cap. Best-effort — uses a small enumerated list. */
static void
extract_group(const char* text, char* group_out, size_t out_cap)
{
    if (!group_out || out_cap == 0) return;
    group_out[0] = '\0';
    if (!text) return;

    static const char* GROUPS[] = {
        "jews","muslims","christians","hindus","buddhists","catholics","protestants",
        "blacks","whites","asians","latinos","hispanics","arabs","africans",
        "americans","europeans","mexicans","chinese","japanese","koreans","indians",
        "pakistanis","nigerians",
        "gays","lesbians","trans","transgender","queers",
        "women","girls","ladies","females","wives",
        "men","boys","males",
        "kids","children","babies",
        "immigrants","refugees","disabled","elderly",
        NULL
    };

    /* lowercased copy */
    size_t L = strlen(text);
    char* lc = (char*)nimcp_malloc(L + 1);
    if (!lc) return;
    for (size_t i = 0; i < L; i++) lc[i] = (char)tolower((unsigned char)text[i]);
    lc[L] = '\0';

    size_t pos = 0;
    int found = 0;
    for (int i = 0; GROUPS[i]; i++) {
        const char* g = GROUPS[i];
        size_t gL = strlen(g);
        /* whole-word match: scan for occurrences with word boundaries */
        const char* p = lc;
        while ((p = strstr(p, g)) != NULL) {
            int left_ok  = (p == lc) || !isalnum((unsigned char)*(p-1));
            int right_ok = !isalnum((unsigned char)*(p + gL));
            if (left_ok && right_ok) {
                if (pos + gL + 2 < out_cap) {
                    if (pos > 0) group_out[pos++] = ' ';
                    memcpy(group_out + pos, g, gL);
                    pos += gL;
                    group_out[pos] = '\0';
                    found++;
                }
                break;  /* one occurrence per group */
            }
            p += gL;
        }
        if (found >= 3) break;  /* cap */
    }
    nimcp_free(lc);
    if (group_out[0] == '\0') {
        strncpy(group_out, "they", out_cap - 1);
        group_out[out_cap - 1] = '\0';
    }
}

/* Substitute {group} placeholders in template_text with the group string.
 * Output written to dst (capped). Returns number of substitutions made. */
static int
substitute_group(const char* template_text, const char* group,
                  char* dst, size_t dst_cap)
{
    if (!template_text || !dst || dst_cap == 0) return 0;
    if (!group) group = "they";
    const char* p = template_text;
    size_t pos = 0;
    int subs = 0;
    while (*p && pos + 1 < dst_cap) {
        if (p[0] == '{' && p[1] == 'g' && p[2] == 'r' && p[3] == 'o' &&
            p[4] == 'u' && p[5] == 'p' && p[6] == '}') {
            size_t gL = strlen(group);
            if (pos + gL >= dst_cap) gL = dst_cap - pos - 1;
            memcpy(dst + pos, group, gL);
            pos += gL;
            p += 7;
            subs++;
        } else {
            dst[pos++] = *p++;
        }
    }
    dst[pos] = '\0';
    return subs;
}

/* Template lookup: returns index of best matching row or -1.
 * Match logic: rows with category == matched_category OR "*", whose
 * stage <= current_stage. Among those, choose the row with the highest
 * stage value (most specific). Tie-break: non-"*" beats "*". */
static int
find_best_template(const toxicity_response_t* tr,
                    const char* matched_category, int current_stage)
{
    if (!tr || !matched_category) return -1;
    int best_idx = -1;
    int best_stage = -1;
    int best_is_wild = 1;
    for (size_t i = 0; i < tr->num_templates; i++) {
        const tr_template_t* t = &tr->templates[i];
        int is_wild = (t->category[0] == '*' && t->category[1] == '\0');
        int cat_match = is_wild || (strcmp(t->category, matched_category) == 0);
        if (!cat_match) continue;
        if (t->stage > current_stage) continue;
        /* Prefer higher stage; on tie, prefer non-wild. */
        if (t->stage > best_stage ||
            (t->stage == best_stage && best_is_wild && !is_wild)) {
            best_stage = t->stage;
            best_is_wild = is_wild;
            best_idx = (int)i;
        }
    }
    return best_idx;
}

/* Anti-frame swap. Tokenizes text by whitespace, for each token tries
 * single-word swap; for multi-word phrases scans the input for exact
 * matches and applies the swap once per match. Returns swap count. */
static int
apply_antiframe_swap(const toxicity_response_t* tr,
                      const char* text,
                      char* dst, size_t dst_cap)
{
    if (!tr || !text || !dst || dst_cap == 0) return 0;
    if (tr->num_antiframes == 0) {
        strncpy(dst, text, dst_cap - 1);
        dst[dst_cap - 1] = '\0';
        return 0;
    }

    /* Phase 1: working buffer = input. */
    size_t L = strlen(text);
    char* buf = (char*)nimcp_malloc(L + 1);
    if (!buf) return 0;
    memcpy(buf, text, L + 1);

    /* Phase 2: multi-word swaps first (longer patterns first to avoid
     * partial matches). Simple n^2 scan; the antiframe set is small. */
    int swaps = 0;
    /* For each multi-word antiframe, find all case-insensitive occurrences
     * in buf and rebuild. */
    char tmp[1024];
    for (size_t a = 0; a < tr->num_antiframes; a++) {
        const tr_antiframe_t* af = &tr->antiframes[a];
        if (!af->multi_word) continue;
        size_t toxL = strlen(af->toxic_word);
        size_t bufL = strlen(buf);
        size_t out = 0;
        size_t i = 0;
        int hit_this = 0;
        while (i < bufL && out + 1 < sizeof(tmp)) {
            /* case-insensitive compare at i */
            int match = 1;
            if (i + toxL > bufL) match = 0;
            for (size_t k = 0; match && k < toxL; k++) {
                if (tolower((unsigned char)buf[i + k]) !=
                    (unsigned char)af->toxic_word[k]) match = 0;
            }
            /* word-boundary check */
            if (match) {
                int left_ok  = (i == 0) || !isalnum((unsigned char)buf[i-1]);
                int right_ok = (i + toxL >= bufL) ||
                               !isalnum((unsigned char)buf[i + toxL]);
                if (!left_ok || !right_ok) match = 0;
            }
            if (match) {
                size_t cwL = strlen(af->counter_word);
                if (out + cwL >= sizeof(tmp)) break;
                memcpy(tmp + out, af->counter_word, cwL);
                out += cwL;
                i += toxL;
                hit_this = 1;
            } else {
                tmp[out++] = buf[i++];
            }
        }
        if (out < sizeof(tmp)) tmp[out] = '\0';
        else tmp[sizeof(tmp) - 1] = '\0';
        if (hit_this) {
            strncpy(buf, tmp, L + 1);
            swaps++;
        }
    }

    /* Phase 3: single-word swaps. Tokenize buf by whitespace, look up each
     * token (lowercased) in the antiframe table; emit replacement or
     * original. */
    size_t out = 0;
    size_t i = 0;
    size_t bufL2 = strlen(buf);
    while (i < bufL2 && out + 1 < dst_cap) {
        /* copy whitespace verbatim */
        while (i < bufL2 && isspace((unsigned char)buf[i])) {
            if (out + 1 < dst_cap) dst[out++] = buf[i];
            i++;
        }
        /* token = [i, j) */
        size_t j = i;
        while (j < bufL2 && !isspace((unsigned char)buf[j])) j++;
        if (j == i) break;
        size_t tlen = j - i;

        /* strip trailing punctuation for matching */
        size_t core_end = j;
        while (core_end > i && ispunct((unsigned char)buf[core_end - 1]) &&
               buf[core_end - 1] != '\'') core_end--;
        size_t core_len = core_end - i;

        /* lower-cased copy */
        char lc[TR_TOKEN_LEN];
        if (core_len < sizeof(lc)) {
            for (size_t k = 0; k < core_len; k++)
                lc[k] = (char)tolower((unsigned char)buf[i + k]);
            lc[core_len] = '\0';
        } else {
            lc[0] = '\0';
        }

        int matched = 0;
        if (lc[0] != '\0') {
            for (size_t a = 0; a < tr->num_antiframes; a++) {
                const tr_antiframe_t* af = &tr->antiframes[a];
                if (af->multi_word) continue;
                if (strcmp(lc, af->toxic_word) == 0) {
                    size_t cwL = strlen(af->counter_word);
                    if (out + cwL + (j - core_end) >= dst_cap) break;
                    memcpy(dst + out, af->counter_word, cwL);
                    out += cwL;
                    /* append trailing punctuation that was on the token */
                    for (size_t k = core_end; k < j; k++) {
                        if (out + 1 >= dst_cap) break;
                        dst[out++] = buf[k];
                    }
                    matched = 1;
                    swaps++;
                    break;
                }
            }
        }
        if (!matched) {
            /* copy token verbatim */
            for (size_t k = i; k < j && out + 1 < dst_cap; k++) dst[out++] = buf[k];
        }
        i = j;
    }
    dst[out < dst_cap ? out : dst_cap - 1] = '\0';
    nimcp_free(buf);
    return swaps;
}

/* ---------- public API ----------------------------------------------- */

toxicity_response_t*
toxicity_response_create(const char* counterclaims_path,
                          const char* antiframes_path)
{
    toxicity_response_t* tr = (toxicity_response_t*)nimcp_calloc(1, sizeof(*tr));
    if (!tr) return NULL;
    tr->reload_mutex = nimcp_mutex_create(NULL);
    if (!tr->reload_mutex) {
        nimcp_free(tr);
        return NULL;
    }
    atomic_init(&tr->total_generations, 0ULL);
    atomic_init(&tr->total_template_hits, 0ULL);
    atomic_init(&tr->total_antiframe_hits, 0ULL);
    atomic_init(&tr->total_no_match, 0ULL);

    load_templates_file(counterclaims_path, tr);
    load_antiframes_file(antiframes_path, tr);
    return tr;
}

void
toxicity_response_destroy(toxicity_response_t* tr)
{
    if (!tr) return;
    if (tr->reload_mutex) nimcp_mutex_free(tr->reload_mutex);
    nimcp_free(tr->templates);
    nimcp_free(tr->antiframes);
    nimcp_free(tr);
}

int
toxicity_response_generate(const toxicity_response_t* tr,
                            const char* toxic_text,
                            const char* matched_category,
                            int current_stage,
                            toxicity_response_result_t* out)
{
    if (!out) return -1;
    memset(out, 0, sizeof(*out));
    if (!tr) return 0;
    if (!matched_category) matched_category = "toxic_generic";
    if (current_stage < 0) current_stage = 0;

    toxicity_response_t* trm = (toxicity_response_t*)tr;
    atomic_fetch_add_explicit(&trm->total_generations, 1ULL,
                              memory_order_relaxed);

    nimcp_mutex_lock(trm->reload_mutex);

    /* Tier 1: template lookup. */
    int idx = find_best_template(tr, matched_category, current_stage);
    if (idx >= 0) {
        char group_buf[128];
        extract_group(toxic_text, group_buf, sizeof(group_buf));
        substitute_group(tr->templates[idx].template_text, group_buf,
                          out->text, sizeof(out->text));
        strncpy(out->source, "template", sizeof(out->source) - 1);
        out->stage_matched = tr->templates[idx].stage;
        nimcp_mutex_unlock(trm->reload_mutex);
        atomic_fetch_add_explicit(&trm->total_template_hits, 1ULL,
                                  memory_order_relaxed);
        return 0;
    }

    /* Tier 2: anti-frame swap fallback. */
    if (tr->num_antiframes > 0 && toxic_text && *toxic_text) {
        int swaps = apply_antiframe_swap(tr, toxic_text,
                                          out->text, sizeof(out->text));
        if (swaps > 0) {
            strncpy(out->source, "antiframe", sizeof(out->source) - 1);
            out->antiframe_swaps = swaps;
            nimcp_mutex_unlock(trm->reload_mutex);
            atomic_fetch_add_explicit(&trm->total_antiframe_hits, 1ULL,
                                      memory_order_relaxed);
            return 0;
        }
    }

    /* No counterclaim available — caller uses its default refusal. */
    nimcp_mutex_unlock(trm->reload_mutex);
    atomic_fetch_add_explicit(&trm->total_no_match, 1ULL,
                              memory_order_relaxed);
    return 0;
}

int
toxicity_response_reload(toxicity_response_t* tr,
                          const char* counterclaims_path,
                          const char* antiframes_path)
{
    if (!tr) return -1;
    nimcp_mutex_lock(tr->reload_mutex);
    /* Free + reload. Atomic-swap version is overkill for this — generation
     * holds the same mutex so reload doesn't race a concurrent generate. */
    nimcp_free(tr->templates);  tr->templates = NULL;
    tr->num_templates = 0; tr->cap_templates = 0;
    nimcp_free(tr->antiframes); tr->antiframes = NULL;
    tr->num_antiframes = 0; tr->cap_antiframes = 0;
    load_templates_file(counterclaims_path, tr);
    load_antiframes_file(antiframes_path, tr);
    nimcp_mutex_unlock(tr->reload_mutex);
    return 0;
}

void
toxicity_response_get_stats(const toxicity_response_t* tr,
                             uint64_t* out_generations,
                             uint64_t* out_template_hits,
                             uint64_t* out_antiframe_hits,
                             uint64_t* out_no_match)
{
    if (!tr) return;
    toxicity_response_t* trm = (toxicity_response_t*)tr;
    if (out_generations)
        *out_generations = atomic_load_explicit(&trm->total_generations,
                                                memory_order_relaxed);
    if (out_template_hits)
        *out_template_hits = atomic_load_explicit(&trm->total_template_hits,
                                                  memory_order_relaxed);
    if (out_antiframe_hits)
        *out_antiframe_hits = atomic_load_explicit(&trm->total_antiframe_hits,
                                                   memory_order_relaxed);
    if (out_no_match)
        *out_no_match = atomic_load_explicit(&trm->total_no_match,
                                             memory_order_relaxed);
}

size_t
toxicity_response_template_count(const toxicity_response_t* tr)
{
    return tr ? tr->num_templates : 0;
}

size_t
toxicity_response_antiframe_count(const toxicity_response_t* tr)
{
    return tr ? tr->num_antiframes : 0;
}
