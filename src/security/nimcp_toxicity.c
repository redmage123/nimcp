/**
 * @file nimcp_toxicity.c
 * @brief Implementation of the pattern-based toxicity classifier.
 *
 * See nimcp_toxicity.h for design + limits.
 */
#include "security/nimcp_toxicity.h"
#include "security/nimcp_toxicity_ml.h"

#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"

#include <ctype.h>
#include <errno.h>
#include <regex.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG_MODULE "toxicity"

typedef struct {
    regex_t  regex;
    float    harm_weight;
    float    fairness_weight;
    char     category[TOXICITY_CATEGORY_LEN];
    char     description[96];
    int      compiled;            /* 1 once regcomp succeeded */
} toxicity_pattern_t;

struct toxicity_classifier {
    toxicity_pattern_t* patterns;
    size_t              num_patterns;
    size_t              capacity;
    float               threshold;
    nimcp_mutex_t*      reload_mutex;
    /* Optional ML head — toxicity_ml_classifier_t* (opaque). When non-NULL,
     * classify() ensembles pattern scores with ML scores via max(): the
     * pattern classifier does NOT own the ML head; the caller wires and
     * destroys it. The pattern classifier is high-precision, the ML head
     * is high-recall — ensemble catches more than either alone. */
    void*               ml_head;
    /* stats (atomic so classify() stays read-only on the hot path) */
    _Atomic uint64_t    total_classifications;
    _Atomic uint64_t    total_matches;
    _Atomic uint64_t    total_would_block;
};

/* ---------------------------------------------------------------- helpers */

static void
free_patterns(toxicity_pattern_t* patterns, size_t n)
{
    if (!patterns) return;
    for (size_t i = 0; i < n; i++) {
        if (patterns[i].compiled) {
            regfree(&patterns[i].regex);
        }
    }
    nimcp_free(patterns);
}

static int
ensure_capacity(toxicity_pattern_t** patterns_io,
                size_t* cap_io,
                size_t need)
{
    if (need <= *cap_io) return 0;
    size_t new_cap = *cap_io ? *cap_io : 16;
    while (new_cap < need) new_cap *= 2;
    toxicity_pattern_t* np =
        (toxicity_pattern_t*)nimcp_calloc(new_cap, sizeof(toxicity_pattern_t));
    if (!np) return -1;
    if (*patterns_io) {
        memcpy(np, *patterns_io, (*cap_io) * sizeof(toxicity_pattern_t));
        nimcp_free(*patterns_io);
    }
    *patterns_io = np;
    *cap_io = new_cap;
    return 0;
}

static char*
trim(char* s)
{
    if (!s) return s;
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s) return s;
    char* end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) { *end = '\0'; end--; }
    return s;
}

/* Add a single rule (category, weights, regex). Returns 0 on success, -1 on
 * compile failure (the slot is left uninitialized — caller drops the rule). */
static int
add_rule(toxicity_pattern_t** patterns_io, size_t* num_io, size_t* cap_io,
         const char* category, float harm, float fairness,
         const char* regex_pattern, const char* description)
{
    if (ensure_capacity(patterns_io, cap_io, *num_io + 1) != 0) {
        LOG_MODULE_ERROR("toxicity", "out of memory growing pattern table");
        return -1;
    }
    toxicity_pattern_t* p = &(*patterns_io)[*num_io];
    memset(p, 0, sizeof(*p));
    /* NOTE: REG_NOSUB intentionally removed — we need match positions
     * (regmatch_t) so the classify() span-overlap check can suppress toxic
     * matches that fall fully inside an allowlist span (e.g. "kill the
     * immigrants" inside the allowlist span "don't kill the immigrants"). */
    if (regcomp(&p->regex, regex_pattern,
                REG_EXTENDED | REG_ICASE | REG_NEWLINE) != 0) {
        LOG_MODULE_WARN("toxicity", "skipping uncompilable pattern in '%s': %s",
                        category ? category : "?", regex_pattern);
        return -1;
    }
    p->compiled = 1;
    if (harm < 0.0f) harm = 0.0f;
    if (harm > 1.0f) harm = 1.0f;
    if (fairness < 0.0f) fairness = 0.0f;
    if (fairness > 1.0f) fairness = 1.0f;
    p->harm_weight = harm;
    p->fairness_weight = fairness;
    strncpy(p->category, category ? category : "uncategorized",
            sizeof(p->category) - 1);
    strncpy(p->description, description ? description : "",
            sizeof(p->description) - 1);
    (*num_io)++;
    return 0;
}

