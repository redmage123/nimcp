/**
 * @file nimcp_retina.h
 * @brief Extended-Spectrum Photoreceptor Layer - UV, NIR, and Thermal-IR Detection
 *
 * WHAT: Biologically-inspired retinal layer with extended wavelength detection
 * WHY:  Enable NIMCP to perceive non-visible wavelengths for enhanced environmental awareness
 * HOW:  UV opsin (SWS1), NIR quantum dots, thermal-IR pit organ, multispectral fusion
 *
 * WAVELENGTH COVERAGE:
 * - UV (Ultraviolet):    300-400nm  - SWS1 opsin (bird-like vision)
 * - Visible (RGB):       400-700nm  - Standard cone/rod photoreceptors
 * - NIR (Near-Infrared): 700-1100nm - Quantum dot semiconductor detectors
 * - LWIR (Thermal-IR):   8-14µm     - Pit organ thermoreceptors (snake-like)
 * - X-ray (placeholder): <10nm      - Future extension
 *
 * BIOLOGICAL INSPIRATION:
 *
 * 1. UV VISION (Birds, Bees, Butterflies):
 *    - SWS1 opsin with 360nm peak sensitivity
 *    - Enables nectar guide detection, UV markings, polarization
 *    - Spectral response: Gaussian with λ_peak=360nm, σ=40nm
 *
 * 2. NIR VISION (Quantum Dot Layer):
 *    - Inspired by some fish, deep-sea creatures
 *    - Quantum dots with tunable bandgap (PbS, InGaAs)
 *    - 700-1100nm detection for night vision, thermal contrast
 *    - Bandgap energy: E_g = hc/λ, photoelectric conversion
 *
 * 3. THERMAL-IR (Snake Pit Organs):
 *    - Trigeminal nerve thermoreceptors (pythons, vipers)
 *    - Stefan-Boltzmann detection: radiant power ∝ T⁴
 *    - 8-14µm atmospheric window (LWIR)
 *    - Temperature resolution: ~0.003°C (rattlesnake sensitivity)
 *
 * MULTISPECTRAL FUSION ARCHITECTURE:
 *
 *   UV Layer (360nm)  ──┐
 *   Blue Cone (450nm) ──┤
 *   Green Cone (535nm)──┤
 *   Red Cone (565nm)  ──┼──> Multispectral Fusion ──> Feature Vector
 *   NIR QD (850nm)    ──┤         (7 channels)
 *   Thermal (10µm)    ──┤
 *   X-ray (future)    ──┘
 *
 * USE CASES:
 * - UV: Nectar guides, UV markings, mineral fluorescence, arc detection
 * - NIR: Night vision, thermal contrast, camouflage penetration
 * - Thermal: Warm body detection, temperature gradients, heat signatures
 * - Multispectral: Material identification, chemical analysis, threat detection
 *
 * PHYSICS CONSTANTS:
 * - Planck constant: h = 6.62607015e-34 J·s
 * - Speed of light: c = 299792458 m/s
 * - Boltzmann constant: k_B = 1.380649e-23 J/K
 * - Stefan-Boltzmann: σ = 5.670374419e-8 W·m⁻²·K⁻⁴
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 3.0 (Extended-Spectrum Retina)
 */

#ifndef NIMCP_RETINA_H
#define NIMCP_RETINA_H

#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Physical Constants
//=============================================================================

/** Planck constant (J·s) */
#define RETINA_PLANCK_CONSTANT 6.62607015e-34

/** Speed of light (m/s) */
#define RETINA_LIGHT_SPEED 299792458.0

/** Boltzmann constant (J/K) */
#define RETINA_BOLTZMANN_CONSTANT 1.380649e-23

/** Stefan-Boltzmann constant (W·m⁻²·K⁻⁴) */
#define RETINA_STEFAN_BOLTZMANN 5.670374419e-8

/** Wien's displacement constant (m·K) */
#define RETINA_WIEN_CONSTANT 2.897771955e-3

//=============================================================================
// Wavelength Bands
//=============================================================================

