#!/usr/bin/env python3
"""Validate schema-1.7 graph parity and noncanonical bytecode diagnostics."""

from __future__ import annotations

import argparse
from collections import Counter
import json
from pathlib import Path
import re
from typing import Any


MD5_RE = re.compile(r"^[0-9a-fA-F]{32}$")
EASE_FUNCTION_NODE_TYPE = "K2Node_EaseFunction"
EASE_REQUIRED_PINS = {
    "Function": ("Input", "byte"),
    "Alpha": ("Input", "real"),
    "A": ("Input", None),
    "B": ("Input", None),
    "Result": ("Output", None),
    "ShortestPath": ("Input", "bool"),
    "BlendExp": ("Input", "real"),
    "Steps": ("Input", "int"),
}
EASE_VALUE_CONTRACTS = {
    "Ease": {"category": "real", "subCategories": {"float", "double"}},
    "VEase": {"category": "struct", "objectPath": "/Script/CoreUObject.Vector"},
    "REase": {"category": "struct", "objectPath": "/Script/CoreUObject.Rotator"},
    "TEase": {"category": "struct", "objectPath": "/Script/CoreUObject.Transform"},
}


def _flat_node_guids(document: dict[str, Any], issues: list[str]) -> list[str]:
    guids: list[str] = []
    for index, encoded_node in enumerate(document.get("graphNodes") or []):
        if not isinstance(encoded_node, str):
            issues.append(f"graphNodes[{index}] is not a JSON string")
            continue
        try:
            node = json.loads(encoded_node)
        except json.JSONDecodeError as exc:
            issues.append(f"graphNodes[{index}] is invalid JSON: {exc}")
            continue
        guid = node.get("nodeGuid") if isinstance(node, dict) else None
        if not isinstance(guid, str) or not guid:
            issues.append(f"graphNodes[{index}] has no nodeGuid")
            continue
        guids.append(guid)
    return guids


def _structured_node_guids(document: dict[str, Any], issues: list[str]) -> list[str]:
    guids: list[str] = []
    for graph_index, graph in enumerate(document.get("structuredGraphs") or []):
        if not isinstance(graph, dict):
            issues.append(f"structuredGraphs[{graph_index}] is not an object")
            continue
        for node_index, node in enumerate(graph.get("nodes") or []):
            guid = node.get("nodeGuid") if isinstance(node, dict) else None
            if not isinstance(guid, str) or not guid:
                issues.append(
                    f"structuredGraphs[{graph_index}].nodes[{node_index}] has no nodeGuid"
                )
                continue
            guids.append(guid)
    return guids


def _validate_raw_diagnostic(value: Any, location: str, issues: list[str]) -> None:
    if not isinstance(value, dict):
        issues.append(f"{location} is not an object")
        return
    if value.get("algorithm") != "MD5":
        issues.append(f"{location}.algorithm is not MD5")
    if value.get("source") != "UFunction::Script in-memory bytes":
        issues.append(f"{location}.source is not the raw in-memory Script buffer")
    if value.get("stabilityScope") != "single_loaded_function_instance":
        issues.append(f"{location}.stabilityScope is not fail-closed")
    if value.get("canonical") is not False:
        issues.append(f"{location}.canonical must be false")
    if value.get("semanticEquivalenceProof") is not False:
        issues.append(f"{location}.semanticEquivalenceProof must be false")
    diagnostic_value = value.get("value")
    if not isinstance(diagnostic_value, str) or not MD5_RE.fullmatch(diagnostic_value):
        issues.append(f"{location}.value is not a 32-hex MD5 diagnostic")


def _validate_ease_function_contract(
    node: dict[str, Any], location: str, issues: list[str]
) -> None:
    properties = node.get("nodeProperties")
    ease_function_name: str | None = None
    if not isinstance(properties, dict):
        issues.append(f"{location}.nodeProperties is not an object")
    elif not isinstance(properties.get("EaseFunctionName"), str) or not properties.get(
        "EaseFunctionName"
    ):
        issues.append(f"{location}.nodeProperties.EaseFunctionName is missing")
    else:
        ease_function_name = properties["EaseFunctionName"]

    value_contract = EASE_VALUE_CONTRACTS.get(ease_function_name or "")
    if ease_function_name and value_contract is None:
        issues.append(
            f"{location}.nodeProperties.EaseFunctionName {ease_function_name!r} "
            "has no supported value-pin contract"
        )

    pins = node.get("pins")
    if not isinstance(pins, list):
        issues.append(f"{location}.pins is not an array")
        return

    pins_by_name: dict[str, list[dict[str, Any]]] = {}
    for pin_index, pin in enumerate(pins):
        if not isinstance(pin, dict):
            issues.append(f"{location}.pins[{pin_index}] is not an object")
            continue
        name = pin.get("name")
        if isinstance(name, str):
            pins_by_name.setdefault(name, []).append(pin)

    coherent_real_sub_category: str | None = None
    for name, (direction, category) in EASE_REQUIRED_PINS.items():
        matches = pins_by_name.get(name, [])
        if len(matches) != 1:
            issues.append(
                f"{location} requires exactly one {name} pin; found {len(matches)}"
            )
            continue
        pin = matches[0]
        if pin.get("direction") != direction:
            issues.append(f"{location}.{name}.direction is not {direction}")
        if category is not None and pin.get("category") != category:
            issues.append(f"{location}.{name}.category is not {category}")
        elif category is None and value_contract is not None:
            expected_category = value_contract["category"]
            if pin.get("category") != expected_category:
                issues.append(
                    f"{location}.{name}.category is not {expected_category} "
                    f"for {ease_function_name}"
                )
            elif ease_function_name == "Ease":
                sub_category = pin.get("subCategory")
                allowed_sub_categories = value_contract["subCategories"]
                if sub_category not in allowed_sub_categories:
                    issues.append(
                        f"{location}.{name}.subCategory is not float or double for Ease"
                    )
                elif coherent_real_sub_category is None:
                    coherent_real_sub_category = sub_category
                elif sub_category != coherent_real_sub_category:
                    issues.append(
                        f"{location}.{name}.subCategory is not coherent with "
                        f"{coherent_real_sub_category} Ease value pins"
                    )
            elif pin.get("objectPath") != value_contract["objectPath"]:
                issues.append(
                    f"{location}.{name}.objectPath is not "
                    f"{value_contract['objectPath']} for {ease_function_name}"
                )

        if name == "Result":
            if pin.get("is_out") is not True:
                issues.append(f"{location}.Result.is_out must be true")
            continue

        if name == "Function":
            if pin.get("objectPath") != "/Script/Engine.EEasingFunc":
                issues.append(
                    f"{location}.Function.objectPath is not /Script/Engine.EEasingFunc"
                )

        if pin.get("connected") is not True and not pin.get("defaultValue"):
            issues.append(f"{location}.{name} has neither a connection nor a default")


