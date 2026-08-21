from __future__ import annotations

import json
import importlib.util
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


TEST_ROOT = Path(__file__).resolve().parent
PLUGIN_ROOT = TEST_ROOT.parents[1]
VALIDATOR_PATH = PLUGIN_ROOT / "Scripts" / "validate_specs.py"
FIXTURES = TEST_ROOT / "fixtures"
SPECS = FIXTURES / "specs"
EXPORTS = FIXTURES / "exports"

module_spec = importlib.util.spec_from_file_location("validate_specs", VALIDATOR_PATH)
validate_specs = importlib.util.module_from_spec(module_spec)
assert module_spec.loader is not None
module_spec.loader.exec_module(validate_specs)


class ValidateSpecsV2RegressionTests(unittest.TestCase):
    def test_ir_notes_text_is_populated(self) -> None:
        parsed = validate_specs.parse_spec((SPECS / "BP_TableLinks.md").read_text(encoding="utf-8"))
        self.assertTrue(parsed["has_ir_notes"])
        self.assertIn("K2Node_UnsupportedFixture", parsed["ir_notes_text"])
        self.assertIn("K2Node_PartialFixture", parsed["ir_notes_text"])

    def test_pipe_tables_keep_code_span_and_escaped_pipes(self) -> None:
        parsed = validate_specs.parse_spec((SPECS / "BP_TableLinks.md").read_text(encoding="utf-8"))
        self.assertEqual(parsed["variable_names"], {"Mode|State", "Pipe|Name"})
        self.assertEqual(parsed["function_names"], {"Resolve|State"})

    def test_exact_metadata_pair_avoids_abstract_prefix_collision(self) -> None:
        export_index, errors = validate_specs.index_json_exports(EXPORTS)
        self.assertEqual(errors, [])
        match = validate_specs.find_json_for_spec(
            "BP_Inventory_Container", EXPORTS, export_index
        )
        self.assertIsNotNone(match)
        self.assertEqual(
            validate_specs.load_ir_json(match)["blueprintName"],
            "BP_Inventory_Container",
        )
        self.assertNotIn("Abstract", match.name)

    def test_prefix_only_candidate_is_not_accepted(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            export_dir = Path(directory)
            source = EXPORTS / (
                "BP_SLZR_Blueprint_BP_Inventory_Container_Abstract_"
                "22222222_20260821_000001.json"
            )
            shutil.copy2(source, export_dir / source.name)
            self.assertIsNone(
                validate_specs.find_json_for_spec(
                    "BP_Inventory_Container", export_dir
                )
            )

    def test_duplicate_exact_candidates_are_rejected_as_ambiguous(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            export_dir = Path(directory)
            source = EXPORTS / (
                "BP_SLZR_Blueprint_BP_Inventory_Container_"
                "11111111_20260821_000000.json"
            )
            shutil.copy2(source, export_dir / source.name)
            shutil.copy2(
                source,
                export_dir
                / "BP_SLZR_Blueprint_BP_Inventory_Container_99999999_20260821_000003.json",
            )
            with self.assertRaises(validate_specs.JsonPairingError):
                validate_specs.find_json_for_spec(
                    "BP_Inventory_Container", export_dir
                )

    def test_validate_one_rejects_mismatched_metadata(self) -> None:
        with self.assertRaises(validate_specs.JsonPairingError):
            validate_specs.validate_one(
                SPECS / "BP_Inventory_Container.md",
                EXPORTS
                / (
                    "BP_SLZR_Blueprint_BP_Inventory_Container_Abstract_"
                    "22222222_20260821_000001.json"
                ),
            )

    def test_unsupported_and_partial_node_types_are_exactly_disclosed(self) -> None:
        result = validate_specs.validate_one(
            SPECS / "BP_TableLinks.md",
            EXPORTS / "BP_SLZR_Blueprint_BP_TableLinks_33333333_20260821_000002.json",
        )
        support_issues = [
            issue for issue in result["issues"]
            if issue["field"] in {"unsupported_node_type", "partial_node_type"}
        ]
        self.assertEqual(support_issues, [])

        text = (SPECS / "BP_TableLinks.md").read_text(encoding="utf-8")
        parsed = validate_specs.parse_spec(
            text.replace("K2Node_PartialFixture", "K2Node_OmittedFixture")
        )
        ir = validate_specs.load_ir_json(
            EXPORTS / "BP_SLZR_Blueprint_BP_TableLinks_33333333_20260821_000002.json"
        )
        issues = validate_specs.validate_structural(parsed, ir)
        self.assertTrue(any(issue["field"] == "partial_node_type" for issue in issues))

    def test_message_bearing_compiler_diagnostic_requires_guid_visibility(self) -> None:
        ir = validate_specs.load_ir_json(
            EXPORTS / "BP_SLZR_Blueprint_BP_TableLinks_33333333_20260821_000002.json"
        )
        diagnostics = validate_specs.collect_compiler_diagnostics(ir)
        self.assertEqual(len(diagnostics), 1)
        self.assertEqual(validate_specs.count_nonzero_error_type_markers(ir), 1)

        text = (SPECS / "BP_TableLinks.md").read_text(encoding="utf-8")
        parsed = validate_specs.parse_spec(
            text.replace(
                "DIAGNOSTIC000000000000000000000001",
                "OMITTED00000000000000000000000001",
            )
        )
        issues = validate_specs.validate_structural(parsed, ir)
        self.assertTrue(any(issue["field"] == "compiler_diagnostic" for issue in issues))

    def test_relative_links_decode_spaces_and_ignore_code_or_external_urls(self) -> None:
        spec_path = SPECS / "BP_TableLinks.md"
        issues = validate_specs.validate_relative_links(
            spec_path, spec_path.read_text(encoding="utf-8")
        )
        self.assertEqual(issues, [])

    def test_missing_relative_link_is_visible(self) -> None:
        issues = validate_specs.validate_relative_links(
            SPECS / "BP_TableLinks.md", "[Missing](linked/not-there.md)"
        )
        self.assertEqual(len(issues), 1)
        self.assertEqual(issues[0]["field"], "relative_link")

    def test_branch_coverage_uses_source_markdown(self) -> None:
        result = validate_specs.validate_one(
            SPECS / "BP_TableLinks.md",
            EXPORTS / "BP_SLZR_Blueprint_BP_TableLinks_33333333_20260821_000002.json",
        )
        self.assertFalse(
            any(issue["field"] == "branch_coverage" for issue in result["issues"])
        )

    def test_utf16_json_loading(self) -> None:
        payload = {"blueprintName": "BP_UTF16"}
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "fixture.json"
            path.write_bytes(json.dumps(payload).encode("utf-16"))
            self.assertEqual(validate_specs.load_ir_json(path), payload)

    def test_cli_report_distinguishes_static_pass_from_unexecuted_runtime(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            report = Path(directory) / "report.json"
            fixes = Path(directory) / "fixes.md"
            process = subprocess.run(
                [
                    sys.executable,
                    str(VALIDATOR_PATH),
                    str(SPECS),
                    str(EXPORTS),
                    "--report",
                    str(report),
                    "--fix-list",
                    str(fixes),
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(process.returncode, 0, process.stdout + process.stderr)
            payload = json.loads(report.read_text(encoding="utf-8"))
            self.assertEqual(payload["result"], "STRUCTURAL_PASS")
            self.assertTrue(payload["structural_pass"])
            self.assertFalse(payload["scope"]["blueprint_compile_validated"])
            self.assertFalse(payload["scope"]["editor_roundtrip_validated"])
            self.assertFalse(payload["scope"]["pie_runtime_validated"])
            self.assertIn("does not prove Blueprint compile cleanliness", fixes.read_text(encoding="utf-8"))

    def test_cli_fails_when_a_spec_has_no_exact_pair(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            temp_root = Path(directory)
            spec_dir = temp_root / "specs"
            spec_dir.mkdir()
            (spec_dir / "BP_Missing.md").write_text(
                "# BP_Missing\n\n## Identity\n\n**Path:** x\n\n**Parent:** Actor\n\n"
                "## Purpose\n\nMissing exact pair.\n\n## IR Notes\n\nNo support gaps.\n",
                encoding="utf-8",
            )
            report = temp_root / "report.json"
            fixes = temp_root / "fixes.md"
            process = subprocess.run(
                [
                    sys.executable,
                    str(VALIDATOR_PATH),
                    str(spec_dir),
                    str(EXPORTS),
                    "--report",
                    str(report),
                    "--fix-list",
                    str(fixes),
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(process.returncode, 1)
            payload = json.loads(report.read_text(encoding="utf-8"))
            self.assertEqual(payload["result"], "STRUCTURAL_FAIL")
            self.assertEqual(payload["pairing_errors"], 1)


if __name__ == "__main__":
    unittest.main()