/** UV wavelength range (nm) */
#define RETINA_UV_MIN_NM 300.0f
#define RETINA_UV_MAX_NM 400.0f
#define RETINA_UV_PEAK_NM 360.0f  /**< SWS1 opsin peak */

/** Visible wavelength range (nm) */
#define RETINA_VISIBLE_MIN_NM 400.0f
#define RETINA_VISIBLE_MAX_NM 700.0f

/** NIR wavelength range (nm) */
#define RETINA_NIR_MIN_NM 700.0f
#define RETINA_NIR_MAX_NM 1100.0f
#define RETINA_NIR_PEAK_NM 850.0f  /**< Optimal NIR detection */

/** Thermal-IR wavelength range (µm) */
#define RETINA_THERMAL_MIN_UM 8.0f
#define RETINA_THERMAL_MAX_UM 14.0f
#define RETINA_THERMAL_PEAK_UM 10.0f  /**< Peak atmospheric transmission */

/** Number of spectral channels */
#define RETINA_NUM_CHANNELS 7

//=============================================================================
// Photoreceptor Types
//=============================================================================

/**
 * @brief Extended photoreceptor types
 */
typedef enum {
    PHOTORECEPTOR_ROD,           /**< Low-light scotopic vision (498nm peak) */
    PHOTORECEPTOR_CONE_S,        /**< Short-wave cone (blue, 420nm) */
    PHOTORECEPTOR_CONE_M,        /**< Medium-wave cone (green, 534nm) */
    PHOTORECEPTOR_CONE_L,        /**< Long-wave cone (red, 564nm) */
    PHOTORECEPTOR_CONE_UV,       /**< UV-sensitive SWS1 opsin (360nm) */
    PHOTORECEPTOR_CONE_NIR,      /**< NIR quantum dot detector (850nm) */
    PHOTORECEPTOR_THERMORECEPTOR_LWIR, /**< Thermal-IR pit organ (8-14µm) */
    PHOTORECEPTOR_XRAY_PLACEHOLDER     /**< Future X-ray detection */
} photoreceptor_type_t;

//=============================================================================
// Enhanced Vision Modes
//=============================================================================

/**
 * @brief Vision enhancement modes inspired by animals
 */
typedef enum {
    VISION_MODE_NORMAL,    /**< Standard human-like vision */
    VISION_MODE_EAGLE,     /**< Eagle-enhanced: 4-5x acuity, motion tracking, UV vision */
    VISION_MODE_CAT        /**< Cat-enhanced: Night vision, tapetum lucidum, motion hunting */
} vision_mode_t;

/**
 * @brief Eagle vision enhancements
 *
 * BIOLOGY (Aquila chrysaetos - Golden Eagle):
 * - Foveal cone density: 1,000,000 cones/mm² (vs 200,000 in humans)
 * - Visual acuity: 20/4 (5x sharper than human 20/20)
 * - Two foveae per eye: central (forward) + temporal (lateral)
 * - Deep foveal pit for magnification effect
 * - UV sensitivity via SWS1 cone
 * - Motion detection: specialized retinal ganglion cells
 */
typedef struct {
    float acuity_multiplier;      /**< Visual acuity boost: 4.0-5.0x normal */
    float foveal_magnification;   /**< Foveal pit magnification: 1.3-1.5x */
    float motion_sensitivity;     /**< Motion detection gain: 2.0-3.0x */
    float depth_perception_range; /**< Stereo vision range multiplier: 1.5x */
    bool dual_fovea_enabled;      /**< Enable dual fovea simulation */
} eagle_vision_config_t;

/**
 * @brief Cat vision enhancements
 *
 * BIOLOGY (Felis catus - Domestic Cat):
 * - Tapetum lucidum: Reflective layer behind retina (40-130% light reflection)
 * - Rod-dominated retina: 25:1 rod-to-cone ratio (vs 20:1 in humans)
 * - Slit pupil: Vertical slit for precise light control
 * - Dichromatic color: Blue and green cones only (no red sensitivity)
 * - Field of view: 200° (vs 180° in humans)
 * - Motion tracking: Optimized for hunting small prey
 */
