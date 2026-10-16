"""
Sensory Encoder — synthesizes somatosensory data from text descriptions.

Parses natural language descriptions of physical sensations and encodes
them as 45-dimensional somatosensory vectors that feed the somato cortex CNN.

Architecture:
  Parent text: "The towel is thick and fluffy, so much softer than cool air"
       ↓
  Phi-3 extracts sensory properties (or keyword fallback)
       ↓
  45-dim vector: [temperature | texture | pressure | wetness | location |
                  material | motion | valence | temporal]
       ↓
  brain.submit_sensory("somatosensory", vector)
       ↓
  Somatosensory Cortex CNN learns to associate text ↔ touch

This creates genuine cross-modal binding: when Athena hears "soft warm towel,"
she simultaneously "feels" it — temporal co-occurrence drives STDP wiring.
"""

import logging
import re
import numpy as np

logger = logging.getLogger(__name__)

# ============================================================
# Sensory property keyword dictionaries
# ============================================================

TEMPERATURE_KEYWORDS = {
    # value: (center_position_in_0_to_1_scale)
    'frozen': 0.0, 'freezing': 0.05, 'icy': 0.08, 'ice': 0.05,
    'cold': 0.15, 'chilly': 0.18, 'cool': 0.25, 'brisk': 0.22,
    'lukewarm': 0.45, 'tepid': 0.42,
    'warm': 0.65, 'cozy': 0.68, 'toasty': 0.72, 'heated': 0.7,
    'hot': 0.85, 'burning': 0.95, 'scalding': 0.98, 'fiery': 0.92,
    'sun': 0.7, 'sunlight': 0.65, 'sunshine': 0.68,
    'winter': 0.15, 'summer': 0.7, 'snow': 0.05, 'fire': 0.9,
}

TEXTURE_KEYWORDS = {
    'smooth': 0.05, 'silky': 0.08, 'glass': 0.03, 'satin': 0.1,
    'polished': 0.05, 'slick': 0.07, 'glossy': 0.06,
    'soft': 0.2, 'fluffy': 0.18, 'fuzzy': 0.22, 'velvety': 0.15,
    'velvet': 0.15, 'plush': 0.17, 'downy': 0.16, 'feathery': 0.14,
    'cottony': 0.19, 'cushiony': 0.2, 'spongy': 0.25,
    'rough': 0.6, 'coarse': 0.65, 'gritty': 0.7, 'grainy': 0.68,
    'sandpaper': 0.75, 'scratchy': 0.72, 'bristly': 0.7,
    'textured': 0.5, 'bumpy': 0.55, 'ridged': 0.58,
    'sharp': 0.85, 'prickly': 0.88, 'spiky': 0.9, 'thorny': 0.92,
    'pointed': 0.82, 'jagged': 0.87,
    'woolly': 0.45, 'wool': 0.45, 'woolen': 0.45, 'knitted': 0.42,
    'crinkly': 0.55, 'papery': 0.5, 'crisp': 0.48,
}

PRESSURE_KEYWORDS = {
    'feather': 0.05, 'feathery': 0.05, 'whisper': 0.08, 'barely': 0.1,
    'light': 0.2, 'gentle': 0.22, 'delicate': 0.15, 'tender': 0.18,
    'soft': 0.25, 'caress': 0.2,
    'medium': 0.5, 'moderate': 0.5, 'steady': 0.5,
    'firm': 0.7, 'solid': 0.72, 'strong': 0.75, 'pressed': 0.68,
    'heavy': 0.85, 'weighty': 0.88, 'crushing': 0.95,
    'squeeze': 0.7, 'hug': 0.55, 'bundled': 0.5, 'wrapped': 0.45,
    'tap': 0.3, 'tapping': 0.3, 'pat': 0.35,
}

