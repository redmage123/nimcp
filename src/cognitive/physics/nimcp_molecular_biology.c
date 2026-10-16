/**
 * @file nimcp_molecular_biology.c
 * @brief Molecular Biology simulator — DNA/RNA, transcription, translation, gene regulation
 *
 * Implements the central dogma (DNA->RNA->Protein), full codon table,
 * gene regulation by transcription factors, epigenetic modulation,
 * mRNA/protein degradation, GC content, melting temperature, lac operon.
 */

#include "cognitive/physics/nimcp_molecular_biology.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>
#include <math.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define LOG_TAG "MOLBIO"

#define CONC_EPSILON    1e-12f
#define EXPRESSION_MAX  10.0f

/* ============================================================================
 * Default config
 * ============================================================================ */

molbio_config_t molecular_biology_default_config(void)
{
    molbio_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.transcription_rate      = 50.0f;    /* ~50 nt/s for RNA pol II */
    cfg.translation_rate        = 15.0f;    /* ~15 aa/s for ribosome */
    cfg.mrna_degradation_rate   = 0.005f;   /* ~3 min half-life (bacterial) */
    cfg.protein_degradation_rate = 0.0001f; /* ~2 hour half-life */
    cfg.dt                      = 1.0f;     /* 1 second steps */
    cfg.enable_epigenetics      = true;
    cfg.enable_splicing         = false;
    return cfg;
}

/* ============================================================================
 * Create / Destroy
 * ============================================================================ */

molecular_biology_sim_t* molecular_biology_create(const molbio_config_t* config)
{
    molecular_biology_sim_t* sim =
        (molecular_biology_sim_t*)nimcp_calloc(1, sizeof(molecular_biology_sim_t));
    if (!sim) {
        LOG_ERROR(LOG_TAG, "Failed to allocate molecular_biology_sim_t");
        return NULL;
    }
    sim->config = config ? *config : molecular_biology_default_config();
    molecular_biology_load_codon_table(sim);
    sim->initialized = true;
    LOG_INFO(LOG_TAG, "Molecular biology sim created");
    return sim;
}

void molecular_biology_destroy(molecular_biology_sim_t* sim)
{
    if (!sim) return;
    LOG_INFO(LOG_TAG, "Destroying molecular biology sim (steps=%lu)",
             (unsigned long)sim->stats.step_count);
    nimcp_free(sim);
}

/* ============================================================================
 * Add entities
 * ============================================================================ */

uint32_t molecular_biology_add_gene(molecular_biology_sim_t* sim, const molbio_gene_t* g)
{
    if (!sim || !g) return UINT32_MAX;
    if (sim->num_genes >= MOLBIO_MAX_GENES) {
        LOG_WARN(LOG_TAG, "Max genes reached (%d)", MOLBIO_MAX_GENES);
        return UINT32_MAX;
    }
    uint32_t idx = sim->num_genes++;
    sim->genes[idx] = *g;
    sim->genes[idx].id = idx;
    sim->genes[idx].active = true;
    return idx;
}

uint32_t molecular_biology_add_protein(molecular_biology_sim_t* sim, const molbio_protein_t* p)
{
    if (!sim || !p) return UINT32_MAX;
    if (sim->num_proteins >= MOLBIO_MAX_PROTEINS) {
        LOG_WARN(LOG_TAG, "Max proteins reached (%d)", MOLBIO_MAX_PROTEINS);
        return UINT32_MAX;
    }
    uint32_t idx = sim->num_proteins++;
    sim->proteins[idx] = *p;
    sim->proteins[idx].id = idx;
    sim->proteins[idx].active = true;
    return idx;
}

uint32_t molecular_biology_add_regulator(molecular_biology_sim_t* sim, const molbio_regulator_t* r)
{
    if (!sim || !r) return UINT32_MAX;
    if (sim->num_regulators >= MOLBIO_MAX_REGULATORS) {
        LOG_WARN(LOG_TAG, "Max regulators reached (%d)", MOLBIO_MAX_REGULATORS);
        return UINT32_MAX;
    }
    uint32_t idx = sim->num_regulators++;
    sim->regulators[idx] = *r;
    sim->regulators[idx].active = true;
    return idx;
}