typedef struct {
    float tapetum_reflectance;    /**< Light reflection gain: 0.4-1.3 (130% boost) */
    float scotopic_boost;         /**< Rod sensitivity amplification: 1.5-2.0x */
    float rod_cone_ratio;         /**< Rod-to-cone density ratio: 25.0 */
    bool dichromatic_mode;        /**< Disable red cone, enhance blue/green */
    float motion_tracking_gain;   /**< Motion detection for prey: 2.5x */
    float pupil_dynamic_range;    /**< Pupil diameter range: 6-8x */
} cat_vision_config_t;

//=============================================================================
// UV Opsin (SWS1 - Short Wavelength Sensitive Type 1)
//=============================================================================

/**
 * @brief UV-sensitive opsin configuration
 */
typedef struct {
    float peak_wavelength_nm;     /**< Peak sensitivity wavelength (360nm for SWS1) */
    float spectral_width_nm;      /**< Gaussian σ (40nm typical) */
    float quantum_efficiency;     /**< Peak quantum efficiency (0.7 for SWS1) */
    float dark_current;           /**< Dark current noise (pA) */
    float adaptation_rate;        /**< Light adaptation rate (1/s) */
} uv_opsin_config_t;

/**
 * @brief UV opsin state
 */
typedef struct {
    uv_opsin_config_t config;
    float current_sensitivity;    /**< Light-adapted sensitivity */
    float photocurrent;           /**< Current photocurrent (pA) */
    float isomerization_count;    /**< Cumulative photoisomerizations */
} uv_opsin_t;

/**
 * WHAT: Create UV-sensitive opsin photoreceptor
 * WHY:  Enable detection of UV wavelengths (300-400nm)
 * HOW:  Initialize SWS1 opsin with Gaussian spectral response
 *
 * @param config UV opsin configuration
 * @return UV opsin instance or NULL on failure
 *
 * BIOLOGY: SWS1 opsin found in birds (4-color vision), bees, butterflies
 * SPECTRAL RESPONSE: Gaussian centered at 360nm with σ=40nm
 */
uv_opsin_t* uv_opsin_create(const uv_opsin_config_t* config);

/**
 * WHAT: Destroy UV opsin
 */
void uv_opsin_destroy(uv_opsin_t* opsin);

/**
 * WHAT: Compute UV opsin spectral response
 * WHY:  Calculate probability of photon absorption at given wavelength
 * HOW:  Gaussian: R(λ) = Q_eff · exp(-(λ-λ_peak)² / (2σ²))
 *
 * @param opsin UV opsin instance
 * @param wavelength_nm Wavelength in nanometers
 * @return Absorption probability [0, 1]
 *
 * COMPLEXITY: O(1)
 * PHYSICS: Opsin spectral sensitivity follows Gaussian approximation
 */
float uv_opsin_spectral_response(const uv_opsin_t* opsin, float wavelength_nm);

/**
 * WHAT: Process UV photon flux
 * WHY:  Convert UV light intensity to photocurrent
 * HOW:  I_photo = Φ · R(λ) · e · gain
 *
 * @param opsin UV opsin instance
 * @param wavelength_nm Input wavelength (nm)
 * @param photon_flux Photon flux (photons/s/cm²)
 * @param dt Time step (seconds)
 * @return Photocurrent (pA)
 *
 * COMPLEXITY: O(1)
 * BIOLOGY: Each absorbed photon triggers rhodopsin isomerization cascade
 */
float uv_opsin_process_light(uv_opsin_t* opsin, float wavelength_nm,
                              float photon_flux, float dt);

//=============================================================================
// NIR Quantum Dot Layer
//=============================================================================

/**
 * @brief NIR quantum dot configuration
 */
typedef struct {
    float bandgap_ev;             /**< Bandgap energy (eV) - determines cutoff wavelength */
    float quantum_efficiency;     /**< Quantum efficiency [0, 1] */
    float dot_diameter_nm;        /**< Quantum dot diameter (nm) - affects bandgap */
    float carrier_lifetime_ns;    /**< Charge carrier lifetime (ns) */
    float dark_count_rate;        /**< Dark count rate (counts/s) */
} nir_quantum_dot_config_t;

