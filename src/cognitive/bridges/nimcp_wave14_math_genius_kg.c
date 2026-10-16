//=============================================================================
// nimcp_wave14_math_genius_kg.c - Wave W14 Math / Game / Parietal genius KG
//=============================================================================
/**
 * See include/cognitive/kg/nimcp_wave14_math_genius_kg.h for scope + contract.
 *
 * Admin-token self-elevate pattern — all writes follow the
 * kg-node-naming-registry.md §7 rule: set ADMIN with
 * brain->internal_kg_admin_token, write, restore READ.
 *
 * Template pedigree: mirrors src/cognitive/bridges/nimcp_wave10_affective_kg.c
 * and src/physics/graphs/nimcp_graph_theory_bridge.c (discipline-subgraph
 * pattern).
 */

#include "cognitive/kg/nimcp_wave14_math_genius_kg.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(wave14_math_genius_kg, MESH_ADAPTER_CATEGORY_COGNITIVE)

/* ========================================================================= */
/* Internal helpers                                                          */
/* ========================================================================= */

static int w14_kg_usable(struct brain_struct* brain) {
    return (brain && brain->internal_kg_enabled && brain->internal_kg);
}

static uint64_t w14_ts_us(void) {
    return (uint64_t)time(NULL) * 1000000ULL;
}

/** Add a node if missing. Caller must hold ADMIN. */
static brain_kg_node_id_t w14_ensure_node(
    brain_kg_t* kg, const char* name,
    brain_kg_node_type_t type, const char* desc)
{
    brain_kg_node_id_t id = brain_kg_find_node(kg, name);
    if (id != BRAIN_KG_INVALID_NODE) return id;
    return brain_kg_add_node(kg, name, type, desc);
}

/** Emit an event node + back-edge to a parent. Elevates + restores. */
static void w14_emit_linked(
    struct brain_struct* brain,
    const char* event_name,
    const char* description,
    const char* parent_name,
    float weight)
{
    if (!w14_kg_usable(brain) || !event_name || !parent_name) return;

    brain_kg_t* kg = brain->internal_kg;
    const uint64_t token = brain->internal_kg_admin_token;

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN, token);

    brain_kg_node_id_t ev = brain_kg_add_node(kg, event_name,
        BRAIN_KG_NODE_COGNITIVE,
        description ? description : "wave14 math/game/genius runtime event");

    if (ev != BRAIN_KG_INVALID_NODE) {
        brain_kg_node_id_t parent = brain_kg_find_node(kg, parent_name);
        if (parent != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, parent, ev, BRAIN_KG_EDGE_SENDS_TO,
                              "produced_by", weight);
        }
    }

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);
}

/** Store numeric metadata on a named root. Elevates + restores. */
static void w14_store_meta_f(
    struct brain_struct* brain,
    const char* root_name,
    const char* key,
    float value)
{
    if (!w14_kg_usable(brain) || !root_name || !key) return;
    brain_kg_t* kg = brain->internal_kg;
    brain_kg_node_id_t nid = brain_kg_find_node(kg, root_name);
    if (nid == BRAIN_KG_INVALID_NODE) return;

    const uint64_t token = brain->internal_kg_admin_token;
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN, token);

    char buf[32];
    snprintf(buf, sizeof(buf), "%.4f", value);
    brain_kg_add_metadata(kg, nid, key, buf);

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);
}

/** Read a float metadata value from a named root; default on miss. */
static float w14_read_meta_f(
    struct brain_struct* brain,
    const char* root_name,
    const char* key,
    float default_v)
{
    if (!w14_kg_usable(brain) || !root_name || !key) return default_v;
    brain_kg_t* kg = brain->internal_kg;
    brain_kg_node_id_t nid = brain_kg_find_node(kg, root_name);
    if (nid == BRAIN_KG_INVALID_NODE) return default_v;
    const brain_kg_node_t* n = brain_kg_get_node(kg, nid);
    if (!n) return default_v;
    for (uint32_t i = 0; i < n->metadata_count; ++i) {
        if (strcmp(n->metadata[i].key, key) == 0) {
            float v = default_v;
            if (sscanf(n->metadata[i].value, "%f", &v) == 1) return v;
            return default_v;
        }
    }
    return default_v;
}