/* ============================================================================
 * Codon table: 64 codons -> amino acid single-letter codes
 * Encoding: base index 0=T/U, 1=C, 2=A, 3=G
 * Codon index = first*16 + second*4 + third
 * ============================================================================ */

static int base_to_idx(char b)
{
    switch (b) {
    case 'T': case 't': case 'U': case 'u': return 0;
    case 'C': case 'c': return 1;
    case 'A': case 'a': return 2;
    case 'G': case 'g': return 3;
    default: return -1;
    }
}

void molecular_biology_load_codon_table(molecular_biology_sim_t* sim)
{
    if (!sim) return;

    /*
     * Standard genetic code, ordered by first-second-third base
     * T=0, C=1, A=2, G=3
     * Index = b1*16 + b2*4 + b3
     */
    static const char codons[64] = {
        /* TTT TTG TTC TTA  TCT TCC TCA TCG  TAT TAC TAA TAG  TGT TGC TGA TGG */
        'F', 'L', 'F', 'L', 'S', 'S', 'S', 'S', 'Y', 'Y', '*', '*', 'C', 'C', '*', 'W',
        /* CTT CTC CTA CTG  CCT CCC CCA CCG  CAT CAC CAA CAG  CGT CGC CGA CGG */
        'L', 'L', 'L', 'L', 'P', 'P', 'P', 'P', 'H', 'H', 'Q', 'Q', 'R', 'R', 'R', 'R',
        /* ATT ATC ATA ATG  ACT ACC ACA ACG  AAT AAC AAA AAG  AGT AGC AGA AGG */
        'I', 'I', 'I', 'M', 'T', 'T', 'T', 'T', 'N', 'N', 'K', 'K', 'S', 'S', 'R', 'R',
        /* GTT GTC GTA GTG  GCT GCC GCA GCG  GAT GAC GAA GAG  GGT GGC GGA GGG */
        'V', 'V', 'V', 'V', 'A', 'A', 'A', 'A', 'D', 'D', 'E', 'E', 'G', 'G', 'G', 'G'
    };

    memcpy(sim->codon_table, codons, 64);
    LOG_INFO(LOG_TAG, "Standard genetic code loaded (64 codons)");
}

char molecular_biology_codon_to_aa(const molecular_biology_sim_t* sim,
                                   char b1, char b2, char b3)
{
    if (!sim) return '?';
    int i1 = base_to_idx(b1);
    int i2 = base_to_idx(b2);
    int i3 = base_to_idx(b3);
    if (i1 < 0 || i2 < 0 || i3 < 0) return '?';
    int idx = i1 * 16 + i2 * 4 + i3;
    if (idx < 0 || idx >= 64) return '?';
    return sim->codon_table[idx];
}

/* ============================================================================
 * Analytical functions
 * ============================================================================ */

/**
 * GC content = (G+C) / (A+T+G+C)
 */
float molecular_biology_gc_content(const char* sequence, uint32_t length)
{
    if (!sequence || length == 0) return 0.0f;
    uint32_t gc = 0, total = 0;
    for (uint32_t i = 0; i < length; i++) {
        char c = sequence[i];
        if (c == 'G' || c == 'g' || c == 'C' || c == 'c') { gc++; total++; }
        else if (c == 'A' || c == 'a' || c == 'T' || c == 't' ||
                 c == 'U' || c == 'u') { total++; }
    }
    return total > 0 ? (float)gc / (float)total : 0.0f;
}

/**
 * Melting temperature (simplified):
 * For short oligos (<14 nt): Tm = 2(A+T) + 4(G+C)
 * For longer: Tm = 64.9 + 41*(nG+nC-16.4)/(nA+nT+nG+nC)
 */