/**
 * @brief NIR quantum dot state
 */
typedef struct {
    nir_quantum_dot_config_t config;
    float carrier_concentration; /**< Photoexcited carrier density (cm⁻³) */
    float photocurrent;          /**< Current photocurrent (pA) */
    uint64_t photon_count;       /**< Cumulative photon detections */
} nir_quantum_dot_t;

/**
 * WHAT: Create NIR quantum dot detector
 * WHY:  Enable NIR detection (700-1100nm) for night vision
 * HOW:  Semiconductor quantum dot with tunable bandgap
 *
 * @param config Quantum dot configuration
 * @return Quantum dot instance or NULL on failure
 *
 * PHYSICS: Bandgap energy E_g determines cutoff wavelength λ_c = hc/E_g
 * MATERIALS: PbS, InGaAs, HgCdTe quantum dots with tunable bandgap
 */
nir_quantum_dot_t* nir_quantum_dot_create(const nir_quantum_dot_config_t* config);

/**
 * WHAT: Destroy NIR quantum dot
 */
void nir_quantum_dot_destroy(nir_quantum_dot_t* qd);

/**
 * WHAT: Calculate cutoff wavelength from bandgap
 * WHY:  Determine maximum detectable wavelength
 * HOW:  λ_c = hc/E_g (Planck relation)
 *
 * @param bandgap_ev Bandgap energy (eV)
 * @return Cutoff wavelength (nm)
 *
 * COMPLEXITY: O(1)
 * PHYSICS: E_photon = hc/λ, photons with E < E_g cannot excite carriers
 */
float nir_quantum_dot_cutoff_wavelength(float bandgap_ev);

/**
 * WHAT: Calculate bandgap from quantum dot size
 * WHY:  Quantum confinement increases bandgap for smaller dots
 * HOW:  E_g = E_g,bulk + (h²/8m*)(π/r)² (effective mass approximation)
 *
 * @param dot_diameter_nm Quantum dot diameter (nm)
 * @param bulk_bandgap_ev Bulk material bandgap (eV)
 * @return Quantum-confined bandgap (eV)
 *
 * COMPLEXITY: O(1)
 * PHYSICS: Quantum confinement shifts absorption to shorter wavelengths
 */
float nir_quantum_dot_bandgap_from_size(float dot_diameter_nm, float bulk_bandgap_ev);

/**
 * WHAT: Process NIR photon absorption
 * WHY:  Convert NIR photons to electrical signal
 * HOW:  If E_photon > E_g, create electron-hole pair with probability η
 *
 * @param qd Quantum dot instance
 * @param wavelength_nm Incident wavelength (nm)
 * @param photon_flux Photon flux (photons/s/cm²)
 * @param dt Time step (seconds)
 * @return Photocurrent (pA)
 *
 * COMPLEXITY: O(1)
 * PHYSICS: Only photons with λ < λ_c = hc/E_g can excite carriers
 */
float nir_quantum_dot_process_light(nir_quantum_dot_t* qd, float wavelength_nm,
                                     float photon_flux, float dt);

//=============================================================================
// Thermal-IR Pit Organ (Snake-like Thermoreceptor)
//=============================================================================

/**
 * @brief Thermal-IR pit organ configuration
 */
typedef struct {
    float membrane_area_mm2;      /**< Thermoreceptor membrane area (mm²) */
    float thermal_time_constant;  /**< Thermal response time (ms) */
    float temperature_resolution; /**< Minimum detectable ΔT (°C) - 0.003°C for rattlesnake */
    float emissivity;             /**< Membrane emissivity [0, 1] - typically 0.95 */
    float ambient_temperature_k;  /**< Ambient temperature (Kelvin) */
} thermal_pit_organ_config_t;

/**
 * @brief Thermal-IR pit organ state
 */
