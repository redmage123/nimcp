/**
 * @file nimcp_inorganic_chemistry.c
 * @brief Inorganic Chemistry simulator — crystal field theory, coordination chemistry
 *
 * Crystal field splitting (Delta_oct), CFSE, spectrochemical series,
 * HSAB stability, Jahn-Teller distortion, magnetic moment,
 * Irving-Williams series, geometry prediction.
 */

#include "cognitive/physics/nimcp_inorganic_chemistry.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <math.h>
#include <string.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define LOG_TAG "INOCHEM"

#define DEFAULT_TEMP        298.15f
#define DEFAULT_PAIRING_E   15000.0f  /* cm^-1, typical pairing energy */

/* ============================================================================
 * Electron filling tables
 *
 * Octahedral high-spin: t2g before eg
 * t2g max = 3, then eg; high-spin fills one per orbital first
 * Low-spin: fill t2g completely (6) before eg
 *
 * Returns (t2g_count, eg_count) for octahedral geometry
 * ============================================================================ */

static void oct_filling(int d, inochem_spin_state_t spin, int* t2g, int* eg)
{
    *t2g = 0; *eg = 0;
    if (d <= 0 || d > 10) return;

    if (spin == INOCHEM_SPIN_HIGH) {
        /* High-spin: fill one per orbital first (Hund's rule), then pair */
        /* t2g has 3 orbitals, eg has 2 orbitals => 5 slots unpaired */
        if (d <= 3)      { *t2g = d; *eg = 0; }
        else if (d <= 5) { *t2g = 3; *eg = d - 3; }
        else if (d <= 8) { *t2g = d - 2; *eg = 2; }  /* pairing starts in t2g */
        else             { *t2g = 6; *eg = d - 6; }
        /* Fix: re-derive properly */
        /* d1-d3: t2g^d, eg^0 */
        /* d4: t2g^3, eg^1 (HS) */
        /* d5: t2g^3, eg^2 (HS) */
        /* d6: t2g^4, eg^2 (HS) */
        /* d7: t2g^5, eg^2 (HS) */
        /* d8: t2g^6, eg^2 (always) */
        /* d9: t2g^6, eg^3 */
        /* d10: t2g^6, eg^4 */
        if (d == 4) { *t2g = 3; *eg = 1; }
        else if (d == 5) { *t2g = 3; *eg = 2; }
        else if (d == 6) { *t2g = 4; *eg = 2; }
        else if (d == 7) { *t2g = 5; *eg = 2; }
        else if (d == 8) { *t2g = 6; *eg = 2; }
        else if (d == 9) { *t2g = 6; *eg = 3; }
        else if (d == 10) { *t2g = 6; *eg = 4; }
    } else {
        /* Low-spin: fill t2g fully before eg */
        if (d <= 6) { *t2g = d; *eg = 0; }
        else        { *t2g = 6; *eg = d - 6; }
    }
}

/* Unpaired electron count from orbital filling */
static int count_unpaired_oct(int d, inochem_spin_state_t spin)
{
    if (d <= 0 || d > 10) return 0;

    if (spin == INOCHEM_SPIN_HIGH) {
        /* High-spin octahedral unpaired counts: d1=1,d2=2,d3=3,d4=4,d5=5,d6=4,d7=3,d8=2,d9=1,d10=0 */
        static const int hs_unpaired[] = { 0, 1, 2, 3, 4, 5, 4, 3, 2, 1, 0 };
        return hs_unpaired[d];
    } else {
        /* Low-spin octahedral: d1=1,d2=2,d3=3,d4=2,d5=1,d6=0,d7=1,d8=2,d9=1,d10=0 */
        static const int ls_unpaired[] = { 0, 1, 2, 3, 2, 1, 0, 1, 2, 1, 0 };
        return ls_unpaired[d];
    }
}

/* Tetrahedral: smaller splitting, always high-spin (Delta_tet ~ 4/9 Delta_oct) */
static int count_unpaired_tet(int d)
{
    if (d <= 0 || d > 10) return 0;
    /* Tetrahedral: e before t2 (inverted from oct) */
    /* Always high-spin: d1=1,d2=2,d3=3,d4=4,d5=5,d6=4,d7=3,d8=2,d9=1,d10=0 */
    static const int tet_unpaired[] = { 0, 1, 2, 3, 4, 5, 4, 3, 2, 1, 0 };
    return tet_unpaired[d];
}