float molecular_biology_melting_temp(const char* sequence, uint32_t length)
{
    if (!sequence || length == 0) return 0.0f;
    uint32_t nA = 0, nT = 0, nG = 0, nC = 0;
    for (uint32_t i = 0; i < length; i++) {
        switch (sequence[i]) {
        case 'A': case 'a': nA++; break;
        case 'T': case 't': case 'U': case 'u': nT++; break;
        case 'G': case 'g': nG++; break;
        case 'C': case 'c': nC++; break;
        default: break;
        }
    }
    uint32_t total = nA + nT + nG + nC;
    if (total == 0) return 0.0f;

    if (total < 14) {
        /* Wallace rule */
        return (float)(2 * (nA + nT) + 4 * (nG + nC));
    }
    /* Marmur-Doty with salt correction approximation */
    return 64.9f + 41.0f * ((float)(nG + nC) - 16.4f) / (float)total;
}

/* ============================================================================
 * Transcription: gene -> mRNA expression level
 * ============================================================================ */

/**
 * Compute effective transcription rate for a gene:
 *  rate = promoter_strength * (1 - methylation) * (0.5 + 0.5*acetylation)
 *  Apply regulator effects (activators increase, repressors decrease)
 */
float molecular_biology_transcribe(molecular_biology_sim_t* sim, uint32_t gene_id)
{
    if (!sim || gene_id >= sim->num_genes) return 0.0f;
    molbio_gene_t* gene = &sim->genes[gene_id];
    if (!gene->active || !gene->is_coding) return 0.0f;

    /* Base transcription rate from promoter strength */
    float rate = gene->promoter_strength;

    /* Epigenetic modulation */
    if (sim->config.enable_epigenetics) {
        /* Methylation silences: rate *= (1 - methylation) */
        rate *= (1.0f - gene->methylation);
        /* Histone acetylation opens chromatin: rate *= (0.5 + 0.5*acetylation) */
        rate *= (0.5f + 0.5f * gene->histone_acetylation);
    }

    /* Apply transcription factor regulation */
    for (uint32_t i = 0; i < sim->num_regulators; i++) {
        molbio_regulator_t* reg = &sim->regulators[i];
        if (!reg->active || reg->target_gene != gene_id) continue;
        if (reg->regulator_protein >= sim->num_proteins) continue;

        float tf_conc = sim->proteins[reg->regulator_protein].concentration;
        float binding = tf_conc / (reg->binding_affinity + tf_conc + CONC_EPSILON);

        switch (reg->type) {
        case MOLBIO_REG_ACTIVATOR:
        case MOLBIO_REG_ENHANCER:
            rate *= (1.0f + reg->fold_change * binding);
            break;
        case MOLBIO_REG_REPRESSOR:
        case MOLBIO_REG_SILENCER:
            rate *= 1.0f / (1.0f + reg->fold_change * binding);
            break;
        case MOLBIO_REG_MIRNA:
            /* Post-transcriptional: reduce existing mRNA, handled in step */
            break;
        }
    }

    sim->stats.transcriptions++;
    return rate;
}

/* ============================================================================
 * Translation: mRNA -> protein concentration change
 * ============================================================================ */

float molecular_biology_translate(molecular_biology_sim_t* sim, uint32_t gene_id)
{
    if (!sim || gene_id >= sim->num_genes) return 0.0f;
    molbio_gene_t* gene = &sim->genes[gene_id];
    if (!gene->active) return 0.0f;

    /* Translation rate proportional to mRNA level (expression_level) */
    float rate = gene->expression_level * sim->config.translation_rate;

    /* Find the protein encoded by this gene */
    for (uint32_t i = 0; i < sim->num_proteins; i++) {
        if (sim->proteins[i].gene_id == gene_id && sim->proteins[i].active) {
            sim->stats.translations++;
            return rate;
        }
    }
    return 0.0f;
}

/* ============================================================================
 * Step
 * ============================================================================ */