typedef struct {
    thermal_pit_organ_config_t config;
    float membrane_temperature_k; /**< Current membrane temperature (K) */
    float thermal_signal;         /**< Normalized thermal signal [0, 1] */
    float radiant_power_mw;       /**< Incident radiant power (mW) */
} thermal_pit_organ_t;

/**
 * WHAT: Create thermal-IR pit organ
 * WHY:  Enable thermal detection (8-14µm LWIR atmospheric window)
 * HOW:  Thermoreceptor membrane with Stefan-Boltzmann detection
 *
 * @param config Pit organ configuration
 * @return Pit organ instance or NULL on failure
 *
 * BIOLOGY: Found in pythons, boas, vipers (crotaline snakes)
 * PHYSICS: Detects radiant heat via Stefan-Boltzmann law P = σAT⁴
 * SENSITIVITY: Rattlesnakes can detect 0.003°C temperature difference
 */
thermal_pit_organ_t* thermal_pit_organ_create(const thermal_pit_organ_config_t* config);

/**
 * WHAT: Destroy thermal pit organ
 */
void thermal_pit_organ_destroy(thermal_pit_organ_t* pit);

/**
 * WHAT: Calculate blackbody radiance at wavelength
 * WHY:  Determine thermal emission spectrum
 * HOW:  Planck's law: L(λ,T) = (2hc²/λ⁵) · 1/(exp(hc/λk_BT) - 1)
 *
 * @param wavelength_um Wavelength (µm)
 * @param temperature_k Object temperature (K)
 * @return Spectral radiance (W·sr⁻¹·m⁻²·µm⁻¹)
 *
 * COMPLEXITY: O(1)
 * PHYSICS: Planck's blackbody radiation law
 */
float thermal_blackbody_radiance(float wavelength_um, float temperature_k);

/**
 * WHAT: Calculate Stefan-Boltzmann radiant power
 * WHY:  Total thermal radiation from object
 * HOW:  P = ε·σ·A·T⁴ (Stefan-Boltzmann law)
 *
 * @param temperature_k Object temperature (K)
 * @param area_m2 Radiating area (m²)
 * @param emissivity Emissivity [0, 1]
 * @return Radiant power (W)
 *
 * COMPLEXITY: O(1)
 * PHYSICS: Stefan-Boltzmann law - total power ∝ T⁴
 */
float thermal_stefan_boltzmann_power(float temperature_k, float area_m2, float emissivity);

/**
 * WHAT: Process thermal radiation
 * WHY:  Detect warm bodies and temperature gradients
 * HOW:  ΔT_membrane = P_incident / (C_thermal · τ)
 *
 * @param pit Pit organ instance
 * @param target_temperature_k Target object temperature (K)
 * @param target_area_m2 Target radiating area (m²)
 * @param distance_m Distance to target (m)
 * @param dt Time step (seconds)
 * @return Normalized thermal signal [0, 1]
 *
 * COMPLEXITY: O(1)
 * BIOLOGY: Membrane temperature change triggers TRP ion channels
 * PHYSICS: Radiant power falls as 1/r² with distance
 */
float thermal_pit_organ_process(thermal_pit_organ_t* pit, float target_temperature_k,
                                 float target_area_m2, float distance_m, float dt);

//=============================================================================
// Retinal Layer (Multispectral Integration)
//=============================================================================

/**
 * @brief Retinal layer configuration
 */
typedef struct {
    uint32_t width;               /**< Retina width (pixels) */
    uint32_t height;              /**< Retina height (pixels) */
    bool enable_uv;               /**< Enable UV detection */
    bool enable_nir;              /**< Enable NIR detection */
    bool enable_thermal;          /**< Enable thermal-IR detection */
    bool enable_xray;             /**< Enable X-ray placeholder */

    // Vision enhancement mode
    vision_mode_t vision_mode;    /**< Enhanced vision mode (normal/eagle/cat) */

    // Eagle vision configuration (used when vision_mode == VISION_MODE_EAGLE)
    eagle_vision_config_t eagle_config;

    // Cat vision configuration (used when vision_mode == VISION_MODE_CAT)
    cat_vision_config_t cat_config;

    // UV configuration
    uv_opsin_config_t uv_config;

    // NIR configuration
    nir_quantum_dot_config_t nir_config;

    // Thermal configuration
    thermal_pit_organ_config_t thermal_config;
} retina_config_t;