/* ============================================================================
 * Default config
 * ============================================================================ */

inorganic_chemistry_config_t inorganic_chemistry_default_config(void)
{
    inorganic_chemistry_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.dt = 1.0f;
    cfg.temperature = DEFAULT_TEMP;
    cfg.enabled = true;
    return cfg;
}

/* ============================================================================
 * Create / Destroy
 * ============================================================================ */

inorganic_chemistry_sim_t* inorganic_chemistry_create(const inorganic_chemistry_config_t* config)
{
    inorganic_chemistry_sim_t* sim =
        (inorganic_chemistry_sim_t*)nimcp_calloc(1, sizeof(inorganic_chemistry_sim_t));
    if (!sim) {
        LOG_ERROR(LOG_TAG, "Failed to allocate inorganic_chemistry_sim_t");
        return NULL;
    }
    sim->config = config ? *config : inorganic_chemistry_default_config();

    /* Initialize spectrochemical series (cm^-1, for Cr3+ reference) */
    sim->spectro_series[INOCHEM_LIG_IODIDE]    = INOCHEM_DQ_IODIDE;
    sim->spectro_series[INOCHEM_LIG_BROMIDE]    = INOCHEM_DQ_BROMIDE;
    sim->spectro_series[INOCHEM_LIG_CHLORIDE]   = INOCHEM_DQ_CHLORIDE;
    sim->spectro_series[INOCHEM_LIG_FLUORIDE]   = INOCHEM_DQ_FLUORIDE;
    sim->spectro_series[INOCHEM_LIG_HYDROXIDE]  = INOCHEM_DQ_HYDROXIDE;
    sim->spectro_series[INOCHEM_LIG_WATER]      = INOCHEM_DQ_WATER;
    sim->spectro_series[INOCHEM_LIG_AMMONIA]    = INOCHEM_DQ_AMMONIA;
    sim->spectro_series[INOCHEM_LIG_EN]         = INOCHEM_DQ_EN;
    sim->spectro_series[INOCHEM_LIG_NITRITE]    = INOCHEM_DQ_NITRITE;
    sim->spectro_series[INOCHEM_LIG_CYANIDE]    = INOCHEM_DQ_CYANIDE;
    sim->spectro_series[INOCHEM_LIG_CO]         = INOCHEM_DQ_CO;
    sim->spectro_series[INOCHEM_LIG_NO_PLUS]    = INOCHEM_DQ_NO_PLUS;

    sim->initialized = true;
    LOG_INFO(LOG_TAG, "Inorganic chemistry sim created (T=%.1f K)", sim->config.temperature);
    return sim;
}

void inorganic_chemistry_destroy(inorganic_chemistry_sim_t* sim)
{
    if (!sim) return;
    LOG_INFO(LOG_TAG, "Destroying inorganic chemistry sim (steps=%lu)",
             (unsigned long)sim->stats.step_count);
    nimcp_free(sim);
}

/* ============================================================================
 * Entity management
 * ============================================================================ */

uint32_t inorganic_chemistry_add_metal(inorganic_chemistry_sim_t* sim, const inochem_metal_t* m)
{
    if (!sim || !m) return UINT32_MAX;
    if (sim->num_metals >= INOCHEM_MAX_METALS) return UINT32_MAX;
    uint32_t idx = sim->num_metals++;
    sim->metals[idx] = *m;
    sim->metals[idx].id = idx;
    sim->metals[idx].active = true;
    return idx;
}

uint32_t inorganic_chemistry_add_ligand(inorganic_chemistry_sim_t* sim, const inochem_ligand_t* l)
{
    if (!sim || !l) return UINT32_MAX;
    if (sim->num_ligands >= INOCHEM_MAX_LIGANDS) return UINT32_MAX;
    uint32_t idx = sim->num_ligands++;
    sim->ligands[idx] = *l;
    sim->ligands[idx].id = idx;
    sim->ligands[idx].active = true;
    return idx;
}

uint32_t inorganic_chemistry_add_complex(inorganic_chemistry_sim_t* sim, const inochem_complex_t* c)
{
    if (!sim || !c) return UINT32_MAX;
    if (sim->num_complexes >= INOCHEM_MAX_COMPLEXES) return UINT32_MAX;
    uint32_t idx = sim->num_complexes++;
    sim->complexes[idx] = *c;
    sim->complexes[idx].id = idx;
    sim->complexes[idx].active = true;
    return idx;
}

