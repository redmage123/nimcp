"""
Athena Meeting Bridge — virtual camera + microphone for Zoom/Teams.

Captures meeting audio via PulseAudio, processes through brain + Phi-3,
generates voice response via Coqui TTS, and routes to virtual microphone.
OBS captures the avatar browser window as virtual camera.

Usage:
    # 1. Start avatar server (if not running)
    python3 scripts/avatar_server.py &

    # 2. Open avatar in browser
    firefox avatar/index.html &

    # 3. Start meeting bridge
    python3 scripts/athena_meeting.py

    # 4. In Zoom/Teams:
    #    Video: "OBS Virtual Camera"
    #    Microphone: "Athena Virtual Mic"

Prerequisites:
    sudo apt install obs-studio pulseaudio-utils ffmpeg
    # OBS: Tools > Start Virtual Camera
"""

import logging
import os
import subprocess
import sys
import time
import signal
import threading
import json

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

logger = logging.getLogger("athena_meeting")

# PulseAudio virtual device names
VIRTUAL_MIC_SINK = "athena_virtual_mic"
VIRTUAL_MIC_SOURCE = "athena_virtual_mic.monitor"
MONITOR_SOURCE = "athena_meeting_monitor"


class PulseAudioBridge:
    """Manages PulseAudio virtual devices for meeting audio routing."""

    def __init__(self):
        self._sink_module_id = None
        self._monitor_module_id = None

    def setup(self):
        """Create virtual audio devices.

        Creates:
        - athena_virtual_mic: null sink where TTS audio goes
        - athena_virtual_mic.monitor: Zoom/Teams picks this up as microphone
        """
        logger.info("Setting up PulseAudio virtual devices...")

        # Create virtual microphone (null sink)
        result = subprocess.run(
            ["pactl", "load-module", "module-null-sink",
             f"sink_name={VIRTUAL_MIC_SINK}",
             "sink_properties=device.description='Athena_Virtual_Mic'",
             "rate=24000", "channels=1"],
            capture_output=True, text=True
        )
        if result.returncode == 0:
            self._sink_module_id = result.stdout.strip()
            logger.info("Virtual mic sink created (module %s)", self._sink_module_id)
        else:
            logger.error("Failed to create virtual mic: %s", result.stderr)
            return False

        # Make the monitor source visible as an input device
        # (Zoom/Teams will see "Monitor of Athena_Virtual_Mic")
        subprocess.run(
            ["pactl", "set-default-source", VIRTUAL_MIC_SOURCE],
            capture_output=True
        )

        logger.info("Virtual audio devices ready:")
        logger.info("  TTS output -> %s", VIRTUAL_MIC_SINK)
        logger.info("  Zoom mic   <- %s", VIRTUAL_MIC_SOURCE)
        return True

    def teardown(self):
        """Remove virtual audio devices."""
        if self._sink_module_id:
            subprocess.run(
                ["pactl", "unload-module", self._sink_module_id],
                capture_output=True
            )
            logger.info("Virtual mic sink removed")
        if self._monitor_module_id:
            subprocess.run(
                ["pactl", "unload-module", self._monitor_module_id],
                capture_output=True
            )

    def play_audio_to_virtual_mic(self, audio_file):
        """Play audio file to the virtual microphone sink.

        Args:
            audio_file: Path to WAV file.
        """
        subprocess.Popen(
            ["paplay", "--device", VIRTUAL_MIC_SINK, audio_file],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
        )

    def play_raw_audio(self, audio_data, sample_rate=24000):
        """Play raw PCM audio bytes to virtual mic.

        Args:
            audio_data: Raw int16 PCM bytes.
            sample_rate: Sample rate in Hz.
        """
        proc = subprocess.Popen(
            ["pacat", "--device", VIRTUAL_MIC_SINK,
             "--format=s16le", "--channels=1",
             f"--rate={sample_rate}"],
            stdin=subprocess.PIPE,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
        )
        proc.stdin.write(audio_data)
        proc.stdin.close()
        proc.wait()


