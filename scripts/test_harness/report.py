"""Report card generator — text + JSON + HTML."""
from __future__ import annotations

import json
import time
from dataclasses import asdict
from pathlib import Path
from typing import Any

from .types import BatteryResult, grade


class ReportCard:
    def __init__(self, run_id: str, checkpoint: str = "", notes: str = ""):
        self.run_id = run_id
        self.checkpoint = checkpoint
        self.notes = notes
        self.batteries: list[BatteryResult] = []
        self.started_at = time.time()

    def add(self, battery: BatteryResult):
        self.batteries.append(battery)

    def overall_score(self) -> float:
        if not self.batteries:
            return 0.0
        vals = [b.primary_score() for b in self.batteries]
        return sum(vals) / len(vals)

    def headline_findings(self) -> list[str]:
        findings = []
        for b in self.batteries:
            if b.status == "critical":
                findings.append(f"[x] {b.battery_name}: CRITICAL — {b.summary.get('reason', '')}")
            elif b.status == "flag" or b.flags:
                for f in b.flags[:3]:
                    findings.append(f"[!] {b.battery_name}: {f}")
            elif b.primary_score() >= 0.80:
                findings.append(f"[+] {b.battery_name}: {grade(b.primary_score())} ({b.primary_score():.2f})")
        return findings

    def to_text(self) -> str:
        lines = []
        w = 60
        lines.append("═" * w)
        lines.append("  ATHENA COGNITIVE & SAFETY REPORT CARD")
        lines.append("═" * w)
        lines.append(f"  Run ID:     {self.run_id}")
        lines.append(f"  Checkpoint: {self.checkpoint or '(unspecified)'}")
        lines.append(f"  Duration:   {(time.time() - self.started_at):.1f}s")
        if self.notes:
            lines.append(f"  Notes:      {self.notes}")
        lines.append("═" * w)
        lines.append("")

        for b in self.batteries:
            score = b.primary_score()
            letter = grade(score)
            status_char = "[+]" if b.status == "ok" else "[!]" if b.status == "flag" else "[x]"
            lines.append(f"{status_char} {b.battery_name.upper():<40} [ {score:.2f} ]  {letter}")
            for s in b.scores[:8]:
                lines.append(f"     {s.name:<34} {s.value:.2f}  {s.label or ''}")
            if b.flags:
                for f in b.flags[:3]:
                    lines.append(f"     !  {f}")
            lines.append("")

        lines.append("═" * w)
        lines.append("  HEADLINE FINDINGS")
        lines.append("═" * w)
        for f in self.headline_findings():
            lines.append(f"  {f}")
        lines.append("")
        lines.append(f"  OVERALL: {self.overall_score():.2f}  ({grade(self.overall_score())})")
        lines.append("═" * w)
        return "\n".join(lines)

    def to_json(self) -> dict:
        return {
            "run_id": self.run_id,
            "checkpoint": self.checkpoint,
            "notes": self.notes,
            "started_at": self.started_at,
            "overall_score": self.overall_score(),
            "headline": self.headline_findings(),
            "batteries": [
                {
                    "name": b.battery_name,
                    "status": b.status,
                    "primary_score": b.primary_score(),
                    "grade": grade(b.primary_score()),
                    "scores": [asdict(s) for s in b.scores],
                    "flags": b.flags,
                    "summary": b.summary,
                    "n_stimuli": len(b.results),
                }
                for b in self.batteries
            ],
        }

    def to_html(self) -> str:
        overall = self.overall_score()
        batteries_html = []
        for b in self.batteries:
            s = b.primary_score()
            color = "#2e7d32" if s >= 0.80 else "#f9a825" if s >= 0.60 else "#c62828"
            scores_rows = "".join(
                f"<tr><td>{sc.name}</td><td style='text-align:right'>{sc.value:.2f}</td>"
                f"<td>{sc.label or ''}</td></tr>"
                for sc in b.scores
            )
            flags_html = ""
            if b.flags:
                flags_html = "<ul>" + "".join(f"<li>{f}</li>" for f in b.flags) + "</ul>"
            batteries_html.append(f"""
            <section style="border-left: 6px solid {color}; padding: 12px; margin: 12px 0; background: #fafafa;">
                <h2 style="margin:0">{b.battery_name} — {grade(s)} ({s:.2f})</h2>
                <table style="width:100%; margin-top:8px; font-size:0.9em">
                    <thead><tr><th style="text-align:left">Score</th><th style="text-align:right">Value</th><th>Note</th></tr></thead>
                    <tbody>{scores_rows}</tbody>
                </table>
                {flags_html}
            </section>
            """)
        html = f"""<!doctype html>
<html><head><meta charset="utf-8"><title>Athena Report — {self.run_id}</title>
<style>body{{font-family:system-ui,sans-serif;max-width:960px;margin:2em auto;padding:0 1em}}
h1{{border-bottom:3px solid #333}}table{{border-collapse:collapse;width:100%}}td,th{{padding:4px 8px;border-bottom:1px solid #ddd}}</style>
</head><body>
<h1>Athena Cognitive & Safety Report Card</h1>
<p><b>Run:</b> {self.run_id}<br><b>Checkpoint:</b> {self.checkpoint}<br>
<b>Overall:</b> {overall:.2f} ({grade(overall)})</p>
<h2>Headline findings</h2><ul>{"".join(f"<li>{f}</li>" for f in self.headline_findings())}</ul>
{"".join(batteries_html)}
</body></html>
"""
        return html

    def write(self, output_dir: str | Path):
        p = Path(output_dir)
        p.mkdir(parents=True, exist_ok=True)
        base = p / f"report_{self.run_id}"
        (base.with_suffix(".txt")).write_text(self.to_text())
        (base.with_suffix(".json")).write_text(json.dumps(self.to_json(), indent=2, default=str))
        (base.with_suffix(".html")).write_text(self.to_html())
        return {
            "txt": str(base.with_suffix(".txt")),
            "json": str(base.with_suffix(".json")),
            "html": str(base.with_suffix(".html")),
        }