/* Load an external slur file (one POSIX-extended regex per line; '#' comments).
 * Best-effort: missing file is fine (no slur patterns added). Returns the
 * number of slur patterns added (>= 0). */
static int
load_external_slur_file(toxicity_pattern_t** patterns_io,
                        size_t* num_io,
                        size_t* cap_io,
                        const char* path,
                        float weight)
{
    if (!path || !*path) return 0;
    FILE* f = fopen(path, "r");
    if (!f) {
        /* Quiet: this file is expected to be absent on a fresh repo. */
        return 0;
    }
    char line[512];
    int added = 0;
    while (fgets(line, sizeof(line), f)) {
        char* s = trim(line);
        if (!*s || *s == '#') continue;
        if (add_rule(patterns_io, num_io, cap_io,
                     "slurs_external", weight, weight, s,
                     "external curated slur regex") == 0) {
            added++;
        }
    }
    fclose(f);
    if (added > 0) {
        LOG_MODULE_INFO("toxicity", "loaded %d slur patterns from %s",
                        added, path);
    }
    return added;
}

/* Parse a single TSV line of the rules file. Returns 0 on success, -1 if the
 * line is malformed (caller skips it; the file as a whole is not rejected). */
static int
parse_tsv_line(char* line,
               toxicity_pattern_t** patterns_io,
               size_t* num_io,
               size_t* cap_io)
{
    /* Skip comments + blanks. Comment lines start with '#'. */
    char* trimmed = line;
    while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
    if (!*trimmed || *trimmed == '\n' || *trimmed == '#') return 0;

    /* Strip trailing newline. */
    size_t L = strlen(line);
    while (L > 0 && (line[L-1] == '\n' || line[L-1] == '\r')) {
        line[--L] = '\0';
    }
    if (L == 0) return 0;

    /* Five tab-separated columns:
     *   category \t harm \t fairness \t regex \t description
     * Description may contain anything (no further tabs expected). */
    char* fields[5] = {NULL, NULL, NULL, NULL, NULL};
    int n = 0;
    char* p = line;
    fields[n++] = p;
    while (*p && n < 5) {
        if (*p == '\t') {
            *p = '\0';
            p++;
            if (n < 5) fields[n++] = p;
            continue;
        }
        p++;
    }
    if (n < 4) {
        /* Malformed — at least category, harm, fairness, regex required. */
        return -1;
    }
    /* fields[5] is optional (description); fill in blank if absent. */
    if (n < 5) fields[4] = "";

    float harm     = (float)atof(fields[1]);
    float fairness = (float)atof(fields[2]);
    const char* regex_pat = fields[3];
    const char* category  = fields[0];
    const char* desc      = fields[4];

    /* Special pseudo-pattern that means "load slur regexes from external
     * file referenced by the trailing comment". The encoding is:
     *   regex == "__LOAD_FROM_FILE__<path>"
     * We don't add a rule for this row — we recursively load the file. */
    const char* MARKER = "__LOAD_FROM_FILE__";
    size_t mlen = strlen(MARKER);
    if (strncmp(regex_pat, MARKER, mlen) == 0) {
        const char* file_path = regex_pat + mlen;
        load_external_slur_file(patterns_io, num_io, cap_io,
                                file_path, fairness);
        return 0;
    }

    return add_rule(patterns_io, num_io, cap_io, category,
                    harm, fairness, regex_pat, desc);
}