int molecular_biology_step(molecular_biology_sim_t* sim, float dt)
{
    if (!sim || !sim->initialized) return -1;
    if (dt <= 0.0f) dt = sim->config.dt;
    if (dt <= 0.0f) dt = 1.0f;

    /* --- Phase 1: Transcription (update mRNA/expression levels) --- */
    for (uint32_t i = 0; i < sim->num_genes; i++) {
        molbio_gene_t* gene = &sim->genes[i];
        if (!gene->active) continue;

        float txn_rate = molecular_biology_transcribe(sim, i);
        /* mRNA dynamics: d[mRNA]/dt = txn_rate - degradation * [mRNA] */
        float d_mrna = (txn_rate - sim->config.mrna_degradation_rate *
                        gene->expression_level) * dt;
        gene->expression_level += d_mrna;
        if (gene->expression_level < 0.0f) gene->expression_level = 0.0f;
        if (gene->expression_level > EXPRESSION_MAX) gene->expression_level = EXPRESSION_MAX;
    }

    /* --- Phase 2: Translation (update protein concentrations) --- */
    for (uint32_t i = 0; i < sim->num_proteins; i++) {
        molbio_protein_t* prot = &sim->proteins[i];
        if (!prot->active || prot->gene_id >= sim->num_genes) continue;

        float tln_rate = molecular_biology_translate(sim, prot->gene_id);
        /* Protein dynamics: d[P]/dt = tln_rate - degradation * [P] */
        float d_prot = (tln_rate - sim->config.protein_degradation_rate *
                        prot->concentration) * dt;
        prot->concentration += d_prot;
        if (prot->concentration < 0.0f) prot->concentration = 0.0f;

        /* Half-life based degradation (if half_life is specified) */
        if (prot->half_life > 0.0f) {
            float decay_rate = 0.693f / prot->half_life;  /* ln(2)/t_half */
            prot->concentration *= expf(-decay_rate * dt);
        }
    }

    /* --- Phase 3: miRNA post-transcriptional silencing --- */
    for (uint32_t i = 0; i < sim->num_regulators; i++) {
        molbio_regulator_t* reg = &sim->regulators[i];
        if (!reg->active || reg->type != MOLBIO_REG_MIRNA) continue;
        if (reg->target_gene >= sim->num_genes) continue;
        if (reg->regulator_protein >= sim->num_proteins) continue;

        float mirna_conc = sim->proteins[reg->regulator_protein].concentration;
        float binding = mirna_conc / (reg->binding_affinity + mirna_conc + CONC_EPSILON);
        /* Degrade target mRNA proportionally */
        sim->genes[reg->target_gene].expression_level *=
            (1.0f - reg->fold_change * binding * dt);
        if (sim->genes[reg->target_gene].expression_level < 0.0f)
            sim->genes[reg->target_gene].expression_level = 0.0f;
    }

    /* --- Update stats --- */
    sim->stats.step_count++;
    sim->time += dt;

    float total_mrna = 0.0f, total_protein = 0.0f;
    uint32_t active_genes = 0;
    for (uint32_t i = 0; i < sim->num_genes; i++) {
        if (sim->genes[i].active) {
            total_mrna += sim->genes[i].expression_level;
            if (sim->genes[i].expression_level > CONC_EPSILON) active_genes++;
        }
    }
    for (uint32_t i = 0; i < sim->num_proteins; i++) {
        if (sim->proteins[i].active) total_protein += sim->proteins[i].concentration;
    }
    sim->stats.total_mrna = total_mrna;
    sim->stats.total_protein = total_protein;
    sim->stats.active_genes = active_genes;

    return 0;
}

/* ============================================================================
 * Load lac operon
 * ============================================================================ */

