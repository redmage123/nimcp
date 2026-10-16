"""Chat endpoint — conversational AGI interface to the brain."""
import time
from fastapi import APIRouter, HTTPException

import nimcp_logger
from brain_manager import manager
from config import C_API_TIMEOUT_SECONDS
from models.chat import ChatMessage, ChatResponse
from validation import c_api_call

_log = nimcp_logger.get("routers.chat")

router = APIRouter(prefix="/api/chat", tags=["chat"])


def _try_parse_features(text: str) -> list[float] | None:
    """Try to parse comma-separated numbers. Return None if not numeric."""
    parts = text.replace(";", ",").split(",")
    nums = []
    for p in parts:
        p = p.strip()
        if not p:
            continue
        try:
            nums.append(float(p))
        except ValueError:
            return None
    return nums if nums else None


@router.post("/{brain_id}")
async def chat(brain_id: int, msg: ChatMessage) -> ChatResponse:
    if not manager.has_brain(brain_id):
        raise HTTPException(404, "Brain not found")

    t0 = time.monotonic()

    if msg.mode == "probe" or msg.mode == "introspect":
        if msg.mode == "introspect":
            result = await c_api_call(manager.introspect, brain_id,
                                      timeout=C_API_TIMEOUT_SECONDS)
            elapsed = (time.monotonic() - t0) * 1000
            if result is None:
                raise HTTPException(500, "Introspection failed")
            return ChatResponse(message=result["message"], time_ms=round(elapsed, 2))

        probe = await c_api_call(manager.probe_brain, brain_id,
                                 timeout=C_API_TIMEOUT_SECONDS)
        elapsed = (time.monotonic() - t0) * 1000
        if probe is None:
            raise HTTPException(500, "Probe failed")
        summary = (
            f"Neurons: {probe['num_neurons']}, "
            f"Accuracy: {probe['accuracy']:.1%}, LR: {probe['current_learning_rate']:.6f}, "
            f"Steps: {probe['total_learning_steps']}, Inferences: {probe['total_inferences']}"
        )
        return ChatResponse(probe=probe, message=summary, time_ms=round(elapsed, 2))

    if msg.mode == "teach":
        text = msg.text.strip()
        # Parse "label: content" or "content -> label" format
        label = "concept"
        content = text
        if ": " in text and not text.startswith("http"):
            label, _, content = text.partition(": ")
            label = label.strip()
            content = content.strip()
        elif " -> " in text:
            content, _, label = text.partition(" -> ")
            content = content.strip()
            label = label.strip()

        result = await c_api_call(manager.teach_conversational, brain_id,
                                  content, label, timeout=C_API_TIMEOUT_SECONDS)
        elapsed = (time.monotonic() - t0) * 1000
        if result is None:
            raise HTTPException(500, "Teaching failed")
        return ChatResponse(
            message=result["message"],
            time_ms=round(elapsed, 2),
            label=label,
        )

    if msg.mode == "learn":
        # Legacy numeric learn mode
        features = _try_parse_features(msg.text)
        if features is None or len(features) < 2:
            return ChatResponse(
                message="Learn mode requires numeric input: features + label as last number.",
                time_ms=0.0,
            )
        label = str(int(features[-1]))
        features = features[:-1]
        loss = await c_api_call(manager.learn, brain_id, features, label, 1.0,
                                timeout=C_API_TIMEOUT_SECONDS)
        elapsed = (time.monotonic() - t0) * 1000
        return ChatResponse(label=label, message=f"Learned label {label} (loss: {loss:.4f})",
                            time_ms=round(elapsed, 2))

    # Chat mode (default) — conversational interface
    result = await c_api_call(manager.converse, brain_id, msg.text,
                              timeout=C_API_TIMEOUT_SECONDS)
    elapsed = (time.monotonic() - t0) * 1000
    if result is None:
        raise HTTPException(500, "Conversation failed")

    resp = ChatResponse(
        message=result.get("message", ""),
        label=result.get("label"),
        confidence=result.get("confidence"),
        time_ms=round(elapsed, 2),
        explanation=result.get("explanation") or None,
        sparsity=result.get("sparsity"),
        num_active_neurons=result.get("num_active_neurons"),
        inference_time_us=result.get("inference_time_us"),
        output_vector=result.get("output_vector"),
        cognitive_state=result.get("cognitive_state"),
    )

    # Speech: generate spoken text from output vector
    output_vec = result.get("output_vector")
    if output_vec:
        spoken = await c_api_call(manager.speak, brain_id, output_vec,
                                   timeout=C_API_TIMEOUT_SECONDS)
        if spoken:
            resp.spoken_text = spoken.get("text", "")
            resp.speech_confidence = spoken.get("confidence", 0.0)
            resp.speech_fluency = spoken.get("fluency", 0.0)

    # Avatar: get visual state (FACS, visemes, gaze, emotion)
    avatar = await c_api_call(manager.get_avatar_state, brain_id,
                               timeout=C_API_TIMEOUT_SECONDS)
    if avatar:
        resp.avatar = avatar

    return resp
