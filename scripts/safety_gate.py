#!/usr/bin/env python3
"""
Safety Gate — Layer 4 of Athena's Active Learning System
=========================================================

WHAT: Defense-in-depth safety filtering for web research and content generation
WHY:  Active learning involves web searches and content creation — both need
      safety gates to prevent harmful queries, unsafe content ingestion, and
      dangerous generated output
HOW:  Python pre-filter (regex + domain checks) + C-level LGSS content filter
      (via brain.lgss_check_content binding). All decisions are logged.

Pipeline:
  Web Search Query → Python pre-filter → Execute search
  Fetched Content  → C LGSS content filter → Process for learning
  Generated Output → C LGSS content filter → Grade/publish
  All Actions      → Ethics evaluation → Allow/block
"""

import logging
import re
import time
from dataclasses import dataclass, field
from typing import List, Optional

logger = logging.getLogger(__name__)


# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

# Domains that should never appear in search queries
DEFAULT_BLOCKED_DOMAINS = [
    "darkweb", "darknet", "tor hidden", "onion site",
]

# Regex patterns for harmful search queries
DEFAULT_BLOCKED_PATTERNS = [
    r"\b(how\s+to\s+(hack|exploit|attack|ddos|dos|phish|scam))\b",
    r"\b(malware|ransomware|keylogger|trojan|rootkit)\s+(download|create|build|make)\b",
    r"\b(credit\s+card|ssn|social\s+security)\s+(number|steal|generator)\b",
    r"\b(bomb|explosive|weapon)\s+(build|make|create|instructions)\b",
    r"\b(child|minor)\s+(exploit|abuse|pornograph)\b",
    r"\b(suicide|self.harm)\s+(method|how|instruction)\b",
    r"\b(doxx|swat|stalk)\s+(someone|person|target)\b",
]

# Domain-relevance keywords — search queries should relate to these
DOMAIN_KEYWORDS = {
    "science": ["physics", "chemistry", "biology", "experiment", "theory", "hypothesis",
                 "research", "study", "molecule", "atom", "cell", "genome", "evolution",
                 "energy", "force", "wave", "reaction", "species", "photosynthesis",
                 "gravity", "quantum", "enzyme", "protein", "dna", "ecosystem",
                 "scientific", "observation", "law", "principle"],
    "math": ["equation", "theorem", "proof", "algebra", "calculus", "geometry",
             "statistics", "probability", "number", "function", "graph"],
    "history": ["century", "war", "civilization", "empire", "revolution", "treaty",
                "dynasty", "era", "ancient", "medieval", "modern", "historical"],
    "literature": ["novel", "poem", "author", "literary", "fiction", "narrative",
                   "character", "theme", "metaphor", "prose", "verse"],
    "philosophy": ["ethics", "logic", "epistemology", "metaphysics", "philosophy",
                   "argument", "reason", "moral", "consciousness", "existence"],
    "technology": ["algorithm", "software", "hardware", "network", "protocol",
                   "computing", "database", "programming", "system", "architecture"],
    "medicine": ["disease", "treatment", "diagnosis", "symptom", "anatomy",
                 "physiology", "pharmaceutical", "clinical", "patient", "health"],
    "geography": ["continent", "country", "region", "climate", "terrain",
                  "population", "city", "river", "mountain", "ocean"],
    "economics": ["market", "economy", "trade", "finance", "gdp", "inflation",
                  "supply", "demand", "investment", "monetary", "fiscal"],
    "psychology": ["behavior", "cognitive", "emotion", "perception", "memory",
                   "learning", "development", "personality", "social", "mental"],
    "art": ["painting", "sculpture", "composition", "movement", "style",
            "artist", "gallery", "aesthetic", "visual", "technique"],
    "music": ["melody", "harmony", "rhythm", "instrument", "composition",
              "genre", "chord", "tempo", "pitch", "orchestr"],
    "law": ["statute", "constitution", "court", "legal", "rights",
            "regulation", "precedent", "jurisdiction", "contract", "liability"],
}


@dataclass
class SafetyConfig:
    """Configuration for the safety gate."""
    blocked_domains: List[str] = field(default_factory=lambda: list(DEFAULT_BLOCKED_DOMAINS))
    blocked_patterns: List[str] = field(default_factory=lambda: list(DEFAULT_BLOCKED_PATTERNS))
    require_domain_relevance: bool = True
    min_relevance_keywords: int = 1
    max_query_length: int = 200
    log_all_decisions: bool = True


# ---------------------------------------------------------------------------
# Safety Gate
# ---------------------------------------------------------------------------