void molecular_biology_load_lac_operon(molecular_biology_sim_t* sim)
{
    if (!sim) return;
    LOG_INFO(LOG_TAG, "Loading lac operon (lacZ, lacY, lacA + regulation)");

    /* Genes: lacZ (beta-galactosidase), lacY (permease), lacA (transacetylase) */
    const char* gene_names[] = { "lacZ", "lacY", "lacA" };
    const char* gene_seqs[] = {
        "ATGACCATGATTACGGATTCACTGGCCGTCGTT",  /* truncated representative seq */
        "ATGAGCAAAACAACCGAGATCTATCTGACCGTC",
        "ATGAATACTTTCAACATAACTGCGCCGGAAGTG"
    };
    uint32_t gene_ids[3];
    for (int i = 0; i < 3; i++) {
        molbio_gene_t g;
        memset(&g, 0, sizeof(g));
        strncpy(g.name, gene_names[i], MOLBIO_MAX_NAME - 1);
        strncpy(g.sequence, gene_seqs[i], MOLBIO_MAX_SEQ_LEN - 1);
        g.length = (uint32_t)strlen(gene_seqs[i]);
        g.expression_level = 0.01f;  /* low basal expression (repressed) */
        g.promoter_strength = 0.5f;
        g.is_coding = true;
        g.start_codon_pos = 0;
        g.stop_codon_pos = g.length - 3;
        g.methylation = 0.0f;
        g.histone_acetylation = 0.5f;
        gene_ids[i] = molecular_biology_add_gene(sim, &g);
    }

    /* Proteins: LacZ, LacY, LacA, LacI (repressor), CAP (activator) */
    struct {
        const char* name;
        uint32_t gene_id;
        float conc;
        float half_life;
        float mw;
        bool is_tf;
    } prot_defs[] = {
        { "beta-galactosidase", 0, 0.01f, 3600.0f, 116.0f, false },
        { "lactose_permease",   1, 0.01f, 3600.0f,  46.5f, false },
        { "transacetylase",     2, 0.01f, 3600.0f,  25.0f, false },
        { "LacI_repressor",  UINT32_MAX, 0.1f, 7200.0f, 38.6f, true },
        { "CAP_activator",   UINT32_MAX, 0.05f, 7200.0f, 22.0f, true },
    };

    uint32_t prot_ids[5];
    for (int i = 0; i < 5; i++) {
        molbio_protein_t p;
        memset(&p, 0, sizeof(p));
        strncpy(p.name, prot_defs[i].name, MOLBIO_MAX_NAME - 1);
        p.gene_id = (i < 3) ? gene_ids[prot_defs[i].gene_id] : UINT32_MAX;
        p.concentration = prot_defs[i].conc;
        p.half_life = prot_defs[i].half_life;
        p.molecular_weight = prot_defs[i].mw;
        p.is_transcription_factor = prot_defs[i].is_tf;
        prot_ids[i] = molecular_biology_add_protein(sim, &p);
    }

    /* Regulation: LacI represses all 3 lac genes, CAP activates them */
    for (int i = 0; i < 3; i++) {
        /* LacI repressor */
        molbio_regulator_t rep;
        memset(&rep, 0, sizeof(rep));
        rep.regulator_protein = prot_ids[3];  /* LacI */
        rep.target_gene = gene_ids[i];
        rep.type = MOLBIO_REG_REPRESSOR;
        rep.binding_affinity = 1.0e-9f;  /* very tight binding */
        rep.fold_change = 100.0f;        /* 100-fold repression */
        molecular_biology_add_regulator(sim, &rep);

        /* CAP activator */
        molbio_regulator_t act;
        memset(&act, 0, sizeof(act));
        act.regulator_protein = prot_ids[4];  /* CAP */
        act.target_gene = gene_ids[i];
        act.type = MOLBIO_REG_ACTIVATOR;
        act.binding_affinity = 1.0e-7f;
        act.fold_change = 50.0f;  /* 50-fold activation when glucose is low */
        molecular_biology_add_regulator(sim, &act);
    }

    LOG_INFO(LOG_TAG, "Lac operon loaded: %u genes, %u proteins, %u regulators",
             sim->num_genes, sim->num_proteins, sim->num_regulators);
}

/* ============================================================================
 * Stats
 * ============================================================================ */

molbio_stats_t molecular_biology_get_stats(const molecular_biology_sim_t* sim)
{
    molbio_stats_t zero;
    memset(&zero, 0, sizeof(zero));
    if (!sim) return zero;
    return sim->stats;
}
