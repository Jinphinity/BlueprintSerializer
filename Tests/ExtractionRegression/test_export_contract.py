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
        tease_fixture = TEST_DIR / "fixtures" / "schema_1_7_tease_contract.json"
        self.tease_document = json.loads(tease_fixture.read_text(encoding="utf-8"))

    def test_schema_1_7_fixture_passes(self) -> None:
        self.assertEqual(validate_export_contract(self.document), [])

    def test_schema_1_7_tease_fixture_passes(self) -> None:
        self.assertEqual(validate_export_contract(self.tease_document), [])

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

    def _ease_node(self, document: dict) -> dict:
        for graph in document["structuredGraphs"]:
            for node in graph["nodes"]:
                if node.get("nodeType") == "K2Node_EaseFunction":
                    return node
        self.fail("fixture has no K2Node_EaseFunction")

    def test_ease_function_requires_metadata(self) -> None:
        broken = copy.deepcopy(self.document)
        self._ease_node(broken)["nodeProperties"].pop("EaseFunctionName")
        issues = validate_export_contract(broken)
        self.assertTrue(any("EaseFunctionName is missing" in issue for issue in issues))

    def test_ease_function_requires_all_semantic_pins(self) -> None:
        broken = copy.deepcopy(self.document)
        node = self._ease_node(broken)
        node["pins"] = [pin for pin in node["pins"] if pin["name"] != "BlendExp"]
        issues = validate_export_contract(broken)
        self.assertTrue(any("exactly one BlendExp pin" in issue for issue in issues))

    def test_ease_function_requires_typed_enum_pin(self) -> None:
        broken = copy.deepcopy(self.document)
        function_pin = next(
            pin for pin in self._ease_node(broken)["pins"] if pin["name"] == "Function"
        )
        function_pin["objectPath"] = "/Script/Engine.NotTheEasingEnum"
        issues = validate_export_contract(broken)
        self.assertTrue(any("Function.objectPath" in issue for issue in issues))

    def test_ease_function_requires_value_or_connection(self) -> None:
        broken = copy.deepcopy(self.document)
        alpha_pin = next(
            pin for pin in self._ease_node(broken)["pins"] if pin["name"] == "Alpha"
        )
        alpha_pin.pop("connected")
        issues = validate_export_contract(broken)
        self.assertTrue(any("Alpha has neither" in issue for issue in issues))

    def test_tease_function_requires_a_value_or_connection(self) -> None:
        broken = copy.deepcopy(self.tease_document)
        a_pin = next(
            pin for pin in self._ease_node(broken)["pins"] if pin["name"] == "A"
        )
        a_pin.pop("connected")
        issues = validate_export_contract(broken)
        self.assertTrue(any("A has neither" in issue for issue in issues))

    def test_tease_function_rejects_wrong_value_type(self) -> None:
        broken = copy.deepcopy(self.tease_document)
        result_pin = next(
            pin for pin in self._ease_node(broken)["pins"] if pin["name"] == "Result"
        )
        result_pin["objectPath"] = "/Script/CoreUObject.Vector"
        issues = validate_export_contract(broken)
        self.assertTrue(
            any("Result.objectPath is not /Script/CoreUObject.Transform" in issue for issue in issues)
        )

    def test_scalar_ease_requires_coherent_precision(self) -> None:
        broken = copy.deepcopy(self.document)
        b_pin = next(
            pin for pin in self._ease_node(broken)["pins"] if pin["name"] == "B"
        )
        b_pin["subCategory"] = "float"
        issues = validate_export_contract(broken)
        self.assertTrue(any("subCategory is not coherent" in issue for issue in issues))

    def test_vector_and_rotator_ease_contracts_pass(self) -> None:
        for function_name, object_path in (
            ("VEase", "/Script/CoreUObject.Vector"),
            ("REase", "/Script/CoreUObject.Rotator"),
        ):
            with self.subTest(function_name=function_name):
                document = copy.deepcopy(self.tease_document)
                node = self._ease_node(document)
                node["nodeProperties"]["EaseFunctionName"] = function_name
                for pin in node["pins"]:
                    if pin["name"] in {"A", "B", "Result"}:
                        pin["objectPath"] = object_path
                        pin["objectType"] = object_path.rsplit(".", 1)[-1]
                self.assertEqual(validate_export_contract(document), [])

    def test_ease_function_cannot_remain_partial(self) -> None:
        broken = copy.deepcopy(self.document)
        broken["coverage"]["partiallySupportedNodeTypes"] = [
            "K2Node_EaseFunction"
        ]
        broken["compilerIRFallback"]["partiallySupportedNodeTypeCount"] = 1
        broken["compilerIRFallback"]["partiallySupportedNodeTypes"] = [
            "K2Node_EaseFunction"
        ]
        issues = validate_export_contract(broken)
        self.assertTrue(any("still classifies" in issue for issue in issues))


if __name__ == "__main__":
    unittest.main()
