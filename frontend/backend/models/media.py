"""Pydantic models for media processing endpoints."""
from pydantic import BaseModel, field_validator


class AudioProcessRequest(BaseModel):
    brain_id: int = 0
    samples: list[float]

    @field_validator("samples")
    @classmethod
    def validate_samples(cls, v: list[float]) -> list[float]:
        if len(v) > 441000:
            raise ValueError("Too many audio samples (max 441000 = 10s at 44.1kHz)")
        return v


class VideoProcessRequest(BaseModel):
    brain_id: int = 0
    pixels: list[float]
    width: int = 160
    height: int = 120
    channels: int = 3

    @field_validator("pixels")
    @classmethod
    def validate_pixels(cls, v: list[float]) -> list[float]:
        if len(v) > 921600:
            raise ValueError("Too many pixels (max 921600 = 640x480x3)")
        return v


class ComposeRequest(BaseModel):
    text_features: list[float] = []
    audio_features: list[float] = []
    visual_features: list[float] = []
