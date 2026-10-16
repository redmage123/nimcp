#!/usr/bin/env python3
"""
Coverage Analysis Module for Code Surgeon

WHAT: Integrate lcov/gcov for comprehensive coverage analysis
WHY:  Track test effectiveness, identify untested code, guide test creation
HOW:  Execute lcov commands, parse coverage data, generate HTML reports

PATTERNS: Pure functions where possible, clear side-effect marking
"""

import subprocess
from pathlib import Path
from dataclasses import dataclass
from typing import Optional, Dict, Tuple, List
from datetime import datetime

#==============================================================================
# Data Structures
#==============================================================================

@dataclass(frozen=True)
class FileCoverage:
    """Coverage for a single source file"""
    file_path: str
    line_coverage_percent: float
    lines_total: int
    lines_covered: int
    lines_uncovered: int
    function_coverage_percent: float = 0.0
    branch_coverage_percent: float = 0.0

@dataclass(frozen=True)
class CoverageReport:
    """Complete coverage report"""
    overall_percent: float
    total_lines: int
    covered_lines: int
    uncovered_lines: int
    files: Tuple[FileCoverage, ...] = ()
    timestamp: str = ""
    html_report_path: Optional[str] = None

#==============================================================================
# lcov Integration Functions
#==============================================================================

def capture_coverage_data(build_dir: Path, output_file: Path) -> bool:
    """
    Capture coverage data using lcov

    WHAT: Run lcov to collect .gcda files into single info file
    WHY:  Aggregate coverage across all test runs
    HOW:  Execute: lcov --capture --directory . --output-file coverage.info

    SIDE EFFECT: Creates coverage.info file

    RETURNS: True if successful
    """
    # Guard: Check build directory exists
    if not build_dir.exists():
        print(f"❌ Build directory not found: {build_dir}")
        return False

    # Guard: Check lcov is installed
    try:
        subprocess.run(["lcov", "--version"],
                      capture_output=True,
                      check=True)
    except (subprocess.CalledProcessError, FileNotFoundError):
        print("❌ lcov not installed. Install: sudo apt-get install lcov")
        return False

    print(f"\n[COVERAGE] Capturing coverage data...")
    print(f"  Build dir: {build_dir}")
    print(f"  Output: {output_file}")

    try:
        # Capture coverage data
        result = subprocess.run(
            ["lcov",
             "--capture",
             "--directory", str(build_dir),
             "--output-file", str(output_file),
             "--ignore-errors", "mismatch",     # Ignore mismatched line numbers
             "--ignore-errors", "source",       # Ignore missing source files
             "--ignore-errors", "gcov",         # Ignore gcov errors (stamp mismatches)
             "--quiet"                          # Reduce verbosity
            ],
            capture_output=True,
            text=True,
            cwd=build_dir,
            timeout=60
        )

        if result.returncode != 0:
            print(f"⚠️  lcov capture had warnings:")
            print(result.stderr[:500])
            # Don't fail on warnings, lcov often succeeds with warnings

        # Check output file was created
        if output_file.exists() and output_file.stat().st_size > 0:
            print(f"✅ Coverage data captured ({output_file.stat().st_size} bytes)")
            return True
        else:
            print("❌ Coverage capture produced no data")
            return False

    except subprocess.TimeoutExpired:
        print("❌ Coverage capture timed out (60s)")
        return False
    except Exception as e:
        print(f"❌ Coverage capture failed: {e}")
        return False

def filter_coverage_data(info_file: Path,
                        output_file: Path,
                        src_dir: Path) -> bool:
    """
    Filter coverage to only project sources

    WHAT: Remove test code, system headers from coverage report
    WHY:  Focus on actual implementation code
    HOW:  lcov --extract with pattern matching

    SIDE EFFECT: Creates filtered coverage.info file
    """
    # Guard: Check input file exists
    if not info_file.exists():
        print(f"❌ Coverage info file not found: {info_file}")
        return False

    print(f"\n[COVERAGE] Filtering coverage data...")
    print(f"  Including: {src_dir}/*")

    try:
        # Extract only project sources (exclude tests, external libs)
        result = subprocess.run(
            ["lcov",
             "--extract", str(info_file),
             f"{src_dir}/*",
             "--output-file", str(output_file),
             "--ignore-errors", "mismatch",
             "--quiet"
            ],
            capture_output=True,
            text=True,
            timeout=30
        )

        if result.returncode == 0 and output_file.exists():
            print(f"✅ Coverage data filtered")
            return True
        else:
            print(f"⚠️  Filtering had warnings (continuing)")
            return output_file.exists()

    except Exception as e:
        print(f"❌ Coverage filtering failed: {e}")
        return False