/* Parse the entire TSV rules file into a fresh pattern table. Returns the
 * number of rules loaded (>= 0) or -1 on file-open failure. */
static int
load_rules_file(const char* rules_path,
                toxicity_pattern_t** patterns_out,
                size_t* num_out,
                size_t* cap_out)
{
    if (!rules_path) return -1;
    FILE* f = fopen(rules_path, "r");
    if (!f) return -1;

    toxicity_pattern_t* patterns = NULL;
    size_t num = 0, cap = 0;
    char line[2048];
    int line_no = 0;
    int parse_errors = 0;

    while (fgets(line, sizeof(line), f)) {
        line_no++;
        if (parse_tsv_line(line, &patterns, &num, &cap) != 0) {
            parse_errors++;
            LOG_MODULE_WARN("toxicity",
                            "rules file %s: line %d malformed (skipped)",
                            rules_path, line_no);
        }
    }
    fclose(f);

    if (parse_errors > 0) {
        LOG_MODULE_WARN("toxicity",
                        "rules file %s loaded with %d parse warning(s); %zu rules active",
                        rules_path, parse_errors, num);
    }

    *patterns_out = patterns;
    *num_out = num;
    *cap_out = cap;
    return (int)num;
}

/* ----------------------------------------------------------------- public */

toxicity_classifier_t*
toxicity_classifier_create(const char* rules_path, int fail_on_missing)
{
    toxicity_classifier_t* tc =
        (toxicity_classifier_t*)nimcp_calloc(1, sizeof(*tc));
    if (!tc) return NULL;

    tc->threshold = 0.7f;
    tc->reload_mutex = nimcp_mutex_create(NULL);
    if (!tc->reload_mutex) {
        nimcp_free(tc);
        return NULL;
    }
    atomic_init(&tc->total_classifications, 0ULL);
    atomic_init(&tc->total_matches, 0ULL);
    atomic_init(&tc->total_would_block, 0ULL);

    int rc = load_rules_file(rules_path, &tc->patterns, &tc->num_patterns,
                             &tc->capacity);
    if (rc < 0) {
        if (fail_on_missing) {
            LOG_MODULE_ERROR("toxicity", "rules file missing: %s", rules_path);
            nimcp_mutex_free(tc->reload_mutex);
            nimcp_free(tc);
            return NULL;
        }
        LOG_MODULE_WARN("toxicity", "rules file missing: %s — classifier is a no-op",
                        rules_path);
    } else {
        LOG_MODULE_INFO("toxicity", "loaded %zu pattern(s) from %s",
                        tc->num_patterns, rules_path);
    }
    return tc;
}

void
toxicity_classifier_destroy(toxicity_classifier_t* tc)
{
    if (!tc) return;
    free_patterns(tc->patterns, tc->num_patterns);
    if (tc->reload_mutex) nimcp_mutex_free(tc->reload_mutex);
    nimcp_free(tc);
}

/* Allowlist short-circuit: rows with category == "allowlist" are anti-toxic
 * patterns. If ANY allowlist pattern matches, classify() returns a clean
 * result regardless of what toxic patterns would have matched. This protects
 * explicit anti-toxic constructions (e.g. "X are not subhuman") from being
 * mis-flagged. */
#define TOXICITY_ALLOWLIST_CATEGORY "allowlist"