/** Map a discipline string to its canonical root-node name. */
static const char* w14_math_discipline_root(const char* discipline) {
    if (!discipline || !*discipline) return "cog_math";
    if (strcmp(discipline, "algebra") == 0)             return "cog_math_algebra";
    if (strcmp(discipline, "calculus") == 0)            return "cog_math_calculus";
    if (strcmp(discipline, "real_analysis") == 0)       return "cog_math_calculus";
    if (strcmp(discipline, "topology") == 0)            return "cog_math_topology";
    if (strcmp(discipline, "probability") == 0)         return "cog_math_probability";
    if (strcmp(discipline, "statistics") == 0)          return "cog_math_probability";
    if (strcmp(discipline, "number_theory") == 0)       return "cog_math_number_theory";
    if (strcmp(discipline, "logic") == 0)               return "cog_math_logic";
    if (strcmp(discipline, "category_theory") == 0)     return "cog_math_category_theory";
    if (strcmp(discipline, "numerical_methods") == 0)   return "cog_math_numerical_methods";
    if (strcmp(discipline, "optimization") == 0)        return "cog_math_optimization";
    if (strcmp(discipline, "combinatorics") == 0)       return "cog_math_combinatorics";
    if (strcmp(discipline, "graph_theory") == 0)        return "cog_math_graph_theory";
    if (strcmp(discipline, "information_theory") == 0)  return "cog_math_information_theory";
    if (strcmp(discipline, "complexity_theory") == 0)   return "cog_math_complexity_theory";
    return "cog_math";
}

/** Map a genius name to its canonical root. */
static const char* w14_genius_root(const char* genius_name) {
    if (!genius_name || !*genius_name) return "cog_parietal_genius";
    if (strcmp(genius_name, "erdos") == 0)  return "cog_genius_erdos";
    if (strcmp(genius_name, "gauss") == 0)  return "cog_genius_gauss";
    if (strcmp(genius_name, "newton") == 0) return "cog_genius_newton";
    return "cog_parietal_genius";
}

/* ========================================================================= */
/* Structural init                                                           */
/* ========================================================================= */