/**
 * @brief Opaque retinal layer structure
 */
typedef struct retina_struct retina_t;

/**
 * @brief Multispectral pixel (10-channel output with rods + bipolar signals)
 */
typedef struct {
    // Photopic (cone) channels
    float uv;        /**< UV channel (300-400nm) - SWS1 opsin */
    float blue;      /**< Blue channel S-cone (400-500nm) */
    float green;     /**< Green channel M-cone (500-600nm) */
    float red;       /**< Red channel L-cone (600-700nm) */

    // Scotopic (rod) channel
    float rod;       /**< Rod channel (scotopic luminosity, 498nm peak) */
    float rod_on_bipolar;  /**< Rod ON bipolar signal (light onset edges) */
    float rod_off_bipolar; /**< Rod OFF bipolar signal (light offset edges) */

    // Extended spectrum channels
    float nir;       /**< NIR channel (700-1100nm) - quantum dots */
    float thermal;   /**< Thermal channel (8-14µm) - pit organ */
    float xray;      /**< X-ray placeholder (future) */
} multispectral_pixel_t;

/**
 * WHAT: Create retinal layer with extended-spectrum photoreceptors
 * WHY:  Enable multispectral vision across UV to thermal-IR
 * HOW:  Initialize photoreceptor arrays for each wavelength band
 *
 * @param config Retina configuration
 * @return Retina instance or NULL on failure
 *
 * ARCHITECTURE:
 * - UV layer: Array of SWS1 opsins
 * - Visible layer: Standard RGB cones
 * - NIR layer: Quantum dot array
 * - Thermal layer: Pit organ thermoreceptors
 * - Fusion layer: Multispectral integration
 *
 * MEMORY: O(width × height × num_channels)
 */
retina_t* retina_create(const retina_config_t* config);

/**
 * WHAT: Destroy retinal layer
 */
void retina_destroy(retina_t* retina);

/**
 * WHAT: Process multispectral image
 * WHY:  Convert photon flux to neural signals across all wavelengths
 * HOW:  Parallel processing through UV, visible, NIR, thermal layers
 *
 * @param retina Retina instance
 * @param wavelength_nm Input wavelength (nm) - 0 for broadband
 * @param photon_flux Photon flux map [height × width] (photons/s/cm²)
 * @param thermal_map Optional thermal map [height × width] (K)
 * @param output Multispectral output [height × width]
 * @param dt Time step (seconds)
 * @return true on success
 *
 * COMPLEXITY: O(width × height × num_channels)
 */
bool retina_process_multispectral(retina_t* retina, float wavelength_nm,
                                   const float* photon_flux, const float* thermal_map,
                                   multispectral_pixel_t* output, float dt);

/**
 * WHAT: Fuse multispectral channels into feature vector
 * WHY:  Create compact representation for downstream processing
 * HOW:  Weighted fusion: F = Σ w_i · normalize(channel_i)
 *
 * @param pixel Multispectral pixel
 * @param weights Channel weights [7] - NULL for equal weighting
 * @param features Output feature vector [7]
 * @return true on success
 *
 * COMPLEXITY: O(num_channels)
 * USE CASES: Feed into visual cortex, attention system, object recognition
 */
bool retina_fuse_spectral_channels(const multispectral_pixel_t* pixel,
                                    const float* weights, float* features);

/**
 * WHAT: Get retinal statistics
 */
typedef struct {
    uint32_t pixels_processed;
    float avg_rod_response;      /**< Average rod (scotopic) response */
    float avg_uv_response;       /**< Average UV response */
    float avg_nir_response;      /**< Average NIR response */
    float avg_thermal_response;  /**< Average thermal response */
    float processing_time_ms;
} retina_stats_t;

bool retina_get_stats(const retina_t* retina, retina_stats_t* stats);