int
toxicity_classify(const toxicity_classifier_t* tc,
                  const char* text,
                  toxicity_result_t* out)
{
    if (!tc || !out) return -1;
    memset(out, 0, sizeof(*out));
    if (!text || !*text) return 0;  /* empty text -> no match */

    /* Cast away const for atomic counter increments — counters are mutable
     * accounting only; the pattern table is held stable for the duration of
     * this call by the reload_mutex below. */
    toxicity_classifier_t* tcm = (toxicity_classifier_t*)tc;
    atomic_fetch_add_explicit(&tcm->total_classifications, 1ULL,
                              memory_order_relaxed);

    /* Hold the reload mutex around the whole match pass. This prevents
     * reload() from swapping/freeing the pattern table out from under us
     * (use-after-free race). */
    nimcp_mutex_lock(tcm->reload_mutex);

    /* Pass 1: allowlist. Sets anti_toxic_signal AND collects match spans.
     * Does NOT short-circuit. Round-3 walkthrough fix 2026-05-20: an earlier
     * design returned immediately on allowlist match, which let an attacker
     * prepend "X are not subhuman." to a real threat ("Kill all the Y.") —
     * the disclaimer matched the allowlist and the threat was never scanned.
     * Now allowlist is a separate signal AND we collect each match's
     * (start, end) span so the toxic pass can suppress toxic matches that
     * fall fully inside an allowlist span (e.g. "kill the immigrants"
     * substring inside "don't kill the immigrants"). */
    typedef struct { regoff_t so, eo; } match_span_t;
    enum { TOX_ALLOWLIST_MAX_SPANS = 16 };
    match_span_t allowlist_spans[TOX_ALLOWLIST_MAX_SPANS];
    int num_allowlist_spans = 0;
    int allowlist_hit_first_idx = -1;
    for (size_t i = 0; i < tc->num_patterns; i++) {
        const toxicity_pattern_t* p = &tc->patterns[i];
        if (!p->compiled) continue;
        if (strcmp(p->category, TOXICITY_ALLOWLIST_CATEGORY) != 0) continue;
        /* Round-5 walkthrough fix: iterate to find ALL allowlist matches
         * (same approach as Pass 2 below). regexec returns only the
         * leftmost; without iteration, a text with three anti-toxic
         * sentences (e.g. "Don't kill X. Never deport Y. Shouldn't lynch Z.")
         * would only capture span 1, and toxic matches in sentences 2/3
         * would not be covered by an allowlist span. */
        const char* cursor = text;
        int first_iter = 1;
        while (*cursor) {
            regmatch_t m;
            int eflags = first_iter ? 0 : REG_NOTBOL;
            if (regexec(&p->regex, cursor, 1, &m, eflags) != 0) break;
            regoff_t abs_so = (regoff_t)(cursor - text) + m.rm_so;
            regoff_t abs_eo = (regoff_t)(cursor - text) + m.rm_eo;
            out->anti_toxic_signal = 1.0f;
            out->num_matches++;
            if (allowlist_hit_first_idx < 0) allowlist_hit_first_idx = (int)i;
            if (num_allowlist_spans < TOX_ALLOWLIST_MAX_SPANS) {
                allowlist_spans[num_allowlist_spans].so = abs_so;
                allowlist_spans[num_allowlist_spans].eo = abs_eo;
                num_allowlist_spans++;
            }
            if (m.rm_eo <= m.rm_so) cursor += 1;
            else                    cursor += m.rm_eo;
            first_iter = 0;
        }
    }

    /* Pass 2: toxic categories. For each pattern, iterate to find ALL matches
     * in the text (not just the first). Round-3 walkthrough fix 2026-05-20:
     * regexec returns only the LEFTMOST match. If that match happens to fall
     * inside an allowlist span ("deport the refugees" inside "never deport
     * the refugees"), suppressing it would let a later threat outside the
     * disclaimer ("kill all the immigrants") slip through invisibly. We
     * iterate by advancing the cursor past each match's end. */
    float best_score = 0.0f;
    size_t best_idx = SIZE_MAX;

    for (size_t i = 0; i < tc->num_patterns; i++) {
        const toxicity_pattern_t* p = &tc->patterns[i];
        if (!p->compiled) continue;
        if (strcmp(p->category, TOXICITY_ALLOWLIST_CATEGORY) == 0) continue;
        const char* cursor = text;
        int first_iter = 1;
        while (*cursor) {
            regmatch_t m;
            int eflags = first_iter ? 0 : REG_NOTBOL;
            if (regexec(&p->regex, cursor, 1, &m, eflags) != 0) break;
            regoff_t abs_so = (regoff_t)(cursor - text) + m.rm_so;
            regoff_t abs_eo = (regoff_t)(cursor - text) + m.rm_eo;

            /* Span-overlap check: if this toxic match is fully contained in
             * some allowlist span, treat it as disclaimer-covered and skip
             * (but keep iterating for LATER toxic matches in the text). */
            int covered = 0;
            for (int s = 0; s < num_allowlist_spans; s++) {
                if (allowlist_spans[s].so <= abs_so &&
                    allowlist_spans[s].eo >= abs_eo) {
                    covered = 1;
                    break;
                }
            }
            if (!covered) {
                out->num_matches++;
                if (p->harm_weight > out->predicted_harm)
                    out->predicted_harm = p->harm_weight;
                if (p->fairness_weight > out->fairness_violation)
                    out->fairness_violation = p->fairness_weight;
                float score = p->harm_weight > p->fairness_weight
                                ? p->harm_weight : p->fairness_weight;
                if (score > best_score) {
                    best_score = score;
                    best_idx = i;
                }
            }
            /* Advance past this match. Guard against zero-width matches
             * (would otherwise loop forever). */
            if (m.rm_eo <= m.rm_so) cursor += 1;
            else                    cursor += m.rm_eo;
            first_iter = 0;
        }
    }

    out->max_score = best_score;
    if (best_idx != SIZE_MAX) {
        /* A toxic pattern matched — record it in matched_category. */
        const toxicity_pattern_t* p = &tc->patterns[best_idx];
        strncpy(out->matched_category, p->category,
                sizeof(out->matched_category) - 1);
        /* matched_pattern: short tag = "<category>#<idx>". We deliberately do
         * NOT echo back the raw regex (it can contain slur enumerations the
         * caller shouldn't pass through unfiltered). */
        snprintf(out->matched_pattern, sizeof(out->matched_pattern),
                 "%s#%zu", p->category, best_idx);
        atomic_fetch_add_explicit(&tcm->total_matches, 1ULL,
                                  memory_order_relaxed);
    } else if (allowlist_hit_first_idx >= 0) {
        /* Only an allowlist matched (no toxic pattern). Reflect that in the
         * diagnostic fields for observability — caller checks
         * anti_toxic_signal for the actual semantic flag. */
        const toxicity_pattern_t* p = &tc->patterns[allowlist_hit_first_idx];
        strncpy(out->matched_category, p->category,
                sizeof(out->matched_category) - 1);
        snprintf(out->matched_pattern, sizeof(out->matched_pattern),
                 "%s#%d", p->category, allowlist_hit_first_idx);
    }
    /* === ML head ensemble (Task 134, 2026-05-21) ===
     * If an ML head is attached, take max(pattern, ml) for both score
     * channels. The ML head is high-recall (catches coded / paraphrased
     * toxicity the pattern set misses); ensemble preserves pattern's
     * high-precision wins while adding ML's coverage. Skip when the
     * allowlist already covered the span (anti_toxic_signal=1.0 + no toxic
     * matches) — the disclaimer should not be re-flagged by the ML head. */
    if (tc->ml_head) {
        toxicity_ml_result_t mr = {0};
        if (toxicity_ml_predict(tc->ml_head, text, &mr) == 0) {
            if (mr.predicted_harm > out->predicted_harm)
                out->predicted_harm = mr.predicted_harm;
            if (mr.fairness_violation > out->fairness_violation)
                out->fairness_violation = mr.fairness_violation;
            /* max_score across pattern+ml. */
            float ml_score = mr.predicted_harm > mr.fairness_violation
                                ? mr.predicted_harm : mr.fairness_violation;
            if (ml_score > out->max_score) out->max_score = ml_score;
            /* Record ML hit as a match if no pattern matched but ML
             * crosses the threshold; the matched_category is left as
             * "ml_classifier" so the audit trail knows which lane fired. */
            if (best_idx == SIZE_MAX && ml_score >= tc->threshold) {
                strncpy(out->matched_category, "ml_classifier",
                        sizeof(out->matched_category) - 1);
                strncpy(out->matched_pattern, "ml#0",
                        sizeof(out->matched_pattern) - 1);
                out->num_matches++;
                atomic_fetch_add_explicit(&tcm->total_matches, 1ULL,
                                          memory_order_relaxed);
            }
        }
    }

    /* `would_block` is a HINT, not an instruction. The user-stated policy
     * (2026-05-20) is: NEVER delete or filter toxic content — mark it with
     * scores and let downstream evaluators (LGSS rules, ethics gates) decide
     * what to do in context. Callers should propagate predicted_harm /
     * fairness_violation into the action context rather than dropping the
     * content. */
    out->would_block = (out->max_score >= tc->threshold) ? 1 : 0;
    if (out->would_block) {
        atomic_fetch_add_explicit(&tcm->total_would_block, 1ULL,
                                  memory_order_relaxed);
    }
    nimcp_mutex_unlock(tcm->reload_mutex);
    return 0;
}

