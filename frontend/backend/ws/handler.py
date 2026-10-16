"""WebSocket handler — single connection per brain_id, multiplexed channels."""
import asyncio
import json
import time

from fastapi import WebSocket, WebSocketDisconnect

import nimcp_logger
from brain_manager import manager
import conversation_store
from config import C_API_TIMEOUT_SECONDS, MAX_WS_MESSAGE_BYTES
from validation import c_api_call
from ws.probe_streamer import subscribe_probe, unsubscribe_probe
import training_runner

_log = nimcp_logger.get("ws.handler")


async def websocket_handler(ws: WebSocket, brain_id: int, username: str = ""):
    await ws.accept()
    _log.info("WebSocket connected brain_id=%d user=%s", brain_id, username or "anonymous")

    probe_queue: asyncio.Queue | None = None
    training_queue: asyncio.Queue | None = None
    sender_task: asyncio.Task | None = None

    async def _sender():
        """Forward messages from subscribed queues to the client."""
        while True:
            queues = []
            if probe_queue is not None:
                queues.append(probe_queue)
            if training_queue is not None:
                queues.append(training_queue)
            if not queues:
                await asyncio.sleep(0.1)
                continue

            done, pending = await asyncio.wait(
                [asyncio.create_task(q.get()) for q in queues],
                return_when=asyncio.FIRST_COMPLETED,
            )
            # Cancel pending tasks to avoid leaks
            for t in pending:
                t.cancel()
            for task in done:
                try:
                    msg = task.result()
                    if isinstance(msg, str):
                        await ws.send_text(msg)
                    elif isinstance(msg, dict):
                        await ws.send_text(json.dumps(msg))
                except Exception as exc:
                    _log.debug("Sender forward error: %s", exc)

    sender_task = asyncio.create_task(_sender())

    try:
        while True:
            raw = await ws.receive_text()

            if len(raw) > MAX_WS_MESSAGE_BYTES:
                await ws.send_text(json.dumps({
                    "type": "error",
                    "message": f"Message too large ({len(raw)} bytes, max {MAX_WS_MESSAGE_BYTES})",
                }))
                continue

            try:
                data = json.loads(raw)
            except json.JSONDecodeError:
                await ws.send_text(json.dumps({"type": "error", "message": "Invalid JSON"}))
                continue

            msg_type = data.get("type", "")

            if msg_type == "subscribe":
                channels = data.get("channels", [])
                if "probe" in channels and probe_queue is None:
                    probe_queue = subscribe_probe(brain_id)
                if "training" in channels and training_queue is None:
                    run = training_runner.get_run(brain_id)
                    if run:
                        training_queue = run.subscribe()

            elif msg_type == "unsubscribe":
                channels = data.get("channels", [])
                if "probe" in channels and probe_queue is not None:
                    unsubscribe_probe(brain_id, probe_queue)
                    probe_queue = None
                if "training" in channels and training_queue is not None:
                    run = training_runner.get_run(brain_id)
                    if run:
                        run.unsubscribe(training_queue)
                    training_queue = None

            elif msg_type == "chat":
                text = data.get("text", "")
                mode = data.get("mode", "chat")
                conv_id = data.get("conversation_id")
                t0 = time.monotonic()

                # Persist user message if conversation_id provided
                if conv_id and username and mode in ("chat", "teach", "introspect"):
                    try:
                        conversation_store.append_message(
                            username, conv_id, "user", text, {"mode": mode})
                    except Exception as exc:
                        _log.debug("Failed to persist user message: %s", exc)

                if mode == "probe":
                    probe = await c_api_call(manager.probe_brain, brain_id, timeout=C_API_TIMEOUT_SECONDS)
                    elapsed = (time.monotonic() - t0) * 1000
                    await ws.send_text(json.dumps({
                        "type": "chat_response",
                        "probe": probe,
                        "message": f"Neurons: {probe['num_neurons']}, Accuracy: {probe['accuracy']:.1%}" if probe else "Probe failed",
                        "time_ms": round(elapsed, 2),
                    }))

                elif mode == "introspect":
                    result = await c_api_call(manager.introspect, brain_id, username,
                                              timeout=C_API_TIMEOUT_SECONDS)
                    elapsed = (time.monotonic() - t0) * 1000
                    response_msg = result["message"] if result else "Introspection failed"
                    await ws.send_text(json.dumps({
                        "type": "chat_response",
                        "message": response_msg,
                        "time_ms": round(elapsed, 2),
                    }))
                    if conv_id and username:
                        try:
                            conversation_store.append_message(
                                username, conv_id, "brain", response_msg)
                        except Exception:
                            pass

                elif mode == "teach":
                    # Parse "label: content" or "content -> label"
                    label = "concept"
                    content = text.strip()
                    if ": " in content and not content.startswith("http"):
                        label, _, content = content.partition(": ")
                        label = label.strip()
                        content = content.strip()
                    elif " -> " in content:
                        content, _, label = content.partition(" -> ")
                        content = content.strip()
                        label = label.strip()

                    result = await c_api_call(manager.teach_conversational,
                                              brain_id, content, label, username,
                                              timeout=C_API_TIMEOUT_SECONDS)
                    elapsed = (time.monotonic() - t0) * 1000
                    response_msg = result["message"] if result else "Teaching failed"
                    await ws.send_text(json.dumps({
                        "type": "chat_response",
                        "message": response_msg,
                        "label": label,
                        "time_ms": round(elapsed, 2),
                    }))
                    if conv_id and username:
                        try:
                            conversation_store.append_message(
                                username, conv_id, "brain", response_msg, {"label": label})
                        except Exception:
                            pass

                elif mode == "learn":
                    # Legacy numeric learn mode
                    from routers.chat import _try_parse_features
                    features = _try_parse_features(text)
                    if features and len(features) >= 2:
                        label = str(int(features[-1]))
                        features = features[:-1]
                        loss = await c_api_call(manager.learn, brain_id, features,
                                                label, 1.0, timeout=C_API_TIMEOUT_SECONDS)
                        elapsed = (time.monotonic() - t0) * 1000
                        await ws.send_text(json.dumps({
                            "type": "chat_response",
                            "label": label,
                            "message": f"Learned label {label} (loss: {loss:.4f})" if loss else f"Learned label {label}",
                            "time_ms": round(elapsed, 2),
                        }))
                    else:
                        await ws.send_text(json.dumps({
                            "type": "chat_response",
                            "message": "Learn mode requires numeric features + label as last number.",
                            "time_ms": 0.0,
                        }))

                else:
                    # Default: conversational chat
                    result = await c_api_call(manager.converse, brain_id, text, username,
                                              timeout=C_API_TIMEOUT_SECONDS)
                    elapsed = (time.monotonic() - t0) * 1000
                    if result:
                        response_msg = result.get("message", "")
                        response_data = {
                            "type": "chat_response",
                            "message": response_msg,
                            "label": result.get("label", ""),
                            "confidence": result.get("confidence", 0.0),
                            "time_ms": round(elapsed, 2),
                            "explanation": result.get("explanation") or None,
                            "sparsity": result.get("sparsity"),
                            "num_active_neurons": result.get("num_active_neurons"),
                            "inference_time_us": result.get("inference_time_us"),
                            "output_vector": result.get("output_vector"),
                            "cognitive_state": result.get("cognitive_state"),
                        }

                        # Speech: generate spoken text from output vector
                        output_vec = result.get("output_vector")
                        if output_vec:
                            spoken = await c_api_call(manager.speak, brain_id,
                                                      output_vec, timeout=C_API_TIMEOUT_SECONDS)
                            if spoken:
                                response_data["spoken_text"] = spoken.get("text", "")
                                response_data["speech_confidence"] = spoken.get("confidence", 0.0)
                                response_data["speech_fluency"] = spoken.get("fluency", 0.0)

                        # Avatar: get visual state (FACS, visemes, gaze, emotion)
                        avatar = await c_api_call(manager.get_avatar_state, brain_id,
                                                   timeout=C_API_TIMEOUT_SECONDS)
                        if avatar:
                            response_data["avatar"] = avatar

                        # Identity: pass through if conversation updated appearance
                        if result.get("identity_updated"):
                            response_data["identity_updated"] = True
                            response_data["identity"] = result.get("identity")

                        await ws.send_text(json.dumps(response_data))
                        if conv_id and username:
                            try:
                                conversation_store.append_message(
                                    username, conv_id, "brain", response_msg,
                                    {"confidence": result.get("confidence", 0.0)})
                            except Exception:
                                pass
                    else:
                        await ws.send_text(json.dumps({
                            "type": "chat_response",
                            "message": "The brain could not process this input.",
                            "time_ms": round(elapsed, 2),
                        }))

    except WebSocketDisconnect:
        _log.info("WebSocket disconnected brain_id=%d", brain_id)
    except Exception as exc:
        _log.error("WebSocket error brain_id=%d: %s", brain_id, exc, exc_info=True)
    finally:
        if sender_task:
            sender_task.cancel()
        if probe_queue is not None:
            unsubscribe_probe(brain_id, probe_queue)
        if training_queue is not None:
            run = training_runner.get_run(brain_id)
            if run:
                run.unsubscribe(training_queue)