int nimcp_wave14_math_genius_kg_init(struct brain_struct* brain) {
    if (!w14_kg_usable(brain)) return 0;

    brain_kg_t* kg = brain->internal_kg;
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN,
                              brain->internal_kg_admin_token);

    /* Umbrella roots. */
    brain_kg_node_id_t math_root = w14_ensure_node(
        kg, "cog_math", BRAIN_KG_NODE_COGNITIVE,
        "W14: Mathematical cognition umbrella (algebra, calculus, topology, ...)");
    brain_kg_node_id_t gt_root = w14_ensure_node(
        kg, "cog_game_theory", BRAIN_KG_NODE_COGNITIVE,
        "W14: Game-theoretic reasoning umbrella");
    brain_kg_node_id_t genius_root = w14_ensure_node(
        kg, "cog_parietal_genius", BRAIN_KG_NODE_COGNITIVE,
        "W14: Parietal genius profiles (Erdős, Gauss, Newton)");

    /* Math disciplines — structural sub-graph, one node per discipline. */
    struct { const char* name; const char* desc; } disciplines[] = {
        { "cog_math_algebra",             "W14: Abstract algebra (groups, rings, fields)" },
        { "cog_math_calculus",            "W14: Calculus / real analysis (diff, integrate, series)" },
        { "cog_math_topology",            "W14: Topology (continuous maps, invariants, knots)" },
        { "cog_math_probability",         "W14: Probability + statistics (distributions, inference)" },
        { "cog_math_number_theory",       "W14: Number theory (primes, zeta, Diophantine)" },
        { "cog_math_logic",               "W14: Mathematical logic (propositions, proofs, decidability)" },
        { "cog_math_category_theory",     "W14: Category theory (functors, morphisms, limits)" },
        { "cog_math_numerical_methods",   "W14: Numerical methods (ODE, PDE, root-finding)" },
        { "cog_math_optimization",        "W14: Optimization (convex, nonlinear, global)" },
        { "cog_math_combinatorics",       "W14: Combinatorics (enumeration, design, extremal)" },
        { "cog_math_graph_theory",        "W14: Graph theory cognition (paths, flows, centrality)" },
        { "cog_math_information_theory",  "W14: Information theory (entropy, channels, coding)" },
        { "cog_math_complexity_theory",   "W14: Complexity theory (P/NP, hierarchy, reductions)" },
    };

    const size_t nd = sizeof(disciplines) / sizeof(disciplines[0]);
    for (size_t i = 0; i < nd; ++i) {
        brain_kg_node_id_t id = w14_ensure_node(
            kg, disciplines[i].name, BRAIN_KG_NODE_COGNITIVE, disciplines[i].desc);
        if (math_root != BRAIN_KG_INVALID_NODE && id != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, math_root, id, BRAIN_KG_EDGE_CONNECTS_TO,
                              "math discipline", 1.0f);
        }
    }

    /* Game-theory children. */
    brain_kg_node_id_t gt_eq = w14_ensure_node(
        kg, "cog_game_theory_equilibrium", BRAIN_KG_NODE_COGNITIVE,
        "W14: Nash / Bayes-Nash / correlated equilibrium solvers");
    brain_kg_node_id_t gt_co = w14_ensure_node(
        kg, "cog_game_theory_coalition", BRAIN_KG_NODE_COGNITIVE,
        "W14: Coalition formation + Shapley / core / bargaining");
    if (gt_root != BRAIN_KG_INVALID_NODE) {
        if (gt_eq != BRAIN_KG_INVALID_NODE)
            brain_kg_add_edge(kg, gt_root, gt_eq, BRAIN_KG_EDGE_CONNECTS_TO,
                              "equilibrium solvers", 1.0f);
        if (gt_co != BRAIN_KG_INVALID_NODE)
            brain_kg_add_edge(kg, gt_root, gt_co, BRAIN_KG_EDGE_CONNECTS_TO,
                              "coalition formation", 1.0f);
    }

    /* Genius profiles. */
    brain_kg_node_id_t g_erdos = w14_ensure_node(
        kg, "cog_genius_erdos", BRAIN_KG_NODE_COGNITIVE,
        "W14: Erdős profile — combinatorics, Ramsey, probabilistic method");
    brain_kg_node_id_t g_gauss = w14_ensure_node(
        kg, "cog_genius_gauss", BRAIN_KG_NODE_COGNITIVE,
        "W14: Gauss profile — number theory, differential geometry, statistics");
    brain_kg_node_id_t g_newton = w14_ensure_node(
        kg, "cog_genius_newton", BRAIN_KG_NODE_COGNITIVE,
        "W14: Newton profile — calculus, mechanics, unifying principles");
    if (genius_root != BRAIN_KG_INVALID_NODE) {
        if (g_erdos != BRAIN_KG_INVALID_NODE)
            brain_kg_add_edge(kg, genius_root, g_erdos, BRAIN_KG_EDGE_CONNECTS_TO,
                              "Erdős profile", 1.0f);
        if (g_gauss != BRAIN_KG_INVALID_NODE)
            brain_kg_add_edge(kg, genius_root, g_gauss, BRAIN_KG_EDGE_CONNECTS_TO,
                              "Gauss profile", 1.0f);
        if (g_newton != BRAIN_KG_INVALID_NODE)
            brain_kg_add_edge(kg, genius_root, g_newton, BRAIN_KG_EDGE_CONNECTS_TO,
                              "Newton profile", 1.0f);
    }

    /* Parietal cognitive-op roots. */
    brain_kg_node_id_t analog_root = w14_ensure_node(
        kg, "cog_parietal_analogical_reasoning", BRAIN_KG_NODE_COGNITIVE,
        "W14: Parietal analogical reasoning engine");
    brain_kg_node_id_t hypo_root = w14_ensure_node(
        kg, "cog_parietal_hypothesis_generation", BRAIN_KG_NODE_COGNITIVE,
        "W14: Parietal hypothesis generation engine");
    brain_kg_node_id_t insight_root = w14_ensure_node(
        kg, "cog_parietal_insight_discovery", BRAIN_KG_NODE_COGNITIVE,
        "W14: Parietal insight discovery / eureka engine");
    brain_kg_node_id_t fin_root = w14_ensure_node(
        kg, "cog_parietal_financial_orchestrator", BRAIN_KG_NODE_COGNITIVE,
        "W14: Parietal financial cognitive orchestrator");

    /* Cross-discipline edges that are always-true (not runtime):
     * genius profiles consume math disciplines; analogical reasoning
     * integrates with genius; insight discovery depends on analogical. */
    if (g_erdos != BRAIN_KG_INVALID_NODE) {
        brain_kg_node_id_t combi = brain_kg_find_node(kg, "cog_math_combinatorics");
        brain_kg_node_id_t prob  = brain_kg_find_node(kg, "cog_math_probability");
        if (combi != BRAIN_KG_INVALID_NODE)
            brain_kg_add_edge(kg, g_erdos, combi, BRAIN_KG_EDGE_INTEGRATES_WITH,
                              "Erdős uses combinatorics", 0.9f);
        if (prob != BRAIN_KG_INVALID_NODE)
            brain_kg_add_edge(kg, g_erdos, prob, BRAIN_KG_EDGE_INTEGRATES_WITH,
                              "Erdős probabilistic method", 0.8f);
    }
    if (g_gauss != BRAIN_KG_INVALID_NODE) {
        brain_kg_node_id_t nt = brain_kg_find_node(kg, "cog_math_number_theory");
        if (nt != BRAIN_KG_INVALID_NODE)
            brain_kg_add_edge(kg, g_gauss, nt, BRAIN_KG_EDGE_INTEGRATES_WITH,
                              "Gauss number theory", 0.95f);
    }
    if (g_newton != BRAIN_KG_INVALID_NODE) {
        brain_kg_node_id_t cal = brain_kg_find_node(kg, "cog_math_calculus");
        if (cal != BRAIN_KG_INVALID_NODE)
            brain_kg_add_edge(kg, g_newton, cal, BRAIN_KG_EDGE_INTEGRATES_WITH,
                              "Newton co-invented calculus", 0.95f);
    }
    if (analog_root != BRAIN_KG_INVALID_NODE && genius_root != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, analog_root, genius_root, BRAIN_KG_EDGE_PROVIDES_TO,
                          "analogy powers genius insight", 0.7f);
    }
    if (insight_root != BRAIN_KG_INVALID_NODE && analog_root != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, insight_root, analog_root, BRAIN_KG_EDGE_INTEGRATES_WITH,
                          "insight often triggered by cross-domain analogy", 0.8f);
    }
    if (hypo_root != BRAIN_KG_INVALID_NODE && insight_root != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, hypo_root, insight_root, BRAIN_KG_EDGE_PROVIDES_TO,
                          "hypotheses seed insight search", 0.7f);
    }
    if (fin_root != BRAIN_KG_INVALID_NODE) {
        brain_kg_node_id_t prob2 = brain_kg_find_node(kg, "cog_math_probability");
        brain_kg_node_id_t opt   = brain_kg_find_node(kg, "cog_math_optimization");
        if (prob2 != BRAIN_KG_INVALID_NODE)
            brain_kg_add_edge(kg, fin_root, prob2, BRAIN_KG_EDGE_INTEGRATES_WITH,
                              "finance uses probability", 0.8f);
        if (opt != BRAIN_KG_INVALID_NODE)
            brain_kg_add_edge(kg, fin_root, opt, BRAIN_KG_EDGE_INTEGRATES_WITH,
                              "finance uses optimization", 0.7f);
    }

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);

    wave14_math_genius_kg_heartbeat("wave14_init", 1.0f);
    return 0;
}

