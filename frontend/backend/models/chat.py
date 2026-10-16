"""Pydantic models for chat endpoints."""
from pydantic import BaseModel, Field
from typing import Literal, Optional


class ChatMessage(BaseModel):
    text: str = Field(max_length=10000)
    mode: Literal["chat", "teach", "learn", "introspect", "probe", "decide"] = "chat"


class ChatResponse(BaseModel):
    label: Optional[str] = None
    confidence: Optional[float] = None
    probe: Optional[dict] = None
    message: str = ""
    time_ms: float = 0.0
    explanation: Optional[str] = None
    sparsity: Optional[float] = None
    num_active_neurons: Optional[int] = None
    inference_time_us: Optional[int] = None
    output_vector: Optional[list[float]] = None
    cognitive_state: Optional[dict] = None
    # Speech & Avatar (new)
    spoken_text: Optional[str] = None
    speech_confidence: Optional[float] = None
    speech_fluency: Optional[float] = None
    avatar: Optional[dict] = None
