#!/usr/bin/env python3
"""
Coverage Analyzer - Analyze code coverage from gcov data

WHAT: Parse gcov coverage data and generate comprehensive report
WHY:  Identify uncovered code to reach 100% coverage
HOW:  Process .gcda files, extract coverage stats, identify gaps

NIMCP STANDARDS:
- Functions < 50 lines
- Guard clauses
- WHAT-WHY-HOW documentation
"""

import subprocess
from pathlib import Path
from typing import Tuple, Dict, List
from dataclasses import dataclass
import re

#==============================================================================
# Data Structures
#==============================================================================

@dataclass(frozen=True)
class FileCoverage:
    """
    Immutable coverage data for single file

    WHAT: Per-file coverage statistics
    WHY:  Track coverage at file granularity
    HOW:  Frozen dataclass with metrics
    """
    file_path: str
    total_lines: int
    covered_lines: int
    uncovered_lines: int
    coverage_percent: float
    uncovered_line_numbers: Tuple[int, ...]

@dataclass(frozen=True)
class CoverageReport:
    """
    Immutable overall coverage report

    WHAT: Complete coverage analysis
    WHY:  Track progress toward 100%
    HOW:  Aggregate file-level coverage
    """
    total_lines: int
    covered_lines: int
    coverage_percent: float
    file_coverages: Tuple[FileCoverage, ...]

#==============================================================================
# Pure Functions
#==============================================================================

def find_gcda_files(build_dir: Path) -> Tuple[Path, ...]:
    """
    Find all coverage data files

    WHAT: Discover .gcda files
    WHY:  Locate coverage data to analyze
    HOW:  Recursive glob

    COMPLEXITY: O(n) where n = number of files
    """
    if not build_dir.exists():
        return tuple()

    return tuple(sorted(build_dir.rglob("*.gcda")))

def run_gcov_on_file(gcda_file: Path, source_dir: Path) -> Dict[str, any]:
    """
    Run gcov on single file

    WHAT: Extract coverage data for one file
    WHY:  Get line-by-line coverage
    HOW:  Execute gcov command

    NOTE: Side effect - runs gcov
    """
    try:
        # Run gcov
        result = subprocess.run(
            ["gcov", str(gcda_file)],
            capture_output=True,
            text=True,
            cwd=gcda_file.parent
        )

        # Parse gcov output
        lines = result.stdout.split('\n')
        for line in lines:
            # Look for: "Lines executed:XX.XX% of YY"
            match = re.search(r'Lines executed:(\d+\.\d+)% of (\d+)', line)
            if match:
                percent = float(match.group(1))
                total = int(match.group(2))
                covered = int(total * percent / 100.0)

                return {
                    'coverage_percent': percent,
                    'total_lines': total,
                    'covered_lines': covered
                }

    except Exception:
        pass

    return None

def parse_gcov_file(gcov_file: Path) -> Tuple[int, ...]:
    """
    Parse .gcov file to find uncovered lines

    WHAT: Extract uncovered line numbers
    WHY:  Identify exactly what needs testing
    HOW:  Parse gcov output format

    COMPLEXITY: O(n) where n = lines in file
    """
    if not gcov_file.exists():
        return tuple()

    uncovered = []

    try:
        with open(gcov_file, 'r') as f:
            for line in f:
                # Format: "    #####:  123:source code"
                # ##### means uncovered
                if line.strip().startswith('#####:'):
                    parts = line.split(':', 2)
                    if len(parts) >= 2:
                        line_num = parts[1].strip()
                        if line_num.isdigit():
                            uncovered.append(int(line_num))

    except Exception:
        pass

    return tuple(uncovered)

def analyze_coverage(build_dir: Path,
                     source_dir: Path) -> CoverageReport:
    """
    Analyze all coverage data

    WHAT: Generate comprehensive coverage report
    WHY:  Understand current coverage status
    HOW:  Process all .gcda files

    COMPLEXITY: O(n * m) where n = files, m = lines per file
    """
    gcda_files = find_gcda_files(build_dir)

    print(f"Found {len(gcda_files)} coverage data files")

    file_coverages = []
    total_lines_all = 0
    covered_lines_all = 0

    # Process each coverage file
    for gcda_file in gcda_files:
        coverage_data = run_gcov_on_file(gcda_file, source_dir)

        if coverage_data:
            total_lines_all += coverage_data['total_lines']
            covered_lines_all += coverage_data['covered_lines']

            file_coverage = FileCoverage(
                file_path=str(gcda_file),
                total_lines=coverage_data['total_lines'],
                covered_lines=coverage_data['covered_lines'],
                uncovered_lines=coverage_data['total_lines'] - coverage_data['covered_lines'],
                coverage_percent=coverage_data['coverage_percent'],
                uncovered_line_numbers=tuple()
            )

            file_coverages.append(file_coverage)

    overall_percent = (covered_lines_all / total_lines_all * 100.0) if total_lines_all > 0 else 0.0

    return CoverageReport(
        total_lines=total_lines_all,
        covered_lines=covered_lines_all,
        coverage_percent=overall_percent,
        file_coverages=tuple(file_coverages)
    )

def print_coverage_report(report: CoverageReport):
    """
    Print coverage report to stdout

    WHAT: Display coverage statistics
    WHY:  Show user current coverage status
    HOW:  Format and print report data
    """
    print("\n" + "="*60)
    print("NIMCP Code Coverage Report")
    print("="*60)
    print(f"\nOverall Coverage: {report.coverage_percent:.2f}%")
    print(f"Total Lines: {report.total_lines}")
    print(f"Covered Lines: {report.covered_lines}")
    print(f"Uncovered Lines: {report.total_lines - report.covered_lines}")
    print(f"\nGoal: 100% coverage")
    print(f"Gap: {100.0 - report.coverage_percent:.2f}%")

    # Show files with lowest coverage
    print("\n" + "-"*60)
    print("Files with Lowest Coverage (need attention):")
    print("-"*60)

    sorted_files = sorted(report.file_coverages,
                         key=lambda f: f.coverage_percent)

    for file_cov in sorted_files[:20]:  # Top 20 lowest
        print(f"  {file_cov.coverage_percent:5.1f}% - {Path(file_cov.file_path).name}")

def main():
    """
    Main coverage analysis

    WHAT: Analyze NIMCP coverage
    WHY:  Track progress to 100%
    HOW:  Process gcov data
    """
    nimcp_root = Path(__file__).parent.parent.parent
    build_dir = nimcp_root / "build"
    source_dir = nimcp_root / "src"

    print("Analyzing NIMCP code coverage...")
    print(f"Build directory: {build_dir}")
    print(f"Source directory: {source_dir}\n")

    report = analyze_coverage(build_dir, source_dir)
    print_coverage_report(report)

if __name__ == "__main__":
    main()