/* ============================================================================
 * Load common complexes
 * ============================================================================ */

int inorganic_chemistry_load_common_complexes(inorganic_chemistry_sim_t* sim)
{
    if (!sim) return -1;

    /* Metals: first-row transition metals */
    static const struct { const char* name; inochem_metal_id_t type; int ox; int d; float rad; float en; inochem_hsab_class_t hsab; float iw; } metal_db[] = {
        { "Ti(III)",  INOCHEM_METAL_TI,  3, 1, 67.0f, 1.54f, INOCHEM_HSAB_HARD,       1.0f },
        { "Cr(III)",  INOCHEM_METAL_CR,  3, 3, 62.0f, 1.66f, INOCHEM_HSAB_HARD,       3.0f },
        { "Mn(II)",   INOCHEM_METAL_MN2, 2, 5, 83.0f, 1.55f, INOCHEM_HSAB_HARD,       4.0f },
        { "Fe(II)",   INOCHEM_METAL_FE2, 2, 6, 78.0f, 1.83f, INOCHEM_HSAB_BORDERLINE, 5.0f },
        { "Fe(III)",  INOCHEM_METAL_FE3, 3, 5, 65.0f, 1.83f, INOCHEM_HSAB_HARD,       4.5f },
        { "Co(II)",   INOCHEM_METAL_CO2, 2, 7, 75.0f, 1.88f, INOCHEM_HSAB_BORDERLINE, 6.0f },
        { "Co(III)",  INOCHEM_METAL_CO3, 3, 6, 55.0f, 1.88f, INOCHEM_HSAB_HARD,       7.0f },
        { "Ni(II)",   INOCHEM_METAL_NI,  2, 8, 69.0f, 1.91f, INOCHEM_HSAB_BORDERLINE, 8.0f },
        { "Cu(II)",   INOCHEM_METAL_CU2, 2, 9, 73.0f, 1.90f, INOCHEM_HSAB_BORDERLINE, 10.0f },
        { "Zn(II)",   INOCHEM_METAL_ZN,  2, 10, 74.0f, 1.65f, INOCHEM_HSAB_BORDERLINE, 7.0f },
    };

    for (uint32_t i = 0; i < sizeof(metal_db)/sizeof(metal_db[0]); i++) {
        inochem_metal_t m;
        memset(&m, 0, sizeof(m));
        strncpy(m.name, metal_db[i].name, INOCHEM_MAX_NAME - 1);
        m.type = metal_db[i].type;
        m.oxidation_state = metal_db[i].ox;
        m.d_electrons = metal_db[i].d;
        m.ionic_radius = metal_db[i].rad;
        m.electronegativity = metal_db[i].en;
        m.hsab = metal_db[i].hsab;
        m.irving_williams = metal_db[i].iw;
        inorganic_chemistry_add_metal(sim, &m);
    }

    /* Ligands */
    static const struct { const char* name; inochem_ligand_id_t type; float fs; int dent; int chg; inochem_hsab_class_t hsab; bool pi_acc; bool pi_don; } lig_db[] = {
        { "I-",    INOCHEM_LIG_IODIDE,    INOCHEM_DQ_IODIDE,    1, -1, INOCHEM_HSAB_SOFT,       false, true },
        { "Br-",   INOCHEM_LIG_BROMIDE,   INOCHEM_DQ_BROMIDE,   1, -1, INOCHEM_HSAB_SOFT,       false, true },
        { "Cl-",   INOCHEM_LIG_CHLORIDE,  INOCHEM_DQ_CHLORIDE,  1, -1, INOCHEM_HSAB_HARD,       false, true },
        { "F-",    INOCHEM_LIG_FLUORIDE,  INOCHEM_DQ_FLUORIDE,  1, -1, INOCHEM_HSAB_HARD,       false, true },
        { "OH-",   INOCHEM_LIG_HYDROXIDE, INOCHEM_DQ_HYDROXIDE, 1, -1, INOCHEM_HSAB_HARD,       false, true },
        { "H2O",   INOCHEM_LIG_WATER,     INOCHEM_DQ_WATER,     1,  0, INOCHEM_HSAB_HARD,       false, false },
        { "NH3",   INOCHEM_LIG_AMMONIA,   INOCHEM_DQ_AMMONIA,   1,  0, INOCHEM_HSAB_BORDERLINE, false, false },
        { "en",    INOCHEM_LIG_EN,        INOCHEM_DQ_EN,        2,  0, INOCHEM_HSAB_BORDERLINE, false, false },
        { "NO2-",  INOCHEM_LIG_NITRITE,   INOCHEM_DQ_NITRITE,   1, -1, INOCHEM_HSAB_BORDERLINE, true,  false },
        { "CN-",   INOCHEM_LIG_CYANIDE,   INOCHEM_DQ_CYANIDE,   1, -1, INOCHEM_HSAB_SOFT,       true,  false },
        { "CO",    INOCHEM_LIG_CO,        INOCHEM_DQ_CO,        1,  0, INOCHEM_HSAB_SOFT,       true,  false },
    };

    for (uint32_t i = 0; i < sizeof(lig_db)/sizeof(lig_db[0]); i++) {
        inochem_ligand_t l;
        memset(&l, 0, sizeof(l));
        strncpy(l.name, lig_db[i].name, INOCHEM_MAX_NAME - 1);
        l.type = lig_db[i].type;
        l.field_strength = lig_db[i].fs;
        l.denticity = lig_db[i].dent;
        l.charge = lig_db[i].chg;
        l.hsab = lig_db[i].hsab;
        l.is_pi_acceptor = lig_db[i].pi_acc;
        l.is_pi_donor = lig_db[i].pi_don;
        inorganic_chemistry_add_ligand(sim, &l);
    }

    LOG_INFO(LOG_TAG, "Loaded %u metals and %u ligands", sim->num_metals, sim->num_ligands);
    return 0;
}

