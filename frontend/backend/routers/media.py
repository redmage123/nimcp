"""Media processing endpoints — audio/visual cortex processing and multimodal composition."""
from fastapi import APIRouter, Depends, HTTPException

import nimcp_logger
from auth_deps import get_current_user
from brain_manager import manager
from media_processor import compose_multimodal_features
from models.media import AudioProcessRequest, VideoProcessRequest, ComposeRequest
from validation import c_api_call
from config import C_API_TIMEOUT_SECONDS

_log = nimcp_logger.get("routers.media")

router = APIRouter(prefix="/api/media", tags=["media"])


@router.post("/process-audio")
async def process_audio(req: AudioProcessRequest, user: dict = Depends(get_current_user)):
    """Process audio samples through the brain's audio cortex."""
    brain = manager.get_brain(req.brain_id)
    if brain is None:
        raise HTTPException(404, "Brain not found")
    try:
        features = await c_api_call(brain.audio_cortex_process, req.samples,
                                    timeout=C_API_TIMEOUT_SECONDS)
        if features is None:
            # Fallback: simple feature extraction if cortex not available
            features = _fallback_audio_features(req.samples)
        return {"features": list(features)[:128]}
    except Exception as exc:
        _log.debug("Audio cortex processing failed, using fallback: %s", exc)
        features = _fallback_audio_features(req.samples)
        return {"features": features}


@router.post("/process-video")
async def process_video(req: VideoProcessRequest, user: dict = Depends(get_current_user)):
    """Process video frame through the brain's visual cortex."""
    brain = manager.get_brain(req.brain_id)
    if brain is None:
        raise HTTPException(404, "Brain not found")
    try:
        features = await c_api_call(
            brain.visual_cortex_process, req.pixels, req.width, req.height, req.channels,
            timeout=C_API_TIMEOUT_SECONDS)
        if features is None:
            features = _fallback_visual_features(req.pixels, req.width, req.height)
        return {"features": list(features)[:128]}
    except Exception as exc:
        _log.debug("Visual cortex processing failed, using fallback: %s", exc)
        features = _fallback_visual_features(req.pixels, req.width, req.height)
        return {"features": features}


@router.post("/compose")
async def compose_features(req: ComposeRequest, user: dict = Depends(get_current_user)):
    """Compose text, audio, and visual features into a 1024-dim vector."""
    result = compose_multimodal_features(
        text_features=req.text_features or None,
        audio_features=req.audio_features or None,
        visual_features=req.visual_features or None,
    )
    return {"features": result}


def _fallback_audio_features(samples: list[float]) -> list[float]:
    """Simple fallback audio feature extraction when cortex API is unavailable."""
    import math
    features = [0.0] * 128
    if not samples:
        return features
    n = len(samples)
    # Energy
    energy = sum(s * s for s in samples) / n
    features[0] = min(energy, 1.0)
    # Zero crossing rate
    zc = sum(1 for i in range(1, n) if (samples[i] >= 0) != (samples[i - 1] >= 0)) / n
    features[1] = zc
    # RMS
    features[2] = math.sqrt(energy)
    # Spread samples across remaining features
    step = max(1, n // 125)
    for i in range(3, 128):
        idx = (i - 3) * step
        if idx < n:
            features[i] = max(-1.0, min(1.0, samples[idx]))
    return features


def _fallback_visual_features(pixels: list[float], width: int, height: int) -> list[float]:
    """Simple fallback visual feature extraction when cortex API is unavailable."""
    features = [0.0] * 128
    if not pixels:
        return features
    n = len(pixels)
    # Mean brightness
    features[0] = sum(pixels) / n if n > 0 else 0.0
    # Spread spatial samples
    step = max(1, n // 127)
    for i in range(1, 128):
        idx = i * step
        if idx < n:
            features[i] = max(0.0, min(1.0, pixels[idx]))
    return features
