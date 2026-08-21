from __future__ import annotations

import copy
import json
from pathlib import Path
import sys
import unittest


TEST_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(TEST_DIR))

from check_export_contract import validate_export_contract  # noqa: E402


class ExportContractTests(unittest.TestCase):
    def setUp(self) -> None:
        fixture = TEST_DIR / "fixtures" / "schema_1_7_contract.json"
        self.document = json.loads(fixture.read_text(encoding="utf-8"))

    def test_schema_1_7_fixture_passes(self) -> None:
        self.assertEqual(validate_export_contract(self.document), [])

    def test_missing_collapsed_body_in_flat_surface_fails(self) -> None:
        broken = copy.deepcopy(self.document)
        broken["graphNodes"].pop()
        issues = validate_export_contract(broken)
        self.assertTrue(any("nodeGuid multisets differ" in issue for issue in issues))

    def test_duplicate_flat_node_fails(self) -> None:
        broken = copy.deepcopy(self.document)
        broken["graphNodes"].append(broken["graphNodes"][0])
        issues = validate_export_contract(broken)
        self.assertTrue(any("nodeGuid multisets differ" in issue for issue in issues))

    def test_legacy_bytecode_hash_fails(self) -> None:
        broken = copy.deepcopy(self.document)
        broken["detailedFunctions"][0]["bytecodeHash"] = "0" * 32
        issues = validate_export_contract(broken)
        self.assertTrue(any("legacy bytecodeHash" in issue for issue in issues))

    def test_raw_diagnostic_difference_is_noncanonical(self) -> None:
        another_load = copy.deepcopy(self.document)
        another_load["detailedFunctions"][0]["rawInMemoryBytecodeDiagnostic"][
            "value"
        ] = "fedcba9876543210fedcba9876543210"
        another_load["compilerIRFallback"]["bytecodeBackedFunctions"][0][
            "rawInMemoryBytecodeDiagnostic"
        ]["value"] = "fedcba9876543210fedcba9876543210"
        self.assertEqual(validate_export_contract(another_load), [])


if __name__ == "__main__":
    unittest.main()