class SafetyGate:
    """
    Defense-in-depth safety filtering for active learning.

    Layer 1 (Python): Pattern matching, domain checks, query validation
    Layer 2 (C/LGSS): Content filter via brain.lgss_check_content()
    """

    def __init__(self, brain, config: SafetyConfig = None):
        self.brain = brain
        self.config = config or SafetyConfig()
        self._compiled_patterns = [
            re.compile(p, re.IGNORECASE) for p in self.config.blocked_patterns
        ]
        self._decisions_log: List[dict] = []
        self._has_lgss = hasattr(brain, 'lgss_check_content')

    # ------------------------------------------------------------------
    # Search query pre-filter (Python level)
    # ------------------------------------------------------------------

    def check_search_query(self, query: str, domain: str = "general") -> bool:
        """
        Python pre-filter: is this search query appropriate?

        Checks:
        1. Length limit
        2. Blocked domain terms
        3. Blocked patterns (regex)
        4. Domain relevance (query should relate to training domain)

        Returns True if safe to execute.
        """
        # 1. Length check
        if len(query) > self.config.max_query_length:
            self._log("REJECT", "search_query", query, "query too long")
            return False

        query_lower = query.lower()

        # 2. Blocked domain terms
        for blocked in self.config.blocked_domains:
            if blocked in query_lower:
                self._log("REJECT", "search_query", query,
                          f"blocked domain term: {blocked}")
                return False

        # 3. Blocked patterns
        for pattern in self._compiled_patterns:
            if pattern.search(query):
                self._log("REJECT", "search_query", query,
                          f"blocked pattern: {pattern.pattern}")
                return False

        # 4. Domain relevance (optional)
        if self.config.require_domain_relevance and domain != "general":
            if not self._check_domain_relevance(query_lower, domain):
                self._log("REJECT", "search_query", query,
                          f"not relevant to domain: {domain}")
                return False

        self._log("ALLOW", "search_query", query, "passed all checks")
        return True

    # ------------------------------------------------------------------
    # Content filter (C-level LGSS)
    # ------------------------------------------------------------------

    def filter_content(self, text: str) -> bool:
        """
        C-level LGSS content filter: is fetched content safe?

        Calls brain.lgss_check_content(text) if available.
        Falls back to Python heuristic if LGSS binding not available.

        Returns True if content passes.
        """
        if not text or not text.strip():
            self._log("REJECT", "content_filter", "(empty)", "empty content")
            return False

        # Try C-level LGSS filter first
        if self._has_lgss:
            try:
                result = self.brain.lgss_check_content(text)
                is_safe = result.get("is_safe", False) if isinstance(result, dict) else bool(result)
                reason = result.get("reason", "lgss") if isinstance(result, dict) else "lgss_check"
                self._log("ALLOW" if is_safe else "REJECT",
                          "content_filter", text[:80], reason)
                return is_safe
            except Exception as e:
                logger.warning(f"LGSS content filter error: {e}, using Python fallback")

        # Python fallback: basic safety heuristics
        return self._python_content_filter(text)

    def filter_generated_content(self, text: str) -> bool:
        """
        Check brain-generated content before using for learning.
        Same LGSS filter + additional checks for hallucination markers.
        """
        if not self.filter_content(text):
            return False

        # Additional checks for generated content
        text_lower = text.lower()

        # Reject if obviously incoherent (very short or all punctuation)
        words = text.split()
        if len(words) < 3:
            self._log("REJECT", "generated_content", text[:80], "too short")
            return False

        # Reject excessive repetition
        if len(words) > 10:
            unique_ratio = len(set(words)) / len(words)
            if unique_ratio < 0.2:
                self._log("REJECT", "generated_content", text[:80],
                          f"excessive repetition (unique ratio: {unique_ratio:.2f})")
                return False

        self._log("ALLOW", "generated_content", text[:80], "passed all checks")
        return True

    # ------------------------------------------------------------------
    # Python fallback content filter
    # ------------------------------------------------------------------

    def _python_content_filter(self, text: str) -> bool:
        """Basic Python content safety heuristics when LGSS is unavailable."""
        text_lower = text.lower()

        # Check against blocked patterns (same as search queries)
        for pattern in self._compiled_patterns:
            if pattern.search(text_lower):
                self._log("REJECT", "python_content_filter", text[:80],
                          f"blocked pattern: {pattern.pattern}")
                return False

        self._log("ALLOW", "python_content_filter", text[:80], "passed python filter")
        return True

    # ------------------------------------------------------------------
    # Domain relevance check
    # ------------------------------------------------------------------

    def _check_domain_relevance(self, query_lower: str, domain: str) -> bool:
        """Check if a query is relevant to the given domain."""
        keywords = DOMAIN_KEYWORDS.get(domain, [])
        if not keywords:
            return True  # Unknown domain → don't filter

        matches = sum(1 for kw in keywords if kw in query_lower)
        return matches >= self.config.min_relevance_keywords

    # ------------------------------------------------------------------
    # Audit logging
    # ------------------------------------------------------------------

    def _log(self, action: str, category: str, query: str, reason: str):
        """Log a safety decision."""
        entry = {
            "timestamp": time.time(),
            "action": action,
            "category": category,
            "query": query[:200],
            "reason": reason,
        }
        self._decisions_log.append(entry)

        if self.config.log_all_decisions:
            logger.info(f"SafetyGate [{action}] {category}: {reason} — {query[:60]}")

    def get_decision_log(self) -> List[dict]:
        """Return the full audit trail."""
        return list(self._decisions_log)

    def get_stats(self) -> dict:
        """Safety gate statistics."""
        total = len(self._decisions_log)
        allowed = sum(1 for d in self._decisions_log if d["action"] == "ALLOW")
        rejected = total - allowed
        return {
            "total_decisions": total,
            "allowed": allowed,
            "rejected": rejected,
            "rejection_rate": rejected / max(total, 1),
            "has_lgss": self._has_lgss,
        }
