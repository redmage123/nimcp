"""Authentication endpoints — login, registration, and user info."""
from fastapi import APIRouter, Depends, HTTPException, Request
from pydantic import BaseModel

import nimcp_logger
import user_store
from auth_deps import get_current_user

_log = nimcp_logger.get("routers.auth")

router = APIRouter(prefix="/api/auth", tags=["auth"])


class AuthRequest(BaseModel):
    username: str
    password: str


@router.post("/login")
async def login(req: AuthRequest):
    if not user_store.verify(req.username, req.password):
        raise HTTPException(401, "Invalid username or password")
    role = user_store.get_role(req.username)
    return {"ok": True, "username": req.username, "role": role}


@router.post("/register")
async def register(req: AuthRequest):
    try:
        user_store.register(req.username, req.password)
        return {"ok": True, "username": req.username, "role": "user"}
    except ValueError as exc:
        raise HTTPException(400, str(exc))


@router.get("/me")
async def me(user: dict = Depends(get_current_user)):
    return user
