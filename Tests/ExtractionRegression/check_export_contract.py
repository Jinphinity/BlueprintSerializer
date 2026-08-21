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
