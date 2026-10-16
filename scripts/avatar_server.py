"""
Avatar WebSocket Server — streams brain state to Three.js avatar.

Connects to the brain daemon, collects avatar state + neural metrics at
30fps, and broadcasts to connected WebSocket clients. Also handles chat
messages from the browser and routes them through Phi-3 + brain.

Usage:
    python scripts/avatar_server.py [--port 8765] [--fps 30]

Architecture:
    Brain Daemon (Unix socket)
         ↓ (poll at 30fps)
    Avatar Server (WebSocket :8765)
         ↓ (broadcast)
    Browser (Three.js avatar + chat UI)
"""

import asyncio
import json
import logging
import os
import sys
import time
import argparse

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

logger = logging.getLogger("avatar_server")


class AvatarBroadcaster:
    """Polls brain state and broadcasts to WebSocket clients."""

    def __init__(self, fps=30):
        self.fps = fps
        self.clients = set()
        self.brain = None
        self.identity = None
        self.phi3 = None
        self._running = False

    def _connect_brain(self):
        """Connect to brain daemon."""
        try:
            from brain_client import BrainProxy, is_daemon_running
            if is_daemon_running():
                self.brain = BrainProxy()
                logger.info("Connected to brain daemon")

                try:
                    from athena_identity import IdentityController
                    self.identity = IdentityController(brain=self.brain)
                except Exception as e:
                    logger.warning("Identity controller not available: %s", e)

                try:
                    from phi3_decoder import Phi3Decoder
                    from hybrid_decoder import HybridDecoder
                    self.phi3 = Phi3Decoder()
                    self.hybrid = HybridDecoder(phi3_decoder=self.phi3,
                                                brain=self.brain)
                    if self.phi3.available:
                        logger.info("Phi-3 language cortex available")
                except Exception as e:
                    logger.warning("Phi-3 not available: %s", e)
                    self.phi3 = None
                    self.hybrid = None

                return True
        except Exception as e:
            logger.error("Brain connection failed: %s", e)
        return False

    async def broadcast(self, message):
        """Send message to all connected clients."""
        if not self.clients:
            return
        data = json.dumps(message)
        disconnected = set()
        for client in list(self.clients):  # Snapshot to avoid set-changed-during-iteration
            try:
                await client.send(data)
            except Exception:
                disconnected.add(client)
        self.clients -= disconnected

    async def poll_brain_state(self):
        """Continuously poll brain state and broadcast."""
        interval = 1.0 / self.fps
        self._running = True

        while self._running:
            if not self.brain:
                if not self._connect_brain():
                    await asyncio.sleep(5)
                    continue

            try:
                # Get avatar state
                avatar = self.brain.get_avatar_state()
                if not avatar:
                    avatar = {}

                # Get SNN spikes
                snn_spikes = 0
                try:
                    snn = self.brain.snn_get_stats()
                    if snn:
                        snn_spikes = snn.get('total_spikes', 0)
                except Exception:
                    pass

                # Get identity if available
                identity_data = {}
                if self.identity:
                    try:
                        identity_data = self.identity.update()
                    except Exception:
                        pass

                # Get training metrics
                training = {}
                try:
                    nm = self.brain.get_network_metrics()
                    if nm:
                        training = {
                            'steps': nm.get('ann_steps', 0),
                            'ann_loss': nm.get('ann_loss', 0),
                            'snn_loss': nm.get('snn_loss', 0),
                            'lnn_loss': nm.get('lnn_loss', 0),
                            'cnn_loss': nm.get('cnn_loss', 0),
                        }
                except Exception:
                    pass

                # Get active cognitive modules
                active_modules = []
                try:
                    cs = self.brain.get_cognitive_stats()
                    if cs:
                        active_modules = [k for k, v in cs.items()
                                         if v.get('steps', 0) > 0]
                except Exception:
                    pass

                message = {
                    'type': 'avatar_state',
                    'state': avatar,
                    'snn_spikes': snn_spikes,
                    'training': training,
                    'active_modules': active_modules,
                    'timestamp': time.time(),
                }

                # Include identity data periodically (every 30 frames)
                if hasattr(self, '_frame_count'):
                    self._frame_count += 1
                else:
                    self._frame_count = 0

                if self._frame_count % 30 == 0 and identity_data:
                    voice = identity_data.get('voice', {})
                    await self.broadcast({
                        'type': 'identity',
                        'personality': identity_data.get('personality_summary', ''),
                        'accent': identity_data.get('accent', 'neutral'),
                        'voice_quality': voice.get('voice_quality', 'normal'),
                        'narrative': identity_data.get('narrative', ''),
                        'pitch_hz': voice.get('base_pitch_hz', 210),
                        'speed': voice.get('base_speed', 1.0),
                    })

                await self.broadcast(message)

            except Exception as e:
                logger.error("Brain poll error: %s", e)
                self.brain = None  # Reconnect on next iteration
                await asyncio.sleep(2)
                continue

            await asyncio.sleep(interval)

    async def handle_client(self, websocket):
        """Handle a single WebSocket client connection."""
        self.clients.add(websocket)
        client_addr = websocket.remote_address
        logger.info("Client connected: %s", client_addr)

        try:
            async for message in websocket:
                try:
                    data = json.loads(message)
                    if data.get('type') == 'chat':
                        await self._handle_chat(data.get('text', ''), websocket)
                except json.JSONDecodeError:
                    pass
        except Exception:
            pass
        finally:
            self.clients.discard(websocket)
            logger.info("Client disconnected: %s", client_addr)

    async def _handle_chat(self, text, websocket):
        """Process chat message through brain + Phi-3."""
        if not text:
            return

        response_text = ""

        # Try Phi-3 hybrid decoder
        if self.hybrid and self.phi3 and self.phi3.available:
            try:
                result = self.hybrid.respond(text, brain=self.brain)
                response_text = result.get('text', '')
            except Exception as e:
                logger.error("Chat generation failed: %s", e)

        # Fallback to brain decide_full label
        if not response_text and self.brain:
            try:
                from claude_teacher import encode_text
                import numpy as np
                emb = encode_text(text)
                features = emb[:1024].tolist()
                result = self.brain.decide_full(features)
                response_text = result.get('label', '') or result.get('explanation', '')
            except Exception:
                response_text = "(brain processing failed)"

        if not response_text:
            response_text = "(no response available)"

        # Try TTS audio generation
        audio_data = None
        if response_text and response_text != "(no response available)":
            try:
                from athena_tts import AthenaTTS
                if not hasattr(self, '_tts'):
                    self._tts = AthenaTTS()
                if self._tts.available:
                    result = self._tts.speak(response_text, brain=self.brain)
                    if result and result.get('audio') is not None:
                        import numpy as np
                        import io
                        import wave
                        audio = np.array(result['audio'], dtype=np.float32)
                        audio_int16 = (audio * 32767).astype(np.int16)
                        buf = io.BytesIO()
                        with wave.open(buf, 'wb') as wf:
                            wf.setnchannels(1)
                            wf.setsampwidth(2)
                            wf.setframerate(result.get('sample_rate', 24000))
                            wf.writeframes(audio_int16.tobytes())
                        audio_data = buf.getvalue()
            except Exception as e:
                logger.debug("TTS failed: %s", e)

        await websocket.send(json.dumps({
            'type': 'chat_response',
            'text': response_text,
        }))

        # Send audio as binary frame if available
        if audio_data:
            try:
                await websocket.send(audio_data)
            except Exception as e:
                logger.debug("Audio send failed: %s", e)


async def main(port=8765, fps=30):
    """Start the avatar WebSocket server."""
    try:
        import websockets
    except ImportError:
        print("Installing websockets...")
        os.system(f"{sys.executable} -m pip install websockets")
        import websockets

    broadcaster = AvatarBroadcaster(fps=fps)

    # Start brain polling in background
    poll_task = asyncio.create_task(broadcaster.poll_brain_state())

    # Start WebSocket server
    logger.info("Avatar server starting on ws://0.0.0.0:%d (fps=%d)", port, fps)
    async with websockets.serve(broadcaster.handle_client, "0.0.0.0", port):
        logger.info("Avatar server running. Open avatar/index.html in browser.")
        await asyncio.Future()  # Run forever


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Athena Avatar WebSocket Server")
    parser.add_argument("--port", type=int, default=8765, help="WebSocket port")
    parser.add_argument("--fps", type=int, default=30, help="State broadcast FPS")
    args = parser.parse_args()

    logging.basicConfig(level=logging.INFO,
                        format="%(asctime)s [%(name)s] %(message)s")

    asyncio.run(main(port=args.port, fps=args.fps))