WETNESS_KEYWORDS = {
    'dry': 0.0, 'arid': 0.02, 'parched': 0.01,
    'damp': 0.3, 'moist': 0.35, 'dewy': 0.25, 'humid': 0.3,
    'wet': 0.6, 'splash': 0.65, 'dripping': 0.7, 'rain': 0.6,
    'raindrop': 0.5, 'raindrops': 0.5,
    'soaked': 0.8, 'drenched': 0.85, 'saturated': 0.82,
    'submerged': 0.95, 'underwater': 0.98, 'bath': 0.7,
    'melting': 0.5, 'puddle': 0.6, 'stream': 0.7, 'ocean': 0.9,
    'water': 0.6, 'bubble': 0.5, 'bubbles': 0.5,
}

LOCATION_KEYWORDS = {
    # Maps to one-hot-ish over 5 body regions: face, hands, feet, torso, head
    'face': [1, 0, 0, 0, 0], 'cheek': [1, 0, 0, 0, 0], 'forehead': [1, 0, 0, 0, 0.3],
    'nose': [1, 0, 0, 0, 0], 'lip': [1, 0, 0, 0, 0], 'chin': [1, 0, 0, 0, 0],
    'eye': [1, 0, 0, 0, 0.2], 'mouth': [1, 0, 0, 0, 0],
    'hand': [0, 1, 0, 0, 0], 'hands': [0, 1, 0, 0, 0], 'finger': [0, 1, 0, 0, 0],
    'fingers': [0, 1, 0, 0, 0], 'fingertip': [0, 1, 0, 0, 0], 'palm': [0, 1, 0, 0, 0],
    'thumb': [0, 1, 0, 0, 0], 'fingernail': [0, 1, 0, 0, 0],
    'toe': [0, 0, 1, 0, 0], 'toes': [0, 0, 1, 0, 0], 'foot': [0, 0, 1, 0, 0],
    'feet': [0, 0, 1, 0, 0], 'sole': [0, 0, 1, 0, 0],
    'body': [0, 0, 0, 1, 0], 'skin': [0.2, 0.2, 0.2, 0.4, 0],
    'chest': [0, 0, 0, 1, 0], 'belly': [0, 0, 0, 1, 0], 'back': [0, 0, 0, 1, 0],
    'arm': [0, 0.5, 0, 0.5, 0], 'shoulder': [0, 0, 0, 0.8, 0.2],
    'head': [0.3, 0, 0, 0, 1], 'ear': [0.3, 0, 0, 0, 0.7],
    'hair': [0, 0, 0, 0, 1], 'neck': [0.2, 0, 0, 0.5, 0.3],
}

