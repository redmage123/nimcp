#!/usr/bin/env python3
"""
Test Migration Script - Move tests to new structure

WHAT: Migrate tests from src/tests/ to test/
WHY:  Separate tests from source code
HOW:  Categorize and move test files

PATTERNS: Functional, pure functions where possible
"""

import shutil
from pathlib import Path
from typing import Tuple, Dict
import re

#==============================================================================
# Pure Functions
#==============================================================================

def categorize_test_file(file_path: Path) -> str:
    """
    Determine test category

    WHAT: Classify test by name/content
    WHY:  Place in correct directory
    HOW:  Pattern matching on filename

    RETURNS: 'unit', 'integration', 'e2e', 'regression', or 'fuzz'
    """
    name = file_path.stem.lower()

    # Fuzzing tests
    if 'fuzz' in name:
        return 'fuzz'

    # Regression tests (bug-specific)
    if 'regression' in name or 'bug' in name:
        return 'regression'

    # E2E tests (full system)
    if 'e2e' in name or 'full' in name or 'system' in name:
        return 'e2e'

    # Integration tests (cross-module)
    if 'integration' in name or 'combined' in name:
        return 'integration'

    # Default to unit
    return 'unit'

def find_test_files(src_dir: Path) -> Tuple[Path, ...]:
    """
    Find all test files

    WHAT: Discover test files in src/tests/
    WHY:  Get files to migrate
    HOW:  Recursive glob

    COMPLEXITY: O(n) where n = number of files
    """
    # Guard clause
    if not src_dir.exists():
        return tuple()

    test_files = []

    # Find C++ test files
    for pattern in ['test_*.cpp', '*_test.cpp', '*_tests.cpp']:
        test_files.extend(src_dir.glob(f'**/{pattern}'))

    return tuple(sorted(test_files))

def create_migration_plan(test_files: Tuple[Path, ...],
                          dest_root: Path) -> Dict[Path, Path]:
    """
    Create file migration mapping

    WHAT: Map source files to destinations
    WHY:  Plan before executing
    HOW:  Categorize each file

    RETURNS: Dict mapping source → destination
    """
    plan = {}

    for test_file in test_files:
        category = categorize_test_file(test_file)
        dest_dir = dest_root / category
        dest_file = dest_dir / test_file.name

        plan[test_file] = dest_file

    return plan

#==============================================================================
# Side Effects (File Operations)
#==============================================================================

def execute_migration(migration_plan: Dict[Path, Path],
                      dry_run: bool = True) -> Tuple[bool, ...]:
    """
    Execute file migrations

    WHAT: Move files to new locations
    WHY:  Implement the migration plan
    HOW:  shutil.move with error handling

    NOTE: Side effect - moves files
    """
    results = []

    for src, dest in migration_plan.items():
        try:
            # Guard: skip if already exists
            if dest.exists():
                print(f"⚠️  Skip (exists): {dest}")
                results.append(False)
                continue

            if dry_run:
                print(f"Would move: {src} → {dest}")
                results.append(True)
            else:
                # Create destination directory
                dest.parent.mkdir(parents=True, exist_ok=True)

                # Move file
                shutil.move(str(src), str(dest))
                print(f"✅ Moved: {src.name} → {dest.parent.name}/")
                results.append(True)

        except Exception as e:
            print(f"❌ Error moving {src}: {e}")
            results.append(False)

    return tuple(results)

def main():
    """
    Migration main

    WHAT: Migrate test files
    WHY:  Execute test structure refactor
    HOW:  Find, plan, execute
    """
    print("Test Migration Script")
    print("=" * 60)

    nimcp_root = Path(__file__).parent.parent.parent
    src_tests = nimcp_root / "src" / "tests"
    dest_root = nimcp_root / "test"

    # Guard clauses
    if not src_tests.exists():
        print(f"❌ Source directory not found: {src_tests}")
        return 1

    if not dest_root.exists():
        print(f"❌ Destination directory not found: {dest_root}")
        return 1

    # Find files
    print(f"\n[DISCOVERY] Scanning {src_tests}...")
    test_files = find_test_files(src_tests)
    print(f"Found {len(test_files)} test files")

    # Create plan
    print("\n[PLANNING] Creating migration plan...")
    plan = create_migration_plan(test_files, dest_root)

    # Show plan
    print("\nMigration Plan:")
    for src, dest in plan.items():
        print(f"  {src.name} → {dest.parent.name}/")

    # Execute (dry run first)
    print("\n[DRY RUN]")
    execute_migration(plan, dry_run=True)

    # Ask for confirmation
    response = input("\nExecute migration? (yes/no): ")

    if response.lower() == 'yes':
        print("\n[EXECUTING]")
        results = execute_migration(plan, dry_run=False)
        success_count = sum(results)
        print(f"\n✅ Migrated {success_count}/{len(plan)} files")
        return 0
    else:
        print("\n❌ Migration cancelled")
        return 1

if __name__ == "__main__":
    import sys
    sys.exit(main())