/* ========================================================================= */
/* Math emit helpers                                                         */
/* ========================================================================= */

void wave14_math_emit_proof(
    struct brain_struct* brain,
    const char* discipline,
    const char* theorem_label,
    float confidence)
{
    if (!w14_kg_usable(brain)) return;
    const char* root = w14_math_discipline_root(discipline);
    const char* lbl = (theorem_label && *theorem_label) ? theorem_label : "theorem";
    char name[192];
    snprintf(name, sizeof(name),
             "%s_event_proof_%.48s_%llu",
             root, lbl, (unsigned long long)w14_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "math proof: discipline=%s theorem=%s confidence=%.3f",
             root, lbl, confidence);
    w14_emit_linked(brain, name, desc, root, confidence);
    w14_store_meta_f(brain, root, "last_proof_confidence", confidence);
    /* Track a running proof count. */
    float prev = w14_read_meta_f(brain, root, "proof_count", 0.0f);
    w14_store_meta_f(brain, root, "proof_count", prev + 1.0f);
}

void wave14_math_emit_conjecture(
    struct brain_struct* brain,
    const char* discipline,
    const char* conjecture_label,
    float novelty)
{
    if (!w14_kg_usable(brain)) return;
    const char* root = w14_math_discipline_root(discipline);
    const char* lbl = (conjecture_label && *conjecture_label) ? conjecture_label : "conjecture";
    char name[192];
    snprintf(name, sizeof(name),
             "%s_event_conjecture_%.48s_%llu",
             root, lbl, (unsigned long long)w14_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "math conjecture: discipline=%s label=%s novelty=%.3f",
             root, lbl, novelty);
    w14_emit_linked(brain, name, desc, root, novelty);
    w14_store_meta_f(brain, root, "last_conjecture_novelty", novelty);
}

