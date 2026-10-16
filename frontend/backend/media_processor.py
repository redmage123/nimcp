"""Multimodal feature composition — combines text, audio, and visual features into 1024-dim vector.

Delegates to MultimodalEncoder when available for the new structured layout:
    [modality_tag:16 | primary_encoder:512 | text_semantic:384 | stats:112]

Falls back to legacy flat layout if MultimodalEncoder is not importable.
"""
import math
import sys
import os

# Allow importing from scripts/
_scripts_dir = os.path.join(os.path.dirname(__file__), "..", "..", "scripts")
if _scripts_dir not in sys.path:
    sys.path.insert(0, os.path.abspath(_scripts_dir))

try:
    from multimodal_encoder import MultimodalEncoder, ModalityType
    _encoder = None

    def _get_encoder():
        global _encoder
        if _encoder is None:
            _encoder = MultimodalEncoder()
        return _encoder

    _MM_AVAILABLE = True
except ImportError:
    _MM_AVAILABLE = False


def compose_multimodal_features(
    text_features: list[float] | None = None,
    audio_features: list[float] | None = None,
    visual_features: list[float] | None = None,
    pil_image=None,
    audio_samples=None,
    audio_sr: int = 44100,
    caption: str | None = None,
) -> list[float]:
    """Compose multimodal features into a 1024-dim vector.

    If MultimodalEncoder is available and raw data (pil_image/audio_samples)
    is provided, uses the new structured layout. Otherwise falls back to
    the legacy flat layout: [text:768 | audio:128 | visual:128].
    """
    # Try structured encoding with raw data
    if _MM_AVAILABLE:
        enc = _get_encoder()
        if pil_image is not None:
            features, _, _ = enc.encode_visual(pil_image, caption)
            return features
        if audio_samples is not None:
            features, _, _ = enc.encode_audio(audio_samples, audio_sr, caption)
            return features

    # Legacy flat layout fallback
    result = [0.0] * 1024

    if text_features:
        for i, v in enumerate(text_features[:768]):
            result[i] = v

    if audio_features:
        for i, v in enumerate(audio_features[:128]):
            result[768 + i] = v

    if visual_features:
        for i, v in enumerate(visual_features[:128]):
            result[896 + i] = v

    norm = math.sqrt(sum(v * v for v in result))
    if norm > 0:
        result = [v / norm for v in result]

    return result
