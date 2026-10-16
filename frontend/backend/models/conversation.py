"""Pydantic models for conversation endpoints."""
from pydantic import BaseModel, field_validator


class ConvCreate(BaseModel):
    brain_id: int = 0
    title: str = "New conversation"

    @field_validator("title")
    @classmethod
    def validate_title(cls, v: str) -> str:
        v = v.strip()
        if not v:
            return "New conversation"
        if len(v) > 200:
            v = v[:200]
        return v


class ConvRename(BaseModel):
    title: str

    @field_validator("title")
    @classmethod
    def validate_title(cls, v: str) -> str:
        v = v.strip()
        if not v:
            raise ValueError("Title cannot be empty")
        if len(v) > 200:
            v = v[:200]
        return v