void wave14_math_emit_insight(
    struct brain_struct* brain,
    const char* discipline,
    const char* insight_label,
    float strength)
{
    if (!w14_kg_usable(brain)) return;
    const char* root = w14_math_discipline_root(discipline);
    const char* lbl = (insight_label && *insight_label) ? insight_label : "insight";
    char name[192];
    snprintf(name, sizeof(name),
             "%s_event_insight_%.48s_%llu",
             root, lbl, (unsigned long long)w14_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "math insight: discipline=%s label=%s strength=%.3f",
             root, lbl, strength);
    w14_emit_linked(brain, name, desc, root, strength);
    w14_store_meta_f(brain, root, "last_insight_strength", strength);
}

float wave14_math_query_confidence_bias(
    struct brain_struct* brain,
    const char* discipline)
{
    const char* root = w14_math_discipline_root(discipline);
    return w14_read_meta_f(brain, root, "last_proof_confidence", 0.5f);
}

/* ========================================================================= */
/* Game-theory emit helpers                                                  */
/* ========================================================================= */

void wave14_game_emit_equilibrium(
    struct brain_struct* brain,
    const char* equilibrium_type,
    uint32_t num_players,
    float payoff)
{
    if (!w14_kg_usable(brain)) return;
    const char* et = (equilibrium_type && *equilibrium_type) ? equilibrium_type : "nash";
    char name[192];
    snprintf(name, sizeof(name),
             "cog_game_theory_event_eq_%.32s_%u_%llu",
             et, num_players, (unsigned long long)w14_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "game eq: type=%s players=%u payoff=%.3f",
             et, num_players, payoff);
    w14_emit_linked(brain, name, desc,
                    "cog_game_theory_equilibrium", payoff);
    w14_store_meta_f(brain, "cog_game_theory", "last_payoff", payoff);
    w14_store_meta_f(brain, "cog_game_theory_equilibrium",
                     "last_payoff", payoff);
}