def _validate_ease_function_support(
    document: dict[str, Any], ease_node_count: int, issues: list[str]
) -> None:
    if ease_node_count == 0:
        return

    coverage = document.get("coverage")
    if not isinstance(coverage, dict):
        issues.append("coverage is missing for K2Node_EaseFunction support")
    else:
        partial = coverage.get("partiallySupportedNodeTypes")
        if not isinstance(partial, list):
            issues.append("coverage.partiallySupportedNodeTypes is not an array")
        elif EASE_FUNCTION_NODE_TYPE in partial:
            issues.append("coverage still classifies K2Node_EaseFunction as partial")

    fallback = document.get("compilerIRFallback")
    if not isinstance(fallback, dict):
        issues.append("compilerIRFallback is missing for K2Node_EaseFunction support")
        return
    partial = fallback.get("partiallySupportedNodeTypes")
    if not isinstance(partial, list):
        issues.append(
            "compilerIRFallback.partiallySupportedNodeTypes is not an array"
        )
    elif EASE_FUNCTION_NODE_TYPE in partial:
        issues.append(
            "compilerIRFallback still classifies K2Node_EaseFunction as partial"
        )
    partial_count = fallback.get("partiallySupportedNodeTypeCount")
    if isinstance(partial, list) and partial_count != len(partial):
        issues.append(
            "compilerIRFallback.partiallySupportedNodeTypeCount does not match its array"
        )


def validate_export_contract(document: dict[str, Any]) -> list[str]:
    issues: list[str] = []
    if document.get("schemaVersion") != "1.7":
        issues.append("schemaVersion is not 1.7")

    flat_guids = _flat_node_guids(document, issues)
    structured_guids = _structured_node_guids(document, issues)
    if Counter(flat_guids) != Counter(structured_guids):
        flat = Counter(flat_guids)
        structured = Counter(structured_guids)
        missing = sorted((structured - flat).elements())
        extra = sorted((flat - structured).elements())
        issues.append(
            "graphNodes/structuredGraphs nodeGuid multisets differ: "
            f"missing_flat={missing[:12]} extra_flat={extra[:12]}"
        )

    if document.get("totalNodeCount") != len(structured_guids):
        issues.append(
            "totalNodeCount does not equal the structured node cardinality: "
            f"reported={document.get('totalNodeCount')} actual={len(structured_guids)}"
        )

    ease_node_count = 0
    for graph_index, graph in enumerate(document.get("structuredGraphs") or []):
        if not isinstance(graph, dict):
            continue
        for node_index, node in enumerate(graph.get("nodes") or []):
            if not isinstance(node, dict) or node.get("nodeType") != EASE_FUNCTION_NODE_TYPE:
                continue
            ease_node_count += 1
            _validate_ease_function_contract(
                node,
                f"structuredGraphs[{graph_index}].nodes[{node_index}]",
                issues,
            )
    _validate_ease_function_support(document, ease_node_count, issues)

    for index, function in enumerate(document.get("detailedFunctions") or []):
        if not isinstance(function, dict):
            continue
        if "bytecodeHash" in function:
            issues.append(f"detailedFunctions[{index}] still emits legacy bytecodeHash")
        diagnostic = function.get("rawInMemoryBytecodeDiagnostic")
        if diagnostic is not None:
            _validate_raw_diagnostic(
                diagnostic,
                f"detailedFunctions[{index}].rawInMemoryBytecodeDiagnostic",
                issues,
            )

    fallback = document.get("compilerIRFallback") or {}
    for index, function in enumerate(fallback.get("bytecodeBackedFunctions") or []):
        if not isinstance(function, dict):
            continue
        if "bytecodeHash" in function:
            issues.append(
                f"compilerIRFallback.bytecodeBackedFunctions[{index}] still emits "
                "legacy bytecodeHash"
            )
        diagnostic = function.get("rawInMemoryBytecodeDiagnostic")
        if diagnostic is not None:
            _validate_raw_diagnostic(
                diagnostic,
                "compilerIRFallback.bytecodeBackedFunctions"
                f"[{index}].rawInMemoryBytecodeDiagnostic",
                issues,
            )
    return issues


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("exports", nargs="+", type=Path)
    args = parser.parse_args()
    failed = False
    for path in args.exports:
        document = json.loads(path.read_text(encoding="utf-8-sig"))
        issues = validate_export_contract(document)
        if issues:
            failed = True
            print(f"FAIL {path}")
            for issue in issues:
                print(f"  - {issue}")
        else:
            print(f"PASS {path}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