def generate_html_report(info_file: Path,
                        output_dir: Path) -> bool:
    """
    Generate HTML coverage report

    WHAT: Create visual HTML report with genhtml
    WHY:  Human-readable coverage visualization
    HOW:  Execute genhtml coverage.info

    SIDE EFFECT: Creates HTML files in output_dir
    """
    # Guard: Check input file exists
    if not info_file.exists():
        print(f"❌ Coverage info file not found: {info_file}")
        return False

    print(f"\n[COVERAGE] Generating HTML report...")
    print(f"  Output: {output_dir}/index.html")

    # Create output directory
    output_dir.mkdir(parents=True, exist_ok=True)

    try:
        result = subprocess.run(
            ["genhtml",
             str(info_file),
             "--output-directory", str(output_dir),
             "--title", "NIMCP Code Coverage",
             "--legend",
             "--quiet",
             "--ignore-errors", "source"
            ],
            capture_output=True,
            text=True,
            timeout=60
        )

        index_file = output_dir / "index.html"
        if result.returncode == 0 and index_file.exists():
            print(f"✅ HTML report generated: {index_file}")
            return True
        else:
            print(f"⚠️  HTML generation had warnings")
            return index_file.exists()

    except Exception as e:
        print(f"❌ HTML generation failed: {e}")
        return False

def parse_coverage_summary(info_file: Path) -> Optional[CoverageReport]:
    """
    Parse coverage summary from lcov info file

    WHAT: Extract overall coverage statistics
    WHY:  Get numeric coverage data for analysis
    HOW:  Run lcov --summary

    PURE FUNCTION: No side effects, just parsing
    """
    # Guard: Check input file exists
    if not info_file.exists():
        return None

    try:
        result = subprocess.run(
            ["lcov",
             "--summary", str(info_file),
             "--quiet"
            ],
            capture_output=True,
            text=True,
            timeout=10
        )

        if result.returncode != 0:
            return None

        # Parse output
        # Example: "lines......: 56.3% (58474 of 103798 lines)"
        lines = result.stdout

        overall_percent = 0.0
        total_lines = 0
        covered_lines = 0

        for line in lines.split('\n'):
            if 'lines' in line.lower() and '%' in line:
                # Extract: "56.3% (58474 of 103798 lines)"
                parts = line.split(':')
                if len(parts) >= 2:
                    percent_part = parts[1].strip()
                    # Extract percentage
                    if '%' in percent_part:
                        percent_str = percent_part.split('%')[0].strip()
                        try:
                            overall_percent = float(percent_str)
                        except ValueError:
                            pass

                    # Extract counts: "(58474 of 103798 lines)"
                    if '(' in percent_part and 'of' in percent_part:
                        count_part = percent_part.split('(')[1].split(')')[0]
                        numbers = count_part.split('of')
                        if len(numbers) == 2:
                            try:
                                covered_lines = int(numbers[0].strip().split()[0])
                                total_lines = int(numbers[1].strip().split()[0])
                            except ValueError:
                                pass

        return CoverageReport(
            overall_percent=overall_percent,
            total_lines=total_lines,
            covered_lines=covered_lines,
            uncovered_lines=total_lines - covered_lines,
            timestamp=datetime.now().isoformat()
        )

    except Exception as e:
        print(f"⚠️  Failed to parse coverage: {e}")
        return None

def get_low_coverage_files(info_file: Path,
                          threshold: float = 50.0) -> List[str]:
    """
    Find files with coverage below threshold

    WHAT: Identify files needing more tests
    WHY:  Guide test creation efforts
    HOW:  Parse lcov info file for per-file coverage

    PURE FUNCTION: Just parsing and filtering
    """
    # Guard: Check input file exists
    if not info_file.exists():
        return []

    low_coverage_files = []

    try:
        # Parse info file manually (lcov doesn't have per-file summary command)
        with open(info_file, 'r') as f:
            current_file = None
            lines_found = 0
            lines_hit = 0

            for line in f:
                line = line.strip()

                if line.startswith('SF:'):
                    # New file
                    if current_file and lines_found > 0:
                        percent = (lines_hit / lines_found) * 100
                        if percent < threshold:
                            low_coverage_files.append(
                                f"{percent:5.1f}% - {Path(current_file).name}"
                            )

                    # Reset for new file
                    current_file = line[3:]
                    lines_found = 0
                    lines_hit = 0

                elif line.startswith('DA:'):
                    # Line data: DA:line_num,hit_count
                    lines_found += 1
                    parts = line[3:].split(',')
                    if len(parts) >= 2:
                        hit_count = int(parts[1])
                        if hit_count > 0:
                            lines_hit += 1

                elif line == 'end_of_record':
                    # End of file section
                    if current_file and lines_found > 0:
                        percent = (lines_hit / lines_found) * 100
                        if percent < threshold:
                            low_coverage_files.append(
                                f"{percent:5.1f}% - {Path(current_file).name}"
                            )
                    current_file = None
                    lines_found = 0
                    lines_hit = 0

        return sorted(low_coverage_files)

    except Exception as e:
        print(f"⚠️  Failed to parse file coverage: {e}")
        return []

#==============================================================================
# High-Level Coverage Workflow
#==============================================================================