void wave14_game_emit_coalition(
    struct brain_struct* brain,
    uint32_t coalition_size,
    float stability)
{
    if (!w14_kg_usable(brain)) return;
    char name[192];
    snprintf(name, sizeof(name),
             "cog_game_theory_event_coalition_%u_%llu",
             coalition_size, (unsigned long long)w14_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "coalition: size=%u stability=%.3f",
             coalition_size, stability);
    w14_emit_linked(brain, name, desc,
                    "cog_game_theory_coalition", stability);
    w14_store_meta_f(brain, "cog_game_theory_coalition",
                     "last_stability", stability);
}

float wave14_game_query_payoff_bias(struct brain_struct* brain) {
    return w14_read_meta_f(brain, "cog_game_theory", "last_payoff", 0.5f);
}

/* ========================================================================= */
/* Genius emit helpers                                                       */
/* ========================================================================= */

void wave14_genius_emit_analogy(
    struct brain_struct* brain,
    const char* genius_name,
    const char* src_domain,
    const char* tgt_domain,
    float similarity)
{
    if (!w14_kg_usable(brain)) return;
    const char* root = w14_genius_root(genius_name);
    const char* sd = (src_domain && *src_domain) ? src_domain : "src";
    const char* td = (tgt_domain && *tgt_domain) ? tgt_domain : "tgt";
    char name[192];
    snprintf(name, sizeof(name),
             "%s_event_analogy_%.24s_%.24s_%llu",
             root, sd, td, (unsigned long long)w14_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "genius analogy: %s %s->%s similarity=%.3f",
             root, sd, td, similarity);
    w14_emit_linked(brain, name, desc, root, similarity);
    w14_store_meta_f(brain, root, "last_similarity", similarity);
}

void wave14_genius_emit_result(
    struct brain_struct* brain,
    const char* genius_name,
    const char* result_label,
    float confidence)
{
    if (!w14_kg_usable(brain)) return;
    const char* root = w14_genius_root(genius_name);
    const char* lbl = (result_label && *result_label) ? result_label : "result";
    char name[192];
    snprintf(name, sizeof(name),
             "%s_event_result_%.48s_%llu",
             root, lbl, (unsigned long long)w14_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "genius result: %s label=%s confidence=%.3f",
             root, lbl, confidence);
    w14_emit_linked(brain, name, desc, root, confidence);
    w14_store_meta_f(brain, root, "last_confidence", confidence);
}

float wave14_genius_query_similarity_bias(
    struct brain_struct* brain,
    const char* genius_name)
{
    const char* root = w14_genius_root(genius_name);
    return w14_read_meta_f(brain, root, "last_similarity", 0.5f);
}

/* ========================================================================= */
/* Parietal cognitive-op emit helpers                                        */
/* ========================================================================= */

void wave14_analogical_emit_mapping(
    struct brain_struct* brain,
    uint32_t src_domain_id,
    uint32_t tgt_domain_id,
    float mapping_score)
{
    if (!w14_kg_usable(brain)) return;
    char name[192];
    snprintf(name, sizeof(name),
             "cog_parietal_analogical_event_map_%u_%u_%llu",
             src_domain_id, tgt_domain_id, (unsigned long long)w14_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "analogical mapping: %u->%u score=%.3f",
             src_domain_id, tgt_domain_id, mapping_score);
    w14_emit_linked(brain, name, desc,
                    "cog_parietal_analogical_reasoning", mapping_score);
    w14_store_meta_f(brain, "cog_parietal_analogical_reasoning",
                     "last_score", mapping_score);
}