class MeetingAudioCapture:
    """Captures meeting audio from the default system source."""

    def __init__(self, callback, sample_rate=16000, chunk_duration=5.0):
        """
        Args:
            callback: Function called with (audio_bytes, sample_rate) when speech detected.
            sample_rate: Capture sample rate.
            chunk_duration: Seconds of audio per chunk.
        """
        self.callback = callback
        self.sample_rate = sample_rate
        self.chunk_duration = chunk_duration
        self._process = None
        self._running = False
        self._thread = None

    def start(self):
        """Start capturing audio from default source."""
        self._running = True
        self._thread = threading.Thread(target=self._capture_loop, daemon=True)
        self._thread.start()
        logger.info("Meeting audio capture started (rate=%d, chunk=%.1fs)",
                    self.sample_rate, self.chunk_duration)

    def stop(self):
        """Stop capturing."""
        self._running = False
        if self._process:
            self._process.terminate()
        if self._thread:
            self._thread.join(timeout=2)

    def _capture_loop(self):
        """Continuously capture audio chunks."""
        chunk_bytes = int(self.sample_rate * self.chunk_duration * 2)  # 16-bit

        while self._running:
            try:
                # Record a chunk using parec
                self._process = subprocess.Popen(
                    ["parec", "--format=s16le", "--channels=1",
                     f"--rate={self.sample_rate}",
                     "--raw"],
                    stdout=subprocess.PIPE, stderr=subprocess.DEVNULL
                )

                audio_data = self._process.stdout.read(chunk_bytes)
                self._process.terminate()
                self._process.wait()

                if audio_data and len(audio_data) >= chunk_bytes // 2:
                    # Simple VAD: check if audio has energy above threshold
                    import numpy as np
                    samples = np.frombuffer(audio_data, dtype=np.int16).astype(np.float32)
                    rms = np.sqrt(np.mean(samples ** 2))

                    if rms > 500:  # Threshold for speech detection
                        logger.debug("Speech detected (RMS=%.0f), processing...", rms)
                        self.callback(audio_data, self.sample_rate)
                    else:
                        logger.debug("Silence (RMS=%.0f)", rms)

            except Exception as e:
                logger.error("Capture error: %s", e)
                time.sleep(1)


class AthenaMeetingBridge:
    """Full meeting bridge: capture → STT → brain → Phi-3 → TTS → virtual mic."""

    def __init__(self, auto_respond=True, response_delay=1.0):
        """
        Args:
            auto_respond: Automatically respond to detected speech.
            response_delay: Seconds to wait after speech before responding.
        """
        self.auto_respond = auto_respond
        self.response_delay = response_delay
        self.pulse = PulseAudioBridge()
        self.brain = None
        self.phi3 = None
        self.hybrid = None
        self.tts = None
        self._last_speech_time = 0
        self._pending_text = None

    def setup(self):
        """Initialize all components."""
        # PulseAudio
        if not self.pulse.setup():
            return False

        # Brain connection
        try:
            from brain_client import BrainProxy, is_daemon_running
            if is_daemon_running():
                self.brain = BrainProxy()
                logger.info("Connected to brain daemon")
            else:
                logger.warning("Brain daemon not running — responses will be limited")
        except Exception as e:
            logger.warning("Brain connection failed: %s", e)

        # Phi-3
        try:
            from phi3_decoder import Phi3Decoder
            from hybrid_decoder import HybridDecoder
            self.phi3 = Phi3Decoder()
            if self.phi3.available:
                self.hybrid = HybridDecoder(phi3_decoder=self.phi3, brain=self.brain)
                logger.info("Phi-3 language cortex ready")
            else:
                logger.warning("Phi-3 model not available")
        except Exception as e:
            logger.warning("Phi-3 init failed: %s", e)

        # TTS
        try:
            from athena_tts import AthenaTTS
            self.tts = AthenaTTS()
            logger.info("TTS engine ready")
        except Exception as e:
            logger.warning("TTS init failed: %s", e)

        return True

    def on_speech_detected(self, audio_data, sample_rate):
        """Called when speech is detected from meeting audio.

        Uses Whisper (if available) or Web Speech API fallback for STT.
        """
        text = self._speech_to_text(audio_data, sample_rate)
        if not text:
            return

        logger.info("Heard: %s", text[:100])

        if self.auto_respond:
            # Delay to avoid interrupting
            self._pending_text = text
            self._last_speech_time = time.time()
            threading.Timer(self.response_delay, self._check_and_respond).start()

    def _speech_to_text(self, audio_data, sample_rate):
        """Convert audio to text using Whisper or fallback."""
        try:
            import whisper
            import numpy as np
            import tempfile
            import wave

            # Save to temp WAV
            with tempfile.NamedTemporaryFile(suffix='.wav', delete=False) as f:
                temp_path = f.name
                with wave.open(f, 'wb') as wf:
                    wf.setnchannels(1)
                    wf.setsampwidth(2)
                    wf.setframerate(sample_rate)
                    wf.writeframes(audio_data)

            # Transcribe
            if not hasattr(self, '_whisper_model'):
                logger.info("Loading Whisper tiny model...")
                self._whisper_model = whisper.load_model("tiny")
            result = self._whisper_model.transcribe(temp_path)
            os.unlink(temp_path)
            return result.get('text', '').strip()

        except ImportError:
            # Whisper not installed — try ffmpeg + vosk or return empty
            logger.debug("Whisper not available, skipping STT")
            return None
        except Exception as e:
            logger.error("STT failed: %s", e)
            return None

    def _check_and_respond(self):
        """Check if we should respond (no new speech for response_delay seconds)."""
        if not self._pending_text:
            return
        if time.time() - self._last_speech_time < self.response_delay:
            return  # More speech came in, wait

        text = self._pending_text
        self._pending_text = None
        self._generate_response(text)

    def _generate_response(self, text):
        """Generate and speak a response."""
        response_text = ""

        # Generate via Phi-3 + brain
        if self.hybrid:
            try:
                result = self.hybrid.respond(text, brain=self.brain)
                response_text = result.get('text', '')
            except Exception as e:
                logger.error("Response generation failed: %s", e)

        if not response_text:
            response_text = "I'm processing that thought."

        logger.info("Responding: %s", response_text[:100])

        # Synthesize speech
        if self.tts and self.tts.available:
            try:
                result = self.tts.speak(response_text, brain=self.brain)
                if result and result.get('audio') is not None:
                    import numpy as np
                    audio = np.array(result['audio'], dtype=np.float32)
                    audio_int16 = (audio * 32767).astype(np.int16)
                    self.pulse.play_raw_audio(
                        audio_int16.tobytes(),
                        sample_rate=result.get('sample_rate', 24000)
                    )
                    return
            except Exception as e:
                logger.error("TTS failed: %s", e)

        # Fallback: use espeak
        try:
            subprocess.run(
                ["espeak", "-s", "150", "--stdout", response_text],
                stdout=subprocess.PIPE, stderr=subprocess.DEVNULL
            )
        except Exception:
            pass

    def teardown(self):
        """Clean up."""
        self.pulse.teardown()
        if self.tts:
            self.tts.unload()
        if self.phi3:
            self.phi3.unload()


