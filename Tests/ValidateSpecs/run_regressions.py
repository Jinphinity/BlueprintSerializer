#!/usr/bin/env python3
"""Run validate_specs v2 regressions and emit versioned release receipts."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import io
import json
import sys
import unittest
from pathlib import Path


TEST_ROOT = Path(__file__).resolve().parent
PLUGIN_ROOT = TEST_ROOT.parents[1]
DEFAULT_REPORT = TEST_ROOT / "receipts" / "validate_specs_v2_regression_receipt.json"
DEFAULT_FAILURES = TEST_ROOT / "receipts" / "validate_specs_v2_regression_failures.md"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def iter_cases(suite: unittest.TestSuite):
    for item in suite:
        if isinstance(item, unittest.TestSuite):
            yield from iter_cases(item)
        else:
            yield item


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--report", type=Path, default=DEFAULT_REPORT)
    parser.add_argument("--failures", type=Path, default=DEFAULT_FAILURES)
    args = parser.parse_args()

    suite = unittest.defaultTestLoader.discover(
        str(TEST_ROOT), pattern="test_*.py", top_level_dir=str(TEST_ROOT)
    )
    case_ids = sorted(case.id() for case in iter_cases(suite))
    stream = io.StringIO()
    result = unittest.TextTestRunner(stream=stream, verbosity=2).run(suite)
    failure_count = len(result.failures) + len(result.errors)
    passed = result.testsRun - failure_count - len(result.skipped)

    artifact_paths = [
        PLUGIN_ROOT / "Scripts" / "validate_specs.py",
        TEST_ROOT / "test_validate_specs.py",
        TEST_ROOT / "run_regressions.py",
        *sorted(path for path in (TEST_ROOT / "fixtures").rglob("*") if path.is_file()),
    ]
    artifacts = [
        {
            "path": path.relative_to(PLUGIN_ROOT).as_posix(),
            "bytes": path.stat().st_size,
            "sha256": sha256(path),
        }
        for path in artifact_paths
    ]
    report = {
        "schema": "blueprintserializer-validate-specs-regression-receipt-v2",
        "validatorVersion": "2.0.0",
        "generatedAtUtc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "scope": {
            "staticValidatorRegression": True,
            "blueprintSerializerCppChanged": False,
            "unrealLaunched": False,
            "blueprintCompileValidated": False,
            "editorRoundtripValidated": False,
            "pieRuntimeValidated": False,
            "multiplayerValidated": False,
        },
        "summary": {
            "tests": result.testsRun,
            "passed": passed,
            "failed": failure_count,
            "skipped": len(result.skipped),
            "result": "PASS" if result.wasSuccessful() else "FAIL",
        },
        "cases": case_ids,
        "failures": [
            {"test": case.id(), "traceback": traceback}
            for case, traceback in [*result.failures, *result.errors]
        ],
        "artifacts": artifacts,
    }

    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    if result.wasSuccessful():
        failure_lines = [
            "# validate_specs v2 regression failures",
            "",
            "No failures. All configured validator regressions passed.",
            "",
            "This receipt covers the static validator only. It does not claim "
            "Blueprint compile cleanliness, editor roundtrip, PIE/runtime behavior, "
            "multiplayer correctness, donor repair, or C++ implementation.",
            "",
        ]
    else:
        failure_lines = [
            "# validate_specs v2 regression failures",
            "",
            f"`{failure_count}` regression case(s) failed:",
            "",
        ]
        for case, traceback in [*result.failures, *result.errors]:
            failure_lines.extend([
                f"## `{case.id()}`",
                "",
                "```text",
                traceback.rstrip(),
                "```",
                "",
            ])
    args.failures.write_text(
        "\n".join(failure_lines), encoding="utf-8", newline="\n"
    )
    sys.stdout.write(stream.getvalue())
    print(args.report)
    print(args.failures)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main())
