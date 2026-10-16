"""
Athena Prompt Assembly — Memory-Augmented Inference Pipeline

Takes the brain's raw decide() output and enriches it with:
1. Engram recall (past experiences)
2. Semantic concepts (knowledge graph)
3. Autobiographical context (personal history)
4. OOD assessment (novelty/familiarity)
5. Emotional context (valence/arousal)

Produces a structured prompt for Phi-3 to generate natural language.
"""


class AthenaPromptAssembler:
    """Assembles enriched prompts from brain output + memory systems."""

    def __init__(self, brain):
        self.brain = brain

    def assemble(self, input_text, brain_output, features=None):
        context = {}

        # 1. Memory text search
        try:
            context['memory_hits'] = self.brain.memory_search_text(
                input_text, max_results=5) or []
        except Exception:
            context['memory_hits'] = []

        # 2. Vector similarity search
        if features is not None:
            try:
                emb = features.tolist() if hasattr(features, 'tolist') else list(features)
                context['similar_memories'] = self.brain.memory_search_similar(emb, top_k=3) or []
            except Exception:
                context['similar_memories'] = []
        else:
            context['similar_memories'] = []

        # 3. OOD assessment
        try:
            ood = self.brain.ood_stats()
            context['ood_stats'] = ood
            is_ood = ood and ood.get('ood_rate', 0) > 0.5
        except Exception:
            is_ood = False
            context['ood_stats'] = None

        # 4. Memory store stats
        try:
            context['memory_stats'] = self.brain.memory_store_stats()
        except Exception:
            context['memory_stats'] = None

        # 5. Memory health
        try:
            context['memory_healthy'] = self.brain.memory_is_healthy()
        except Exception:
            context['memory_healthy'] = True

        # 6. Build structured prompt
        confidence = brain_output.get('confidence', 0.0)
        label = brain_output.get('label', '')
        prompt = self._build_prompt(input_text, label, confidence, context, is_ood)

        # 7. Adjust confidence
        adjusted = self._adjust_confidence(confidence, context, is_ood)

        return {
            'prompt': prompt,
            'context': context,
            'confidence': adjusted,
            'is_ood': is_ood,
            'label': label
        }

    def _build_prompt(self, input_text, label, confidence, context, is_ood):
        parts = []
        parts.append("You are Athena, a neural cognitive system. You think through "
                      "biological neural circuits and have personal memories of learning.")
        parts.append(f"\nUser asks: {input_text}")

        if label:
            parts.append(f"\nYour neural response suggests: {label} "
                         f"(confidence: {confidence:.2f})")

        hits = context.get('memory_hits', [])
        if hits:
            parts.append(f"\nYour memory recalls {len(hits)} related experiences.")

        similar = context.get('similar_memories', [])
        if similar:
            parts.append(f"You've seen {len(similar)} similar patterns before.")
            for mem_id, distance in similar[:3]:
                sim = max(0, 1.0 - distance)
                parts.append(f"  - Memory #{mem_id}: {sim:.0%} similar")

        stats = context.get('memory_stats')
        if stats:
            parts.append(f"\nYour knowledge base: {stats.get('total_concepts', 0)} concepts, "
                         f"{stats.get('total_engrams', 0)} memories.")

        if is_ood:
            parts.append("\n[NOTE: This input is UNFAMILIAR. Be honest about uncertainty.]")

        if confidence < 0.3:
            parts.append("\n[Your neural confidence is LOW. Express uncertainty clearly.]")
        elif confidence > 0.8:
            parts.append("\n[Your neural confidence is HIGH. You can be assertive.]")

        parts.append("\nRespond naturally as Athena. Draw on your memories and knowledge. "
                     "If uncertain, say so honestly.")
        return "\n".join(parts)

    def _adjust_confidence(self, raw, context, is_ood):
        adj = raw
        if is_ood:
            adj *= 0.5
        similar = context.get('similar_memories', [])
        if similar:
            best = max((1.0 - d for _, d in similar), default=0)
            if best > 0.8:
                adj = min(adj * 1.2, 1.0)
        if not context.get('memory_hits') and not similar:
            adj *= 0.7
        return max(0.0, min(1.0, adj))