MATERIAL_KEYWORDS = {
    # Maps to one-hot over: skin, fabric, metal, wood, organic, water, air
    'skin': [1, 0, 0, 0, 0, 0, 0], 'beard': [1, 0, 0, 0, 0, 0, 0],
    'fabric': [0, 1, 0, 0, 0, 0, 0], 'cloth': [0, 1, 0, 0, 0, 0, 0],
    'towel': [0, 1, 0, 0, 0, 0, 0], 'blanket': [0, 1, 0, 0, 0, 0, 0],
    'sweater': [0, 1, 0, 0, 0, 0, 0], 'silk': [0, 1, 0, 0, 0, 0, 0],
    'ribbon': [0, 1, 0, 0, 0, 0, 0], 'wool': [0, 1, 0, 0, 0, 0, 0],
    'cotton': [0, 1, 0, 0, 0, 0, 0], 'linen': [0, 1, 0, 0, 0, 0, 0],
    'velvet': [0, 1, 0, 0, 0, 0, 0], 'satin': [0, 1, 0, 0, 0, 0, 0],
    'metal': [0, 0, 1, 0, 0, 0, 0], 'steel': [0, 0, 1, 0, 0, 0, 0],
    'silver': [0, 0, 1, 0, 0, 0, 0], 'gold': [0, 0, 1, 0, 0, 0, 0],
    'iron': [0, 0, 1, 0, 0, 0, 0], 'key': [0, 0, 1, 0, 0, 0, 0],
    'bracelet': [0, 0, 1, 0, 0, 0, 0],
    'wood': [0, 0, 0, 1, 0, 0, 0], 'wooden': [0, 0, 0, 1, 0, 0, 0],
    'bark': [0, 0, 0, 1, 0, 0, 0], 'stick': [0, 0, 0, 1, 0, 0, 0],
    'floor': [0, 0, 0, 0.7, 0, 0, 0],
    'grass': [0, 0, 0, 0, 1, 0, 0], 'leaf': [0, 0, 0, 0, 1, 0, 0],
    'flower': [0, 0, 0, 0, 1, 0, 0], 'petal': [0, 0, 0, 0, 1, 0, 0],
    'fur': [0, 0, 0, 0, 1, 0, 0], 'feather': [0, 0, 0, 0, 1, 0, 0],
    'fruit': [0, 0, 0, 0, 1, 0, 0], 'peach': [0, 0, 0, 0, 1, 0, 0],
    'water': [0, 0, 0, 0, 0, 1, 0], 'ice': [0, 0, 0, 0, 0, 1, 0],
    'rain': [0, 0, 0, 0, 0, 1, 0], 'snow': [0, 0, 0, 0, 0, 1, 0],
    'wave': [0, 0, 0, 0, 0, 1, 0], 'ocean': [0, 0, 0, 0, 0, 1, 0],
    'air': [0, 0, 0, 0, 0, 0, 1], 'wind': [0, 0, 0, 0, 0, 0, 1],
    'breeze': [0, 0, 0, 0, 0, 0, 1], 'breath': [0, 0, 0, 0, 0, 0, 1],
    'glass': [0, 0, 0.5, 0, 0, 0, 0], 'stone': [0, 0, 0, 0.3, 0.3, 0, 0],
    'sand': [0, 0, 0, 0, 0.5, 0, 0], 'mud': [0, 0, 0, 0, 0.5, 0.5, 0],
    'clay': [0, 0, 0, 0, 0.7, 0.3, 0],
}

MOTION_KEYWORDS = {
    # Maps to one-hot over: still, sliding, tapping, rubbing, pressing
    'still': [1, 0, 0, 0, 0], 'resting': [1, 0, 0, 0, 0], 'lying': [1, 0, 0, 0, 0],
    'slide': [0, 1, 0, 0, 0], 'slides': [0, 1, 0, 0, 0], 'sliding': [0, 1, 0, 0, 0],
    'glide': [0, 1, 0, 0, 0], 'drag': [0, 0.8, 0, 0.2, 0],
    'stroke': [0, 0.5, 0, 0.5, 0], 'caress': [0, 0.5, 0, 0.5, 0],
    'tap': [0, 0, 1, 0, 0], 'tapping': [0, 0, 1, 0, 0], 'taps': [0, 0, 1, 0, 0],
    'pat': [0, 0, 1, 0, 0], 'poke': [0, 0, 1, 0, 0],
    'rub': [0, 0, 0, 1, 0], 'rubbing': [0, 0, 0, 1, 0], 'scratch': [0, 0, 0, 1, 0],
    'scratches': [0, 0, 0, 1, 0],
    'press': [0, 0, 0, 0, 1], 'pressing': [0, 0, 0, 0, 1], 'push': [0, 0, 0, 0, 1],
    'squeeze': [0, 0, 0, 0, 1], 'grip': [0, 0, 0, 0, 1],
    'wrap': [0, 0, 0, 0, 0.7], 'wrapping': [0, 0, 0, 0, 0.7],
    'roll': [0, 0.7, 0, 0.3, 0], 'rolling': [0, 0.7, 0, 0.3, 0],
    'drift': [0, 0.8, 0, 0, 0], 'float': [0, 0.5, 0, 0, 0],
}