//=============================================================================
// Vision Mode Configuration Helpers
//=============================================================================

/**
 * WHAT: Initialize default eagle vision configuration
 * WHY:  Provide biologically-accurate eagle vision parameters
 * HOW:  Set 4.5x acuity, dual fovea, enhanced motion detection
 *
 * @param config Eagle vision config to initialize
 *
 * PARAMETERS:
 * - Acuity: 4.5x (golden eagle: 20/4 vs human 20/20)
 * - Foveal magnification: 1.4x (deep foveal pit)
 * - Motion sensitivity: 2.5x (specialized ganglion cells)
 * - Depth range: 1.5x (wide stereo baseline)
 */
static inline void eagle_vision_default_config(eagle_vision_config_t* config) {
    config->acuity_multiplier = 4.5f;       // Eagle: 4-5x human acuity
    config->foveal_magnification = 1.4f;    // Deep foveal pit magnification
    config->motion_sensitivity = 2.5f;      // Enhanced motion detection
    config->depth_perception_range = 1.5f;  // Improved stereo vision
    config->dual_fovea_enabled = true;      // Central + temporal foveae
}

/**
 * WHAT: Initialize default cat vision configuration
 * WHY:  Provide biologically-accurate cat vision parameters
 * HOW:  Set tapetum lucidum, rod dominance, dichromatic color
 *
 * @param config Cat vision config to initialize
 *
 * PARAMETERS:
 * - Tapetum reflectance: 1.0 (100% light boost from retroreflection)
 * - Scotopic boost: 1.8x (enhanced rod sensitivity)
 * - Rod/cone ratio: 25:1 (heavily rod-dominated retina)
 * - Dichromatic: Blue + green only (no red cones)
 * - Motion tracking: 2.5x (optimized for hunting)
 * - Pupil range: 7.0x (slit pupil: 0.5-3.5mm diameter)
 */
static inline void cat_vision_default_config(cat_vision_config_t* config) {
    config->tapetum_reflectance = 1.0f;     // 100% light reflection boost
    config->scotopic_boost = 1.8f;          // Rod sensitivity amplification
    config->rod_cone_ratio = 25.0f;         // 25:1 rod dominance
    config->dichromatic_mode = true;        // No red cones (blue+green only)
    config->motion_tracking_gain = 2.5f;    // Enhanced prey motion detection
    config->pupil_dynamic_range = 7.0f;     // Vertical slit pupil control
}

//=============================================================================
// Wavelength Conversion Utilities
//=============================================================================

/**
 * WHAT: Convert wavelength to photon energy
 * WHY:  E = hc/λ (fundamental quantum relation)
 * HOW:  Energy in eV = 1239.84 / wavelength_nm
 *
 * @param wavelength_nm Wavelength (nm)
 * @return Photon energy (eV)
 */
static inline float wavelength_to_energy_ev(float wavelength_nm) {
    return (RETINA_PLANCK_CONSTANT * RETINA_LIGHT_SPEED) /
           (wavelength_nm * 1e-9f * 1.602176634e-19f);
}

/**
 * WHAT: Convert photon energy to wavelength
 * WHY:  λ = hc/E (inverse relation)
 * HOW:  Wavelength in nm = 1239.84 / energy_eV
 *
 * @param energy_ev Photon energy (eV)
 * @return Wavelength (nm)
 */
static inline float energy_to_wavelength_nm(float energy_ev) {
    return (RETINA_PLANCK_CONSTANT * RETINA_LIGHT_SPEED) /
           (energy_ev * 1.602176634e-19f) * 1e9f;
}

/**
 * WHAT: Calculate Wien's displacement wavelength
 * WHY:  Peak blackbody emission wavelength for given temperature
 * HOW:  λ_peak = b/T where b = 2.897771955e-3 m·K
 *
 * @param temperature_k Temperature (K)
 * @return Peak wavelength (µm)
 */
static inline float wien_peak_wavelength_um(float temperature_k) {
    return (RETINA_WIEN_CONSTANT / temperature_k) * 1e6f;  // Convert m to µm
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RETINA_H */