def run_fallback_coverage_analysis(nimcp_root: Path) -> Optional[CoverageReport]:
    """
    Fallback coverage analysis using analyze_coverage.py

    WHAT: Use existing Python script when lcov fails
    WHY:  Still get coverage info despite lcov issues
    HOW:  Run analyze_coverage.py and parse output
    """
    print("[COVERAGE] Using fallback analyze_coverage.py...")

    script_path = nimcp_root / "tools" / "scripts" / "analyze_coverage.py"

    if not script_path.exists():
        print(f"❌ Fallback script not found: {script_path}")
        return None

    try:
        result = subprocess.run(
            ["python3", str(script_path)],
            capture_output=True,
            text=True,
            cwd=nimcp_root,
            timeout=30
        )

        if result.returncode != 0:
            print(f"❌ Fallback script failed with code {result.returncode}")
            return None

        # Parse output
        lines = result.stdout.split('\n')
        overall_percent = 0.0
        total_lines = 0
        covered_lines = 0

        for line in lines:
            if "Overall Coverage:" in line:
                parts = line.split(':')
                if len(parts) >= 2:
                    percent_str = parts[1].strip().rstrip('%')
                    overall_percent = float(percent_str)
            elif "Total Lines:" in line:
                parts = line.split(':')
                if len(parts) >= 2:
                    total_lines = int(parts[1].strip().replace(',', ''))
            elif "Covered Lines:" in line:
                parts = line.split(':')
                if len(parts) >= 2:
                    covered_lines = int(parts[1].strip().replace(',', ''))

        if total_lines > 0:
            print(f"✅ Fallback coverage analysis complete")

            report = CoverageReport(
                overall_percent=overall_percent,
                total_lines=total_lines,
                covered_lines=covered_lines,
                uncovered_lines=total_lines - covered_lines,
                timestamp=datetime.now().isoformat()
            )

            # Print summary immediately
            print("\n" + "=" * 60)
            print("COVERAGE SUMMARY (Fallback)")
            print("=" * 60)
            print(f"  Overall Coverage: {report.overall_percent:.2f}%")
            print(f"  Total Lines: {report.total_lines:,}")
            print(f"  Covered Lines: {report.covered_lines:,}")
            print(f"  Uncovered Lines: {report.uncovered_lines:,}")
            print(f"  Gap to 100%: {100 - report.overall_percent:.2f}%")
            print("=" * 60)

            return report

    except Exception as e:
        print(f"⚠️  Fallback coverage failed: {e}")

    return None

def run_full_coverage_analysis(nimcp_root: Path) -> Optional[CoverageReport]:
    """
    Complete coverage analysis workflow

    WHAT: Run all coverage steps: capture → filter → HTML → parse
    WHY:  One-stop function for coverage reporting
    HOW:  Chain all coverage functions, fallback to analyze_coverage.py

    SIDE EFFECTS: Creates coverage.info and HTML report

    WORKFLOW:
    1. Try lcov-based coverage (preferred)
    2. If lcov fails, fallback to analyze_coverage.py
    3. Generate HTML report if possible
    4. Parse summary statistics
    5. Identify low-coverage files
    """
    build_dir = nimcp_root / "build"
    src_dir = nimcp_root / "src"

    # File paths
    raw_coverage = build_dir / "coverage_raw.info"
    filtered_coverage = build_dir / "coverage.info"
    html_dir = build_dir / "coverage_html"

    # Step 1: Capture coverage
    if not capture_coverage_data(build_dir, raw_coverage):
        print("\n⚠️  lcov capture failed, using fallback...")
        return run_fallback_coverage_analysis(nimcp_root)

    # Step 2: Filter to project sources
    if not filter_coverage_data(raw_coverage, filtered_coverage, src_dir):
        print("\n⚠️  lcov filter failed, using fallback...")
        return run_fallback_coverage_analysis(nimcp_root)

    # Step 3: Generate HTML report
    html_success = generate_html_report(filtered_coverage, html_dir)

    # Step 4: Parse summary
    report = parse_coverage_summary(filtered_coverage)

    if report:
        # Add HTML path if generated
        if html_success:
            report = CoverageReport(
                overall_percent=report.overall_percent,
                total_lines=report.total_lines,
                covered_lines=report.covered_lines,
                uncovered_lines=report.uncovered_lines,
                timestamp=report.timestamp,
                html_report_path=str(html_dir / "index.html")
            )

        # Step 5: Print summary
        print("\n" + "=" * 60)
        print("COVERAGE SUMMARY")
        print("=" * 60)
        print(f"  Overall Coverage: {report.overall_percent:.2f}%")
        print(f"  Total Lines: {report.total_lines:,}")
        print(f"  Covered Lines: {report.covered_lines:,}")
        print(f"  Uncovered Lines: {report.uncovered_lines:,}")
        print(f"  Gap to 100%: {100 - report.overall_percent:.2f}%")

        if report.html_report_path:
            print(f"\n  📊 HTML Report: {report.html_report_path}")
            print(f"     Open with: xdg-open {report.html_report_path}")

        # Show files needing attention
        low_coverage = get_low_coverage_files(filtered_coverage, threshold=50.0)
        if low_coverage:
            print(f"\n  Files with <50% coverage (need tests):")
            for file_info in low_coverage[:10]:  # Show top 10
                print(f"    {file_info}")
            if len(low_coverage) > 10:
                print(f"    ... and {len(low_coverage) - 10} more")

        print("=" * 60)

    return report
