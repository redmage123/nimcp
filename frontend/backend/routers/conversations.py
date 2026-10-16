"""Conversation CRUD endpoints — per-user isolated conversations."""
from fastapi import APIRouter, Depends, HTTPException

import nimcp_logger
import conversation_store
from auth_deps import get_current_user
from models.conversation import ConvCreate, ConvRename

_log = nimcp_logger.get("routers.conversations")

router = APIRouter(prefix="/api/conversations", tags=["conversations"])


@router.get("/")
async def list_conversations(user: dict = Depends(get_current_user)):
    return conversation_store.list_conversations(user["username"])


@router.post("/")
async def create_conversation(req: ConvCreate, user: dict = Depends(get_current_user)):
    conv_id = conversation_store.create_conversation(
        user["username"], brain_id=req.brain_id, title=req.title,
    )
    return {"id": conv_id, "brain_id": req.brain_id, "title": req.title}


@router.get("/{conv_id}")
async def get_conversation(conv_id: str, user: dict = Depends(get_current_user)):
    conv = conversation_store.get_conversation(user["username"], conv_id)
    if conv is None:
        raise HTTPException(404, "Conversation not found")
    return conv


@router.delete("/{conv_id}")
async def delete_conversation(conv_id: str, user: dict = Depends(get_current_user)):
    if not conversation_store.delete_conversation(user["username"], conv_id):
        raise HTTPException(404, "Conversation not found")
    return {"deleted": conv_id}


@router.patch("/{conv_id}")
async def rename_conversation(conv_id: str, req: ConvRename, user: dict = Depends(get_current_user)):
    if not conversation_store.rename_conversation(user["username"], conv_id, req.title):
        raise HTTPException(404, "Conversation not found")
    return {"id": conv_id, "title": req.title}