def setup_obs_instructions():
    """Print OBS setup instructions."""
    print("""
╔══════════════════════════════════════════════════════════════╗
║  OBS Virtual Camera Setup                                    ║
╠══════════════════════════════════════════════════════════════╣
║                                                              ║
║  1. Open OBS Studio:  obs &                                  ║
║                                                              ║
║  2. Add a Browser Source:                                    ║
║     Sources → + → Browser                                    ║
║     URL: file:///home/bbrelin/nimcp/avatar/index.html        ║
║     Width: 1280  Height: 720                                 ║
║                                                              ║
║  3. Start Virtual Camera:                                    ║
║     Tools → Start Virtual Camera                             ║
║     (or click "Start Virtual Camera" button)                 ║
║                                                              ║
║  4. In Zoom/Teams:                                           ║
║     Settings → Video → Camera: "OBS Virtual Camera"          ║
║     Settings → Audio → Microphone: "Athena Virtual Mic"      ║
║                                                              ║
║  Athena will appear as a participant with her own             ║
║  camera and microphone, responding to conversation.           ║
║                                                              ║
╚══════════════════════════════════════════════════════════════╝
""")


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Athena Meeting Bridge")
    parser.add_argument("--no-auto-respond", action="store_true",
                        help="Don't auto-respond to speech")
    parser.add_argument("--delay", type=float, default=2.0,
                        help="Response delay after speech (seconds)")
    parser.add_argument("--setup-only", action="store_true",
                        help="Only set up virtual devices and print instructions")
    args = parser.parse_args()

    logging.basicConfig(level=logging.INFO,
                        format="%(asctime)s [%(name)s] %(message)s")

    bridge = AthenaMeetingBridge(
        auto_respond=not args.no_auto_respond,
        response_delay=args.delay
    )

    if not bridge.setup():
        print("Failed to set up meeting bridge")
        sys.exit(1)

    setup_obs_instructions()

    if args.setup_only:
        print("Virtual devices created. Press Ctrl+C to tear down.")
        try:
            signal.pause()
        except KeyboardInterrupt:
            pass
        bridge.teardown()
        return

    # Start audio capture
    capture = MeetingAudioCapture(
        callback=bridge.on_speech_detected,
        chunk_duration=5.0
    )
    capture.start()

    print("\nAthena Meeting Bridge is running.")
    print("She will listen to meeting audio and respond when appropriate.")
    print("Press Ctrl+C to stop.\n")

    try:
        signal.pause()
    except KeyboardInterrupt:
        pass

    print("\nShutting down...")
    capture.stop()
    bridge.teardown()
    print("Meeting bridge stopped.")


if __name__ == "__main__":
    main()