float
toxicity_classifier_get_threshold(const toxicity_classifier_t* tc)
{
    return tc ? tc->threshold : 0.0f;
}

void
toxicity_classifier_set_threshold(toxicity_classifier_t* tc, float threshold)
{
    if (!tc) return;
    if (threshold < 0.0f) threshold = 0.0f;
    if (threshold > 1.0f) threshold = 1.0f;
    tc->threshold = threshold;
}

int
toxicity_classifier_reload(toxicity_classifier_t* tc, const char* rules_path)
{
    if (!tc || !rules_path) return -1;
    toxicity_pattern_t* fresh_patterns = NULL;
    size_t fresh_n = 0, fresh_cap = 0;
    int rc = load_rules_file(rules_path, &fresh_patterns, &fresh_n,
                             &fresh_cap);
    if (rc < 0) {
        LOG_MODULE_ERROR("toxicity", "reload failed (file unreadable: %s) — "
                                     "keeping current rules", rules_path);
        return -1;
    }
    nimcp_mutex_lock(tc->reload_mutex);
    toxicity_pattern_t* old_patterns = tc->patterns;
    size_t old_n = tc->num_patterns;
    tc->patterns = fresh_patterns;
    tc->num_patterns = fresh_n;
    tc->capacity = fresh_cap;
    nimcp_mutex_unlock(tc->reload_mutex);
    free_patterns(old_patterns, old_n);
    LOG_MODULE_INFO("toxicity", "reloaded %zu pattern(s) from %s",
                    fresh_n, rules_path);
    return 0;
}