/* ============================================================================
 * Crystal field splitting: average ligand field strengths
 * Delta_oct = average of ligand field strengths (scaled by metal factor)
 * ============================================================================ */

float inorganic_chemistry_compute_delta_oct(const inochem_metal_t* metal,
                                             const inochem_ligand_t* ligands,
                                             uint32_t num_ligands)
{
    if (!metal || !ligands || num_ligands == 0) return 0.0f;

    float sum_fs = 0.0f;
    for (uint32_t i = 0; i < num_ligands; i++) {
        sum_fs += ligands[i].field_strength;
    }
    /* Average field strength, scaled by oxidation state (higher charge = larger splitting) */
    float avg = sum_fs / (float)num_ligands;
    float metal_factor = 1.0f + 0.1f * (float)(metal->oxidation_state - 2);
    return avg * metal_factor;
}

/* ============================================================================
 * CFSE = (-0.4 * n_t2g + 0.6 * n_eg) * Delta_oct (in Dq units)
 * Returns CFSE in same units as input (cm^-1 if Delta is in cm^-1)
 * Negative CFSE = stabilization
 * ============================================================================ */

float inorganic_chemistry_compute_cfse(int d_electrons, inochem_spin_state_t spin,
                                        inochem_geometry_t geom)
{
    if (d_electrons <= 0 || d_electrons > 10) return 0.0f;

    if (geom == INOCHEM_GEOM_OCTAHEDRAL || geom == INOCHEM_GEOM_SQUARE_PLANAR) {
        int t2g, eg;
        oct_filling(d_electrons, spin, &t2g, &eg);
        /* CFSE in Dq: t2g = -4Dq each, eg = +6Dq each => CFSE = (-4*t2g + 6*eg)*Dq/10 */
        /* Using the -0.4/+0.6 convention in Delta_oct units */
        return (-0.4f * (float)t2g + 0.6f * (float)eg);
    } else if (geom == INOCHEM_GEOM_TETRAHEDRAL) {
        /* Tetrahedral: Delta_tet ~ 4/9 * Delta_oct, inverted levels */
        /* e = -6Dq_tet, t2 = +4Dq_tet */
        /* For simplicity, return in Delta_tet units */
        int e_count, t2_count;
        /* In tet: e fills first (lower), then t2 */
        if (d_electrons <= 4) { e_count = d_electrons; t2_count = 0; }
        else if (d_electrons <= 6) { e_count = 4; t2_count = d_electrons - 4; }
        else { e_count = 4; t2_count = d_electrons - 4; }
        /* Actually: e has 2 orbitals (4 max), t2 has 3 orbitals (6 max) */
        if (d_electrons <= 2) { e_count = d_electrons; t2_count = 0; }
        else if (d_electrons <= 5) { e_count = 2; t2_count = d_electrons - 2; }
        else if (d_electrons <= 7) { e_count = d_electrons - 3; t2_count = 3; }
        else { e_count = 4; t2_count = d_electrons - 4; }
        return (-0.6f * (float)e_count + 0.4f * (float)t2_count) * (4.0f/9.0f);
    }

    return 0.0f;
}

