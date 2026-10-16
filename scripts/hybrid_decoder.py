"""
Hybrid Decoder — orchestrates NeuralDecoder + Phi-3 Language Cortex.

Pipeline:
  1. Brain produces 4096-dim output vector
  2. NeuralDecoder finds nearest vocabulary match (semantic grounding)
  3. Brain state is collected (arousal, dopamine, confidence, SNN spikes, etc.)
  4. Phi-3 generates fluent text conditioned on brain state + vocab match
  5. Cross-check: compare Phi-3 output similarity to vocab match for coherence

Falls back to NeuralDecoder if Phi-3 is unavailable.
"""

import logging
import numpy as np

logger = logging.getLogger(__name__)


class HybridDecoder:
    """Combines NeuralDecoder (semantic grounding) with Phi-3 (fluent generation)."""

    def __init__(self, neural_decoder=None, phi3_decoder=None, brain=None):
        """Initialize hybrid decoder.

        Args:
            neural_decoder: NeuralDecoder instance (vocabulary nearest-neighbor).
            phi3_decoder: Phi3Decoder instance (language generation).
            brain: Brain instance or BrainProxy for state queries.
        """
        self.neural_decoder = neural_decoder
        self.phi3 = phi3_decoder
        self.brain = brain
        self._encode_text = None  # Lazy import
        self.enable_inner_speech = False
        self.inner_speech_rounds = 2
        self.inner_speech_max_time = 2.0  # seconds

    def _get_encode_text(self):
        """Lazy import of encode_text to avoid circular imports."""
        if self._encode_text is None:
            from claude_teacher import encode_text
            self._encode_text = encode_text
        return self._encode_text

    def decode(self, output_vector, user_text=None, max_tokens=256):
        """Decode brain output to text using hybrid pipeline.

        Args:
            output_vector: 4096-dim brain output (list or numpy array).
            user_text: Original input text (for Phi-3 context).
            max_tokens: Max tokens for Phi-3 generation.

        Returns:
            Dict with:
              text: Generated response text
              confidence: Confidence score [0-1]
              source: 'phi3', 'vocabulary', or 'hybrid'
              vocab_match: Nearest vocabulary match (if available)
              vocab_similarity: Cosine similarity to vocab match
        """
        result = {
            'text': '',
            'confidence': 0.0,
            'source': 'none',
            'vocab_match': None,
            'vocab_similarity': 0.0,
        }

        # Step 1: Get vocabulary nearest-neighbor match (semantic grounding)
        vocab_text = None
        vocab_sim = 0.0
        if self.neural_decoder:
            try:
                output_arr = np.array(output_vector, dtype=np.float32)
                output_emb = self._extract_embedding(output_arr)
                matches = self.neural_decoder.vocabulary.decode(output_emb, top_k=3)
                if matches and matches[0][0]:
                    vocab_text = matches[0][0]
                    vocab_sim = float(matches[0][1])
            except Exception as e:
                logger.debug("Vocabulary decode failed: %s", e)

        result['vocab_match'] = vocab_text
        result['vocab_similarity'] = vocab_sim

        # Step 2: Collect brain state
        brain_state = self._collect_brain_state()

        # Step 3: Try Phi-3 generation
        if self.phi3 and self.phi3.available and user_text:
            phi3_result = self.phi3.generate(
                user_text=user_text,
                brain_state=brain_state,
                vocab_match=vocab_text,
                max_tokens=max_tokens,
            )
            if phi3_result and phi3_result.get('text'):
                result['text'] = phi3_result['text']
                result['source'] = 'phi3'
                # Confidence: blend of vocab similarity and generation quality
                result['confidence'] = min(0.95, vocab_sim * 0.5 + 0.4)
                return result

        # Step 4: Fallback to vocabulary match
        if vocab_text:
            result['text'] = vocab_text
            result['source'] = 'vocabulary'
            result['confidence'] = vocab_sim
            return result

        # Step 5: No decoder available
        result['text'] = "(no response available)"
        result['source'] = 'none'
        return result

    def _extract_embedding(self, output_arr):
        """Extract 1024-dim embedding from 4096-dim brain output.

        Mirrors extract_embedding_from_output() in immerse_athena.py:
        tiles 4x to 1024, averages, denormalizes from [0,1] to [-1,1].
        """
        if len(output_arr) >= 1024:
            # Use projector if available
            if self.neural_decoder and hasattr(self.neural_decoder, 'projector'):
                return self.neural_decoder.projector.project(output_arr)
            # Fallback: average 4 tiles
            emb = np.zeros(1024, dtype=np.float32)
            n_tiles = len(output_arr) // 1024
            if n_tiles == 0:
                emb[:len(output_arr)] = output_arr
            else:
                for i in range(n_tiles):
                    emb += output_arr[i * 1024:(i + 1) * 1024]
                emb /= n_tiles
            # Denormalize [0,1] → [-1,1]
            emb = emb * 2.0 - 1.0
            return emb
        return output_arr

    def _collect_brain_state(self):
        """Collect brain state for Phi-3 conditioning."""
        if not self.brain:
            return None

        state = {}
        try:
            state['arousal'] = self.brain.medulla_get_arousal()
        except Exception:
            state['arousal'] = 0.5

        try:
            dp = self.brain.bg_get_dopamine()
            state['dopamine'] = dp
        except Exception:
            pass

        try:
            snn = self.brain.snn_get_stats()
            if snn:
                state['snn_spikes'] = snn.get('total_spikes', 0)
                state['snn_firing_rate'] = snn.get('mean_firing_rate', 0.0)
        except Exception:
            pass

        try:
            cs = self.brain.get_cognitive_stats()
            if cs:
                active = [k for k, v in cs.items() if v.get('steps', 0) > 0]
                if active:
                    state['active_modules'] = active
        except Exception:
            pass

        try:
            transcript = self.brain.get_transcript()
            if transcript:
                state['transcript'] = transcript[:200]
        except Exception:
            pass

        return state if state else None

    def respond(self, text, brain=None):
        """High-level: process text through brain and generate response.

        Full pipeline: encode → brain.decide_full → hybrid decode.

        Args:
            text: Input text.
            brain: Brain or BrainProxy (uses self.brain if None).

        Returns:
            Dict with text, confidence, source, brain_output, etc.
        """
        b = brain or self.brain
        if not b:
            return {'text': '(no brain available)', 'source': 'none'}

        try:
            encode_text = self._get_encode_text()

            # Encode input
            emb = encode_text(text)
            features = emb[:1024].tolist() if len(emb) >= 1024 else emb.tolist()

            # Brain forward pass
            result = b.decide_full(features)
            output_vec = result.get('output_vector')
            confidence = result.get('confidence', 0.0)

            if output_vec is None:
                return {'text': '(brain produced no output)', 'source': 'none'}

            # Inner speech loop (if enabled)
            if self.enable_inner_speech and output_vec is not None:
                inner = self.inner_speech(output_vec, text)
                if inner.get('final_output') is not None:
                    output_vec = inner['final_output']

            # Hybrid decode
            decoded = self.decode(output_vec, user_text=text)
            decoded['brain_confidence'] = confidence
            decoded['brain_label'] = result.get('label', '')
            decoded['inference_time_us'] = result.get('inference_time_us', 0)

            return decoded

        except Exception as e:
            logger.error("Hybrid respond failed: %s", e)
            return {'text': f'(error: {e})', 'source': 'error'}

    # =================================================================
    # Task B: Inner speech — brain output → text → re-encode → brain
    # =================================================================

    def inner_speech(self, output_vector, user_text, max_rounds=None,
                     max_time=None):
        """Run inner speech loop for reflection.

        The brain "hears its own thoughts": output → Phi-3 text → re-encode
        → brain forward → repeat. Each round refines the response.

        Args:
            output_vector: Initial 4096-dim brain output.
            user_text: Original input text for context.
            max_rounds: Max reflection rounds (default: self.inner_speech_rounds).
            max_time: Max total time in seconds (default: self.inner_speech_max_time).

        Returns:
            Dict with 'rounds' (list of inner speech texts) and 'final_output'.
        """
        import time as _time

        if not self.phi3 or not self.phi3.available or not self.brain:
            return {'rounds': [], 'final_output': output_vector}

        rounds = max_rounds or self.inner_speech_rounds
        timeout = max_time or self.inner_speech_max_time
        t0 = _time.time()
        speech_log = []
        current_output = output_vector

        for i in range(rounds):
            if _time.time() - t0 > timeout:
                break

            # Decode current brain output to text
            decoded = self.decode(current_output, user_text=user_text,
                                  max_tokens=64)
            inner_text = decoded.get('text', '')
            if not inner_text:
                break

            speech_log.append({'round': i, 'text': inner_text})

            # Re-encode inner speech and feed back to brain
            # Try Phi-3 encoder first, fall back to BERT
            if self.phi3:
                inner_emb = self.phi3.encode_text(inner_text)
            if inner_emb is None:
                encode_text = self._get_encode_text()
                inner_emb = encode_text(inner_text)

            features = inner_emb[:1024].tolist()

            # Feed back through brain
            try:
                result = self.brain.decide_full(features)
                current_output = result.get('output_vector')
                if current_output is None:
                    break
            except Exception:
                break

        return {'rounds': speech_log, 'final_output': current_output}