void wave14_hypothesis_emit_generation(
    struct brain_struct* brain,
    const char* hypothesis_label,
    float plausibility)
{
    if (!w14_kg_usable(brain)) return;
    const char* lbl = (hypothesis_label && *hypothesis_label) ? hypothesis_label : "hypothesis";
    char name[192];
    snprintf(name, sizeof(name),
             "cog_parietal_hypothesis_event_gen_%.48s_%llu",
             lbl, (unsigned long long)w14_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "hypothesis: %s plausibility=%.3f", lbl, plausibility);
    w14_emit_linked(brain, name, desc,
                    "cog_parietal_hypothesis_generation", plausibility);
    w14_store_meta_f(brain, "cog_parietal_hypothesis_generation",
                     "last_plausibility", plausibility);
}

void wave14_insight_emit_eureka(
    struct brain_struct* brain,
    uint32_t problem_id,
    float surprise,
    float elegance)
{
    if (!w14_kg_usable(brain)) return;
    char name[192];
    snprintf(name, sizeof(name),
             "cog_parietal_insight_event_eureka_%u_%llu",
             problem_id, (unsigned long long)w14_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "insight eureka: problem=%u surprise=%.3f elegance=%.3f",
             problem_id, surprise, elegance);
    w14_emit_linked(brain, name, desc,
                    "cog_parietal_insight_discovery", surprise);
    w14_store_meta_f(brain, "cog_parietal_insight_discovery",
                     "last_surprise", surprise);
    w14_store_meta_f(brain, "cog_parietal_insight_discovery",
                     "last_elegance", elegance);
}

void wave14_financial_emit_decision(
    struct brain_struct* brain,
    const char* decision_label,
    float expected_return,
    float risk)
{
    if (!w14_kg_usable(brain)) return;
    const char* lbl = (decision_label && *decision_label) ? decision_label : "decision";
    char name[192];
    snprintf(name, sizeof(name),
             "cog_parietal_financial_event_decide_%.40s_%llu",
             lbl, (unsigned long long)w14_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "financial decision: %s ret=%.3f risk=%.3f",
             lbl, expected_return, risk);
    w14_emit_linked(brain, name, desc,
                    "cog_parietal_financial_orchestrator", expected_return);
    w14_store_meta_f(brain, "cog_parietal_financial_orchestrator",
                     "last_return", expected_return);
    w14_store_meta_f(brain, "cog_parietal_financial_orchestrator",
                     "last_risk", risk);
}

/* ========================================================================= */
/* Parietal cognitive-op query helpers                                       */
/* ========================================================================= */

float wave14_analogical_query_score_bias(struct brain_struct* brain) {
    return w14_read_meta_f(brain,
                           "cog_parietal_analogical_reasoning",
                           "last_score", 0.5f);
}

float wave14_hypothesis_query_plausibility_bias(struct brain_struct* brain) {
    return w14_read_meta_f(brain,
                           "cog_parietal_hypothesis_generation",
                           "last_plausibility", 0.5f);
}

float wave14_insight_query_surprise_bias(struct brain_struct* brain) {
    return w14_read_meta_f(brain,
                           "cog_parietal_insight_discovery",
                           "last_surprise", 0.5f);
}

float wave14_financial_query_return_bias(struct brain_struct* brain) {
    return w14_read_meta_f(brain,
                           "cog_parietal_financial_orchestrator",
                           "last_return", 0.5f);
}

/* Suppress unused bridge-boilerplate helpers. */
__attribute__((unused))
static void wave14_math_genius_kg_suppress_unused(void) {
    (void)wave14_math_genius_kg_set_health_agent;
    (void)wave14_math_genius_kg_heartbeat;
}