VALENCE_KEYWORDS = {
    # Maps to one-hot over: unpleasant, neutral, pleasant, comforting, delightful
    'pain': [1, 0, 0, 0, 0], 'hurt': [1, 0, 0, 0, 0], 'sting': [0.8, 0.2, 0, 0, 0],
    'scratch': [0.5, 0.3, 0, 0, 0], 'prickly': [0.6, 0.2, 0, 0, 0],
    'ordinary': [0, 1, 0, 0, 0], 'normal': [0, 1, 0, 0, 0],
    'pleasant': [0, 0, 1, 0, 0], 'nice': [0, 0, 1, 0, 0], 'good': [0, 0, 0.7, 0.3, 0],
    'soft': [0, 0, 0.5, 0.5, 0], 'gentle': [0, 0, 0.3, 0.7, 0],
    'cozy': [0, 0, 0, 1, 0], 'comfort': [0, 0, 0, 1, 0], 'comforting': [0, 0, 0, 1, 0],
    'warm': [0, 0, 0.3, 0.7, 0], 'snuggle': [0, 0, 0, 0.8, 0.2],
    'wonderful': [0, 0, 0, 0.3, 0.7], 'delightful': [0, 0, 0, 0, 1],
    'amazing': [0, 0, 0, 0.2, 0.8], 'magical': [0, 0, 0, 0.1, 0.9],
    'beautiful': [0, 0, 0, 0.3, 0.7], 'lovely': [0, 0, 0, 0.4, 0.6],
    'fascinating': [0, 0, 0, 0.2, 0.8],
    'cool': [0, 0, 0.6, 0.4, 0], 'refreshing': [0, 0, 0.3, 0.5, 0.2],
    'tickly': [0, 0, 0.5, 0.3, 0.2], 'tingly': [0, 0, 0.4, 0.3, 0.3],
}

TEMPORAL_KEYWORDS = {
    # Maps to one-hot over: instant, brief, sustained, pulsing, fading
    'instant': [1, 0, 0, 0, 0], 'sudden': [1, 0, 0, 0, 0], 'flash': [1, 0, 0, 0, 0],
    'quick': [0.5, 0.5, 0, 0, 0], 'snap': [1, 0, 0, 0, 0],
    'brief': [0, 1, 0, 0, 0], 'moment': [0, 1, 0, 0, 0], 'fleeting': [0, 1, 0, 0, 0],
    'sustained': [0, 0, 1, 0, 0], 'constant': [0, 0, 1, 0, 0],
    'steady': [0, 0, 1, 0, 0], 'continuous': [0, 0, 1, 0, 0],
    'always': [0, 0, 1, 0, 0], 'forever': [0, 0, 1, 0, 0],
    'pulse': [0, 0, 0, 1, 0], 'pulsing': [0, 0, 0, 1, 0], 'throb': [0, 0, 0, 1, 0],
    'rhythm': [0, 0, 0, 1, 0], 'heartbeat': [0, 0, 0, 1, 0],
    'fading': [0, 0, 0, 0, 1], 'melting': [0, 0, 0, 0, 1],
    'disappearing': [0, 0, 0, 0, 1], 'dissolving': [0, 0, 0, 0, 1],
}