void
toxicity_classifier_get_stats(const toxicity_classifier_t* tc,
                              uint64_t* out_total_classifications,
                              uint64_t* out_total_matches,
                              uint64_t* out_total_would_block)
{
    if (!tc) return;
    toxicity_classifier_t* tcm = (toxicity_classifier_t*)tc;
    if (out_total_classifications) {
        *out_total_classifications =
            atomic_load_explicit(&tcm->total_classifications,
                                 memory_order_relaxed);
    }
    if (out_total_matches) {
        *out_total_matches =
            atomic_load_explicit(&tcm->total_matches, memory_order_relaxed);
    }
    if (out_total_would_block) {
        *out_total_would_block =
            atomic_load_explicit(&tcm->total_would_block,
                                 memory_order_relaxed);
    }
}

size_t
toxicity_classifier_pattern_count(const toxicity_classifier_t* tc)
{
    return tc ? tc->num_patterns : 0;
}

int
toxicity_classifier_attach_ml(toxicity_classifier_t* tc, void* ml_head)
{
    if (!tc) return -1;
    nimcp_mutex_lock(tc->reload_mutex);
    tc->ml_head = ml_head;
    nimcp_mutex_unlock(tc->reload_mutex);
    return 0;
}

void*
toxicity_classifier_get_ml(const toxicity_classifier_t* tc)
{
    return tc ? tc->ml_head : NULL;
}
