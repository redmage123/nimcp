/**
 * @file nimcp_molecular_biology.h
 * @brief Molecular Biology — DNA/RNA, transcription, translation, gene regulation
 *
 * Central dogma: DNA→RNA→Protein. Codon tables, promoter binding,
 * operon regulation, splicing, CRISPR, epigenetics.
 */

#ifndef NIMCP_MOLECULAR_BIOLOGY_H
#define NIMCP_MOLECULAR_BIOLOGY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MOLBIO_MAX_GENES        64
#define MOLBIO_MAX_PROTEINS     64
#define MOLBIO_MAX_REGULATORS   32
#define MOLBIO_MAX_SEQ_LEN      1024
#define MOLBIO_MAX_NAME         32
#define MOLBIO_CODON_TABLE_SIZE 64

typedef enum { MOLBIO_BASE_A=0, MOLBIO_BASE_T=1, MOLBIO_BASE_G=2, MOLBIO_BASE_C=3, MOLBIO_BASE_U=4 } molbio_base_t;

typedef struct {
    uint32_t    id;
    char        name[MOLBIO_MAX_NAME];
    char        sequence[MOLBIO_MAX_SEQ_LEN];   /* ATCG string */
    uint32_t    length;
    float       expression_level;   /* [0..1] */
    float       promoter_strength;  /* basal transcription rate */
    bool        is_coding;          /* has ORF */
    uint32_t    start_codon_pos;    /* position of ATG */
    uint32_t    stop_codon_pos;
    /* Epigenetic state */
    float       methylation;        /* [0..1] (high = silenced) */
    float       histone_acetylation;/* [0..1] (high = active) */
    bool        active;
} molbio_gene_t;

typedef struct {
    uint32_t    id;
    char        name[MOLBIO_MAX_NAME];
    uint32_t    gene_id;            /* gene that encodes this protein */
    float       concentration;      /* mol/L */
    float       half_life;          /* seconds */
    float       molecular_weight;   /* kDa */
    bool        is_transcription_factor;
    bool        is_enzyme;
    bool        active;
} molbio_protein_t;

typedef enum {
    MOLBIO_REG_ACTIVATOR    = 0,
    MOLBIO_REG_REPRESSOR    = 1,
    MOLBIO_REG_ENHANCER     = 2,
    MOLBIO_REG_SILENCER     = 3,
    MOLBIO_REG_MIRNA        = 4,    /* post-transcriptional silencing */
} molbio_regulation_t;

typedef struct {
    uint32_t            regulator_protein;  /* TF protein id */
    uint32_t            target_gene;
    molbio_regulation_t type;
    float               binding_affinity;   /* Kd (mol/L) */
    float               fold_change;        /* effect magnitude */
    bool                active;
} molbio_regulator_t;

typedef struct {
    float       transcription_rate;     /* base pairs/s */
    float       translation_rate;       /* amino acids/s */
    float       mrna_degradation_rate;  /* 1/s */
    float       protein_degradation_rate;
    float       dt;
    bool        enable_epigenetics;
    bool        enable_splicing;
} molbio_config_t;

typedef struct {
    uint64_t    step_count;
    uint64_t    transcriptions;
    uint64_t    translations;
    float       total_mrna;
    float       total_protein;
    uint32_t    active_genes;
} molbio_stats_t;

typedef struct molecular_biology_sim {
    molbio_gene_t       genes[MOLBIO_MAX_GENES];
    uint32_t            num_genes;
    molbio_protein_t    proteins[MOLBIO_MAX_PROTEINS];
    uint32_t            num_proteins;
    molbio_regulator_t  regulators[MOLBIO_MAX_REGULATORS];
    uint32_t            num_regulators;
    /* Codon table: index = codon (0-63), value = amino acid char */
    char                codon_table[MOLBIO_CODON_TABLE_SIZE];
    molbio_config_t     config;
    molbio_stats_t      stats;
    float               time;
    bool                initialized;
} molecular_biology_sim_t;

molecular_biology_sim_t* molecular_biology_create(const molbio_config_t* config);
void molecular_biology_destroy(molecular_biology_sim_t* sim);
uint32_t molecular_biology_add_gene(molecular_biology_sim_t* sim, const molbio_gene_t* g);
uint32_t molecular_biology_add_protein(molecular_biology_sim_t* sim, const molbio_protein_t* p);
uint32_t molecular_biology_add_regulator(molecular_biology_sim_t* sim, const molbio_regulator_t* r);
int molecular_biology_step(molecular_biology_sim_t* sim, float dt);

/** Transcribe gene → mRNA (returns expression level change) */
float molecular_biology_transcribe(molecular_biology_sim_t* sim, uint32_t gene_id);
/** Translate mRNA → protein (returns protein concentration change) */
float molecular_biology_translate(molecular_biology_sim_t* sim, uint32_t gene_id);
/** Translate codon to amino acid */
char molecular_biology_codon_to_aa(const molecular_biology_sim_t* sim, char b1, char b2, char b3);
/** Compute GC content of a sequence */
float molecular_biology_gc_content(const char* sequence, uint32_t length);
/** Compute melting temperature: Tm = 64.9 + 41*(nG+nC-16.4)/(nA+nT+nG+nC) */
float molecular_biology_melting_temp(const char* sequence, uint32_t length);

void molecular_biology_load_codon_table(molecular_biology_sim_t* sim);
void molecular_biology_load_lac_operon(molecular_biology_sim_t* sim);
molbio_config_t molecular_biology_default_config(void);
molbio_stats_t molecular_biology_get_stats(const molecular_biology_sim_t* sim);

#ifdef __cplusplus
}
#endif
#endif