/* ============================================================================
 * Predict spin state from Delta and pairing energy
 * If Delta > pairing_energy: low-spin; else: high-spin
 * ============================================================================ */

inochem_spin_state_t inorganic_chemistry_predict_spin(int d_electrons, float delta_oct,
                                                       float pairing_energy)
{
    /* d1-d3 and d8-d10: spin state is same regardless */
    if (d_electrons <= 3 || d_electrons >= 8) return INOCHEM_SPIN_HIGH;

    if (pairing_energy <= 0.0f) pairing_energy = DEFAULT_PAIRING_E;

    return (delta_oct > pairing_energy) ? INOCHEM_SPIN_LOW : INOCHEM_SPIN_HIGH;
}

/* ============================================================================
 * Magnetic moment: mu = sqrt(n*(n+2)) Bohr magnetons (spin-only formula)
 * ============================================================================ */

float inorganic_chemistry_magnetic_moment(int unpaired_electrons)
{
    if (unpaired_electrons <= 0) return 0.0f;
    float n = (float)unpaired_electrons;
    return sqrtf(n * (n + 2.0f));
}

/* ============================================================================
 * Unpaired electrons for given configuration
 * ============================================================================ */

int inorganic_chemistry_unpaired_electrons(int d_electrons, inochem_spin_state_t spin,
                                            inochem_geometry_t geom)
{
    if (geom == INOCHEM_GEOM_TETRAHEDRAL) {
        return count_unpaired_tet(d_electrons);
    }
    return count_unpaired_oct(d_electrons, spin);
}

/* ============================================================================
 * Jahn-Teller distortion
 * Occurs when degenerate orbitals are unevenly occupied.
 * Strong JT: d4(HS), d9 octahedral (unequal eg filling)
 * Weak JT: d1, d2, d6(HS), d7(HS) (unequal t2g filling)
 * No JT: d0, d3, d5(HS), d8, d10
 * ============================================================================ */

bool inorganic_chemistry_is_jahn_teller(int d_electrons, inochem_spin_state_t spin,
                                         inochem_geometry_t geom)
{
    if (geom != INOCHEM_GEOM_OCTAHEDRAL) return false;

    if (spin == INOCHEM_SPIN_HIGH) {
        /* Strong JT: d4(HS has eg^1), d9(eg^3) */
        /* Weak JT: d1(t2g^1), d2(t2g^2), d6(t2g^4), d7(t2g^5) */
        switch (d_electrons) {
        case 1: case 2: case 6: case 7: return true;   /* weak */
        case 4: case 9: return true;                     /* strong */
        default: return false;
        }
    } else {
        /* Low-spin: JT for d4(LS: t2g^4), d5(LS: t2g^5), d7(LS: t2g^6 eg^1), d9 */
        switch (d_electrons) {
        case 4: case 5: case 7: case 9: return true;
        default: return false;
        }
    }
}

/* ============================================================================
 * HSAB stability prediction
 * Hard-hard and soft-soft interactions are more stable
 * ============================================================================ */

float inorganic_chemistry_hsab_stability(inochem_hsab_class_t acid, inochem_hsab_class_t base)
{
    /* Matching classes: high stability */
    if (acid == base) return 10.0f;

    /* Adjacent classes: moderate */
    int diff = (int)acid - (int)base;
    if (diff < 0) diff = -diff;
    if (diff == 1) return 5.0f;

    /* Hard-soft mismatch: low stability */
    return 1.0f;
}

/* ============================================================================
 * Predict geometry from d-electron count and coordination number
 * ============================================================================ */

inochem_geometry_t inorganic_chemistry_predict_geometry(int d_electrons, int coord_number)
{
    switch (coord_number) {
    case 2: return INOCHEM_GEOM_LINEAR;
    case 3: return INOCHEM_GEOM_TRIGONAL_PLANAR;
    case 4:
        /* d8 with strong-field ligands: square planar (e.g., Pt2+, Pd2+, Ni2+ with CN-) */
        if (d_electrons == 8) return INOCHEM_GEOM_SQUARE_PLANAR;
        return INOCHEM_GEOM_TETRAHEDRAL;
    case 5:
        return INOCHEM_GEOM_TRIG_BIPYRAMIDAL;
    case 6:
    default:
        return INOCHEM_GEOM_OCTAHEDRAL;
    }
}