def encode_sensory(text):
    """Encode a text description into a 45-dim somatosensory vector.

    Scans for sensory keywords and builds a vector encoding:
      [0:5]   temperature
      [5:10]  texture
      [10:15] pressure
      [15:20] wetness
      [20:25] body location
      [25:32] material type
      [30:35] motion type  (overlaps material intentionally — use 25:30 material, 30:35 motion)
    Actual layout:
      [0:5]   temperature (gaussian around detected value)
      [5:10]  texture (gaussian)
      [10:15] pressure (gaussian)
      [15:20] wetness (gaussian)
      [20:25] body location (one-hot-ish)
      [25:30] material (from 7-dim, take top 5)
      [30:35] motion (one-hot-ish)
      [35:40] emotional valence (one-hot-ish)
      [40:45] temporal (one-hot-ish)

    Args:
        text: Natural language description of a physical sensation.

    Returns:
        45-dim np.float32 array, or None if no sensory content detected.
    """
    text_lower = text.lower()
    words = set(re.findall(r'[a-z]+', text_lower))

    vec = np.zeros(45, dtype=np.float32)
    has_sensory = False

    # Temperature [0:5] — gaussian around detected value
    temp_val = _extract_scalar(words, TEMPERATURE_KEYWORDS)
    if temp_val is not None:
        vec[0:5] = _gaussian_encode(temp_val, 5)
        has_sensory = True

    # Texture [5:10]
    tex_val = _extract_scalar(words, TEXTURE_KEYWORDS)
    if tex_val is not None:
        vec[5:10] = _gaussian_encode(tex_val, 5)
        has_sensory = True

    # Pressure [10:15]
    pres_val = _extract_scalar(words, PRESSURE_KEYWORDS)
    if pres_val is not None:
        vec[10:15] = _gaussian_encode(pres_val, 5)
        has_sensory = True

    # Wetness [15:20]
    wet_val = _extract_scalar(words, WETNESS_KEYWORDS)
    if wet_val is not None:
        vec[15:20] = _gaussian_encode(wet_val, 5)
        has_sensory = True

    # Body location [20:25]
    loc = _extract_vector(words, LOCATION_KEYWORDS, 5)
    if loc is not None:
        vec[20:25] = loc
        has_sensory = True

    # Material [25:30] — take first 5 of 7-dim
    mat = _extract_vector(words, MATERIAL_KEYWORDS, 7)
    if mat is not None:
        vec[25:30] = mat[:5]
        has_sensory = True

    # Motion [30:35]
    mot = _extract_vector(words, MOTION_KEYWORDS, 5)
    if mot is not None:
        vec[30:35] = mot
        has_sensory = True

    # Emotional valence [35:40]
    val = _extract_vector(words, VALENCE_KEYWORDS, 5)
    if val is not None:
        vec[35:40] = val
        has_sensory = True

    # Temporal [40:45]
    tmp = _extract_vector(words, TEMPORAL_KEYWORDS, 5)
    if tmp is not None:
        vec[40:45] = tmp
        has_sensory = True
    else:
        # Default to sustained if sensory content but no temporal keyword
        if has_sensory:
            vec[42] = 0.5  # sustained

    if not has_sensory:
        return None

    # Normalize to [0, 1]
    max_val = vec.max()
    if max_val > 1.0:
        vec /= max_val

    return vec


def _extract_scalar(words, keyword_dict):
    """Extract a scalar value from keyword matches. Returns average if multiple."""
    values = []
    for word in words:
        if word in keyword_dict:
            values.append(keyword_dict[word])
    if not values:
        return None
    return np.mean(values)


def _extract_vector(words, keyword_dict, dim):
    """Extract a vector from keyword matches. Accumulates and normalizes."""
    result = np.zeros(dim, dtype=np.float32)
    found = False
    for word in words:
        if word in keyword_dict:
            v = keyword_dict[word]
            if isinstance(v, list):
                for i in range(min(len(v), dim)):
                    result[i] += v[i]
            found = True
    if not found:
        return None
    # Normalize
    max_val = result.max()
    if max_val > 0:
        result /= max_val
    return result


def _gaussian_encode(value, dim):
    """Encode a scalar [0,1] as a gaussian bump across dim bins."""
    centers = np.linspace(0, 1, dim)
    sigma = 1.0 / dim
    return np.exp(-0.5 * ((centers - value) / sigma) ** 2).astype(np.float32)


def submit_sensory_from_text(brain, text):
    """Parse text and submit somatosensory data to brain if sensory content found.

    Args:
        brain: Brain instance or BrainProxy.
        text: Parent narration text.

    Returns:
        True if sensory data was submitted, False otherwise.
    """
    vec = encode_sensory(text)
    if vec is None:
        return False

    try:
        segments = vec.tolist()
        brain.submit_sensory("somatosensory", segments, n_segments=len(segments))
        logger.debug("Submitted somatosensory data (%d non-zero dims) for: %s",
                     int(np.count_nonzero(vec)), text[:60])
        return True
    except Exception as e:
        logger.debug("Failed to submit somatosensory: %s", e)
        return False