/* ============================================================================
 * Analyze a complex: compute all properties
 * ============================================================================ */

int inorganic_chemistry_analyze_complex(inorganic_chemistry_sim_t* sim, uint32_t complex_idx)
{
    if (!sim || complex_idx >= sim->num_complexes) return -1;

    inochem_complex_t* cx = &sim->complexes[complex_idx];
    if (cx->metal_idx >= sim->num_metals) return -1;

    inochem_metal_t* metal = &sim->metals[cx->metal_idx];

    /* Gather ligands */
    inochem_ligand_t lig_array[INOCHEM_MAX_COORD];
    uint32_t valid_ligs = 0;
    for (uint32_t i = 0; i < cx->num_ligands && i < INOCHEM_MAX_COORD; i++) {
        if (cx->ligand_indices[i] < sim->num_ligands) {
            lig_array[valid_ligs++] = sim->ligands[cx->ligand_indices[i]];
        }
    }

    /* Predict geometry */
    cx->geometry = inorganic_chemistry_predict_geometry(metal->d_electrons, (int)valid_ligs);

    /* Compute Delta_oct */
    cx->delta_oct = inorganic_chemistry_compute_delta_oct(metal, lig_array, valid_ligs);

    /* Predict spin state */
    cx->spin_state = inorganic_chemistry_predict_spin(metal->d_electrons, cx->delta_oct,
                                                       DEFAULT_PAIRING_E);

    /* CFSE */
    cx->cfse = inorganic_chemistry_compute_cfse(metal->d_electrons, cx->spin_state, cx->geometry);

    /* Unpaired electrons and magnetic moment */
    cx->unpaired_electrons = inorganic_chemistry_unpaired_electrons(
        metal->d_electrons, cx->spin_state, cx->geometry);
    cx->magnetic_moment = inorganic_chemistry_magnetic_moment(cx->unpaired_electrons);

    /* Jahn-Teller */
    cx->jahn_teller = inorganic_chemistry_is_jahn_teller(
        metal->d_electrons, cx->spin_state, cx->geometry);
    /* Strong JT for d4(HS) and d9 */
    cx->jahn_teller_strong = cx->jahn_teller &&
        (metal->d_electrons == 4 || metal->d_electrons == 9);

    /* HSAB stability: average over all ligands */
    float stab_sum = 0.0f;
    for (uint32_t i = 0; i < valid_ligs; i++) {
        stab_sum += inorganic_chemistry_hsab_stability(metal->hsab, lig_array[i].hsab);
    }
    cx->stability_constant = (valid_ligs > 0) ? stab_sum / (float)valid_ligs : 0.0f;

    /* Scale by Irving-Williams series */
    cx->stability_constant *= metal->irving_williams * 0.5f;

    /* Compute total charge */
    cx->total_charge = metal->oxidation_state;
    for (uint32_t i = 0; i < valid_ligs; i++) {
        cx->total_charge += lig_array[i].charge;
    }

    return 0;
}

/* ============================================================================
 * Step — analyze all active complexes, update stats
 * ============================================================================ */

int inorganic_chemistry_step(inorganic_chemistry_sim_t* sim, float dt)
{
    if (!sim || !sim->initialized) return -1;
    if (dt <= 0.0f) dt = sim->config.dt;

    float sum_cfse = 0.0f, sum_mu = 0.0f;
    uint32_t active = 0;

    for (uint32_t i = 0; i < sim->num_complexes; i++) {
        if (!sim->complexes[i].active) continue;

        inorganic_chemistry_analyze_complex(sim, i);

        sum_cfse += sim->complexes[i].cfse;
        sum_mu   += sim->complexes[i].magnetic_moment;
        active++;
    }

    if (active > 0) {
        sim->stats.avg_cfse = sum_cfse / (float)active;
        sim->stats.avg_magnetic_moment = sum_mu / (float)active;
    }

    sim->stats.step_count++;
    sim->time += dt;
    return 0;
}

/* ============================================================================
 * Stats
 * ============================================================================ */

inorganic_chemistry_stats_t inorganic_chemistry_get_stats(const inorganic_chemistry_sim_t* sim)
{
    inorganic_chemistry_stats_t zero;
    memset(&zero, 0, sizeof(zero));
    if (!sim) return zero;
    return sim->stats;
}
