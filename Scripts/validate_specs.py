#!/usr/bin/env python3
"""
PRIME DIRECTIVE — Spec Validation Engine
=========================================
Validates AI-generated Blueprint specs against their source JSON IR exports.

5-layer validation model:
  L1: Structural completeness — every IR field has a spec counterpart
  L2: Value accuracy — types, defaults, flags match exactly
  L3: Logic coverage — every graph entry point and branch is represented
  L4: (External) Reconstruction roundtrip — AI-driven, not in this script
  L5: (External) Self-improving feedback loop

This script only executes L1-L3 static checks. A PASS is therefore a
STRUCTURAL_PASS. It is not Blueprint compile, editor roundtrip, PIE/runtime,
network, save/load, or converted-C++ validation.

Usage:
    python validate_specs.py [spec_dir] [export_dir] [--report out.json] [--fix-list out.md]

    spec_dir   : Directory containing .md spec files (default: docs_Lyra/BlueprintSpecs/)
    export_dir : BP_SLZR_All_* export directory (default: latest in Saved/BlueprintExports/)

Output:
    JSON validation report with per-Blueprint pass/fail and per-field diagnostics.
    Optional fix-list markdown for specs that need re-generation.
"""

import json
import re
import sys
from pathlib import Path
from collections import defaultdict
from typing import Any
from urllib.parse import unquote, urlsplit


VALIDATOR_VERSION = "2.0.0"
REPORT_SCHEMA = "blueprint-spec-structural-validation-v2"


class JsonPairingError(RuntimeError):
    """Raised when a spec cannot be paired to one exact Blueprint JSON export."""


def load_ir_json(path: Path) -> dict[str, Any]:
    """Load UTF-8/BOM or UTF-16 BlueprintSerializer JSON."""
    raw_ir = path.read_bytes()
    if raw_ir.startswith((b"\xff\xfe", b"\xfe\xff")):
        ir_text = raw_ir.decode("utf-16")
    else:
        ir_text = raw_ir.decode("utf-8-sig")
    return json.loads(ir_text)


def split_markdown_table_row(line: str) -> list[str]:
    """Split a pipe table row without splitting escaped or code-span pipes."""
    cells: list[str] = []
    current: list[str] = []
    escaped = False
    code_delimiter = 0
    index = 0
    while index < len(line):
        character = line[index]
        if escaped:
            if character == "|":
                current.append("|")
            else:
                current.extend(("\\", character))
            escaped = False
            index += 1
            continue
        if character == "\\":
            escaped = True
            index += 1
            continue
        if character == "`":
            end = index
            while end < len(line) and line[end] == "`":
                end += 1
            run_length = end - index
            current.extend("`" * run_length)
            if code_delimiter == 0:
                code_delimiter = run_length
            elif code_delimiter == run_length:
                code_delimiter = 0
            index = end
            continue
        if character == "|" and code_delimiter == 0:
            cells.append("".join(current).strip())
            current = []
        else:
            current.append(character)
        index += 1
    if escaped:
        current.append("\\")
    cells.append("".join(current).strip())
    if cells and cells[0] == "":
        cells.pop(0)
    if cells and cells[-1] == "":
        cells.pop()
    return cells


def _clean_table_name(value: str) -> str:
    value = value.strip()
    code_match = re.fullmatch(r"(`+)(.*)\1", value)
    if code_match:
        value = code_match.group(2)
    return value.strip()


def _table_name(line: str, header: str) -> str | None:
    cells = split_markdown_table_row(line)
    if len(cells) < 2:
        return None
    name = _clean_table_name(cells[0])
    if not name or name.casefold() == header.casefold():
        return None
    if re.fullmatch(r":?-{3,}:?", name):
        return None
    return name


def _without_inline_code(line: str) -> str:
    """Remove inline code spans before scanning prose links."""
    output: list[str] = []
    code_delimiter = 0
    index = 0
    while index < len(line):
        if line[index] == "`":
            end = index
            while end < len(line) and line[end] == "`":
                end += 1
            run_length = end - index
            if code_delimiter == 0:
                code_delimiter = run_length
            elif code_delimiter == run_length:
                code_delimiter = 0
            index = end
            continue
        if code_delimiter == 0:
            output.append(line[index])
        index += 1
    return "".join(output)


def extract_markdown_link_targets(spec_text: str) -> list[str]:
    """Extract Markdown link/image destinations outside fenced and inline code."""
    targets: list[str] = []
    fence: str | None = None
    for raw_line in spec_text.splitlines():
        stripped = raw_line.lstrip()
        if stripped.startswith(("```", "~~~~")):
            token = stripped[:3] if stripped.startswith("```") else stripped[:4]
            if fence is None:
                fence = token
            elif fence == token:
                fence = None
            continue
        if fence is not None:
            continue
        line = _without_inline_code(raw_line)
        search_from = 0
        while True:
            marker = line.find("](", search_from)
            if marker == -1:
                break
            start = marker + 2
            if start < len(line) and line[start] == "<":
                end = line.find(">", start + 1)
                if end != -1 and end + 1 < len(line) and line[end + 1] == ")":
                    targets.append(line[start + 1:end])
                    search_from = end + 2
                    continue
            depth = 0
            escaped = False
            end = start
            while end < len(line):
                character = line[end]
                if escaped:
                    escaped = False
                elif character == "\\":
                    escaped = True
                elif character == "(":
                    depth += 1
                elif character == ")":
                    if depth == 0:
                        break
                    depth -= 1
                end += 1
            if end >= len(line):
                break
            target = line[start:end].strip()
            title_match = re.match(r"^(.*\S)\s+(?:\"[^\"]*\"|'[^']*')$", target)
            if title_match:
                target = title_match.group(1)
            targets.append(target)
            search_from = end + 1
    return targets


# ---------------------------------------------------------------------------
# Spec parser — extracts structured data from markdown specs
# ---------------------------------------------------------------------------

def parse_spec(spec_text: str) -> dict:
    """Parse a markdown spec into structured fields for comparison."""
    # Detect Identity from either explicit ## Identity heading OR from the
    # Cursor-generated bold-field header format (**Path:** / **Parent:**).
    # Both formats carry equivalent reconstruction-complete identity information.
    has_implicit_identity = bool(
        re.search(r'\*\*Path:\*\*', spec_text) and
        re.search(r'\*\*Parent:\*\*', spec_text)
    )

    result = {
        "has_identity": has_implicit_identity,  # upgraded to True if ## Identity found
        "has_purpose": False,
        "has_variables": False,
        "has_functions": False,
        "has_components": False,
        "has_logic": False,
        "has_dependencies": False,
        "has_ir_notes": False,
        "has_cdo_overrides": False,
        "variable_names": set(),
        "function_names": set(),
        "component_names": set(),
        "cdo_properties": set(),
        "module_names": set(),
        "logic_entry_points": set(),
        "mentioned_classes": set(),
        "full_text": spec_text,
        "ir_notes_text": "",
        "line_count": len(spec_text.splitlines()),
    }

    lines = spec_text.splitlines()
    current_section = ""

    for line in lines:
        stripped = line.strip()

        if stripped.startswith("## "):
            section = stripped[3:].strip().lower()
            if "identity" in section:
                result["has_identity"] = True
                current_section = "identity"
            elif "purpose" in section:
                result["has_purpose"] = True
                current_section = "purpose"
            elif "variable" in section:
                result["has_variables"] = True
                current_section = "variables"
            elif "function" in section:
                result["has_functions"] = True
                current_section = "functions"
            elif "component" in section:
                result["has_components"] = True
                current_section = "components"
            elif "logic" in section or "graph" in section or "event" in section:
                result["has_logic"] = True
                current_section = "logic"
            elif "dependenc" in section or "c++ dep" in section:
                result["has_dependencies"] = True
                current_section = "dependencies"
            elif "ir note" in section:
                result["has_ir_notes"] = True
                current_section = "ir_notes"
            elif "cdo" in section or "class default" in section:
                result["has_cdo_overrides"] = True
                current_section = "cdo"
            else:
                current_section = section

        if current_section == "variables" and "|" in stripped:
            name = _table_name(stripped, "Name")
            if name:
                result["variable_names"].add(name)

        if current_section == "functions" and "|" in stripped:
            name = _table_name(stripped, "Name")
            if name:
                result["function_names"].add(name)

        if current_section == "functions" and stripped.startswith("### "):
            fn_name = stripped[4:].split("(")[0].split("—")[0].split(" ")[0].strip("`").strip()
            if fn_name:
                result["function_names"].add(fn_name)

        if current_section == "components" and "|" in stripped:
            name = _table_name(stripped, "Name")
            if name:
                result["component_names"].add(name)

        if current_section == "cdo" and "|" in stripped:
            name = _table_name(stripped, "Property")
            if name:
                result["cdo_properties"].add(name)

        if current_section == "dependencies":
            module_match = re.search(r'\*\*Modules?:\*\*\s*(.+)', stripped)
            if module_match:
                for m in module_match.group(1).split(","):
                    m = m.strip().strip("`")
                    if m:
                        result["module_names"].add(m)

        if current_section == "logic":
            event_match = re.match(r'#{3,4}\s+(?:Event:\s*)?(.+)', stripped)
            if event_match:
                ep = event_match.group(1).strip()
                result["logic_entry_points"].add(ep)

        if current_section == "ir_notes" and stripped:
            result["ir_notes_text"] += stripped + "\n"

    return result


# ---------------------------------------------------------------------------
# L1: Structural Completeness
# ---------------------------------------------------------------------------

def _norm(name: str) -> str:
    """Normalize an IR name for comparison against spec names.

    UE Blueprint display names sometimes carry trailing spaces (e.g. "Follow Player ",
    "Update ", "Initialize Size ").  These are UE editor metadata artifacts — not
    intentional parts of the identifier — and are faithfully preserved in the JSON IR
    for reconstruction completeness.  The spec markdown parser strips them via .strip(),
    so we must strip the IR side too before comparison, otherwise the lookup always fails.

    We use rstrip() (not strip()) to preserve any intentional leading whitespace, though
    none is known to exist in the Lyra corpus.
    """
    return name.rstrip()


def _support_types(ir: dict, field: str) -> list[str]:
    """Return the union of a support-list field across all serializer surfaces."""
    values: set[str] = set()
    for surface in (ir, ir.get("coverage") or {}, ir.get("compilerIRFallback") or {}):
        for value in surface.get(field) or []:
            if value:
                values.add(str(value))
    return sorted(values, key=str.casefold)


def collect_compiler_diagnostics(ir: dict) -> list[dict[str, Any]]:
    """Collect message-bearing node diagnostics, excluding empty severity markers."""
    diagnostics: list[dict[str, Any]] = []
    seen: set[tuple[str, str, str]] = set()
    for graph_index, graph in enumerate(ir.get("structuredGraphs") or []):
        for node_index, node in enumerate(graph.get("nodes") or []):
            properties = node.get("nodeProperties") or {}
            error_message = str(
                node.get("errorMsg")
                or properties.get("ErrorMsg")
                or ""
            ).strip()
            upgrade_message = str(
                node.get("nodeUpgradeMessage")
                or properties.get("NodeUpgradeMessage")
                or ""
            ).strip()
            if not error_message and not upgrade_message:
                continue
            guid = str(node.get("nodeGuid") or "")
            key = (guid, error_message, upgrade_message)
            if key in seen:
                continue
            seen.add(key)
            diagnostics.append({
                "graphIndex": graph_index,
                "graph": graph.get("name"),
                "nodeIndex": node_index,
                "nodeGuid": guid,
                "nodeType": node.get("nodeType"),
                "errorType": node.get("errorType", properties.get("ErrorType")),
                "errorMsg": error_message,
                "nodeUpgradeMessage": upgrade_message,
            })
    return diagnostics


def count_nonzero_error_type_markers(ir: dict) -> int:
    """Count reflected nonzero ErrorType markers without calling them diagnostics."""
    count = 0
    for graph in ir.get("structuredGraphs") or []:
        for node in graph.get("nodes") or []:
            properties = node.get("nodeProperties") or {}
            value = node.get("errorType", properties.get("ErrorType", 0))
            if str(value).strip().casefold() not in {"", "0", "0.0", "none", "false"}:
                count += 1
    return count


def validate_relative_links(spec_path: Path, spec_text: str) -> list[dict]:
    """Check local Markdown destinations relative to the spec file."""
    issues: list[dict] = []
    for target in extract_markdown_link_targets(spec_text):
        raw_target = target.strip()
        if not raw_target or raw_target.startswith("#"):
            continue
        parsed = urlsplit(raw_target)
        if parsed.scheme or parsed.netloc or raw_target.startswith("/"):
            continue
        relative = unquote(parsed.path)
        if not relative:
            continue
        resolved = (spec_path.parent / relative).resolve()
        if not resolved.exists():
            issues.append({
                "layer": "L1",
                "severity": "warning",
                "field": "relative_link",
                "name": raw_target,
                "message": (
                    f"Relative Markdown link '{raw_target}' resolves to missing "
                    f"path '{resolved}'"
                ),
            })
    return issues


def validate_structural(spec: dict, ir: dict) -> list[dict]:
    """Check every IR field has a spec counterpart."""
    issues = []

    ir_vars = [v["name"] for v in (ir.get("detailedVariables") or [])]

    # Functions the validator treats as optional / compiler-managed.
    # ExecuteUbergraph_*  — compiler-generated ubergraph thunks, never user-authored.
    # UserConstructionScript — auto-present on all Actor BPs; should appear in spec
    #   ONLY when it contains authored logic (the playbook rules this).  L3 logic
    #   coverage will flag missing UCS logic for BPs with substantial UCS content.
    #   Requiring it in L1 causes hundreds of false positives on data-only BPs.
    _OPTIONAL_FUNC_NAMES = frozenset({"UserConstructionScript"})
    ir_funcs = [f["name"] for f in (ir.get("detailedFunctions") or [])
                if f["name"] != "ExecuteUbergraph"
                and not f["name"].startswith("ExecuteUbergraph_")
                and f["name"] not in _OPTIONAL_FUNC_NAMES]
    ir_comps = [c["name"] for c in (ir.get("detailedComponents") or [])]
    cdo_delta = ir.get("classDefaultValueDelta") or {}

    # Build a deduplicated normalized set for efficient lookup, but iterate originals
    # so error messages show the real IR name (including trailing spaces when present).
    _seen_vars: set[str] = set()
    for v in ir_vars:
        v_norm = _norm(v)
        if v_norm in _seen_vars:
            continue  # skip duplicate after normalization (e.g. "Update" and "Update ")
        _seen_vars.add(v_norm)
        if v_norm not in spec["variable_names"] and v_norm != "UberGraphFrame":
            issues.append({
                "layer": "L1", "severity": "error", "field": "variable",
                "name": v, "message": f"Variable '{v}' in IR but missing from spec"
            })

    _seen_funcs: set[str] = set()
    for f in ir_funcs:
        f_norm = _norm(f)
        if f_norm in _seen_funcs:
            continue  # skip duplicate after normalization
        _seen_funcs.add(f_norm)
        if f_norm not in spec["function_names"]:
            issues.append({
                "layer": "L1", "severity": "error", "field": "function",
                "name": f, "message": f"Function '{f}' in IR but missing from spec"
            })

    for c in ir_comps:
        if c not in spec["component_names"] and not spec["has_components"]:
            if ir_comps:
                issues.append({
                    "layer": "L1", "severity": "warning", "field": "component",
                    "name": c, "message": f"Component '{c}' in IR but no Components section in spec"
                })

    if cdo_delta and not spec["has_cdo_overrides"]:
        if len(cdo_delta) > 0:
            issues.append({
                "layer": "L1", "severity": "error", "field": "cdo",
                "name": "classDefaultValueDelta",
                "message": f"CDO has {len(cdo_delta)} overrides but spec has no CDO section"
            })

    for prop in cdo_delta:
        prop_norm = _norm(prop)
        if prop_norm not in spec["cdo_properties"]:
            issues.append({
                "layer": "L1", "severity": "warning", "field": "cdo_property",
                "name": prop,
                "message": f"CDO property '{prop}' overridden in IR but not listed in spec"
            })

    dep = ir.get("dependencyClosure") or {}
    ir_modules = set(dep.get("moduleNames") or [])
    missing_modules = ir_modules - spec["module_names"]
    if missing_modules and spec["has_dependencies"]:
        for m in missing_modules:
            issues.append({
                "layer": "L1", "severity": "warning", "field": "module",
                "name": m, "message": f"Module '{m}' in IR dependency closure but missing from spec"
            })

    if not spec["has_identity"]:
        issues.append({
            "layer": "L1", "severity": "error", "field": "identity",
            "name": "Identity", "message": "Spec has no Identity section"
        })

    if not spec["has_purpose"]:
        issues.append({
            "layer": "L1", "severity": "warning", "field": "purpose",
            "name": "Purpose", "message": "Spec has no Purpose section"
        })

    if not spec["has_ir_notes"]:
        issues.append({
            "layer": "L1", "severity": "warning", "field": "ir_notes",
            "name": "IR Notes", "message": "Spec has no IR Notes section"
        })

    ir_notes = spec.get("ir_notes_text", "")
    unsupported_types = _support_types(ir, "unsupportedNodeTypes")
    partially_supported_types = _support_types(ir, "partiallySupportedNodeTypes")
    fallback = ir.get("compilerIRFallback") or {}
    if fallback.get("hasUnsupportedNodes") and not unsupported_types:
        issues.append({
            "layer": "L1", "severity": "error", "field": "unsupported_nodes",
            "name": "unsupportedNodes",
            "message": (
                "IR flags unsupported nodes but emits no exact unsupported node "
                "types; repair/re-export the serializer evidence"
            ),
        })

    for node_type in unsupported_types:
        if "unsupported" not in ir_notes.casefold() or node_type.casefold() not in ir_notes.casefold():
            issues.append({
                "layer": "L1", "severity": "error", "field": "unsupported_node_type",
                "name": node_type,
                "message": (
                    f"Unsupported node type '{node_type}' is present in IR but "
                    "not identified as unsupported in IR Notes"
                ),
            })

    for node_type in partially_supported_types:
        if "partial" not in ir_notes.casefold() or node_type.casefold() not in ir_notes.casefold():
            issues.append({
                "layer": "L1", "severity": "error", "field": "partial_node_type",
                "name": node_type,
                "message": (
                    f"Partially supported node type '{node_type}' is present in "
                    "IR but not identified as partially supported in IR Notes"
                ),
            })

    diagnostics = collect_compiler_diagnostics(ir)
    source_text = spec.get("full_text", "").casefold()
    undisclosed_diagnostics = [
        diagnostic for diagnostic in diagnostics
        if diagnostic["nodeGuid"]
        and diagnostic["nodeGuid"].casefold() not in source_text
    ]
    if undisclosed_diagnostics:
        preview = ", ".join(
            diagnostic["nodeGuid"] for diagnostic in undisclosed_diagnostics[:8]
        )
        suffix = "" if len(undisclosed_diagnostics) <= 8 else ", ..."
        issues.append({
            "layer": "L1",
            "severity": "error",
            "field": "compiler_diagnostic",
            "name": "message-bearing node diagnostics",
            "message": (
                f"IR has {len(undisclosed_diagnostics)} message-bearing compiler "
                f"diagnostic site(s) not disclosed by node GUID in the spec: "
                f"{preview}{suffix}"
            ),
        })

    return issues


# ---------------------------------------------------------------------------
# L2: Value Accuracy
# ---------------------------------------------------------------------------

def validate_accuracy(spec: dict, ir: dict) -> list[dict]:
    """Check types, defaults, flags match."""
    issues = []

    # Build normalized lookup dicts so L2 checks work even when spec strips trailing spaces.
    ir_vars = {}
    for v in (ir.get("detailedVariables") or []):
        ir_vars[_norm(v["name"])] = v
    ir_funcs = {}
    for f in (ir.get("detailedFunctions") or []):
        ir_funcs[_norm(f["name"])] = f

    for vname in spec["variable_names"]:
        if vname in ir_vars:
            v = ir_vars[vname]
            if v.get("isReplicated") and "replicat" not in str(spec).lower():
                issues.append({
                    "layer": "L2", "severity": "error", "field": "replication",
                    "name": vname,
                    "message": f"Variable '{vname}' is replicated in IR but spec doesn't mention replication"
                })

    for fname in spec["function_names"]:
        if fname in ir_funcs:
            f = ir_funcs[fname]
            if f.get("isNetServer") or f.get("isNetClient") or f.get("isNetMulticast"):
                net_type = "Server" if f.get("isNetServer") else ("Client" if f.get("isNetClient") else "Multicast")
                if net_type.lower() not in str(spec).lower():
                    issues.append({
                        "layer": "L2", "severity": "error", "field": "network_function",
                        "name": fname,
                        "message": f"Function '{fname}' is {net_type} RPC in IR but spec doesn't mention it"
                    })

    return issues


# ---------------------------------------------------------------------------
# L3: Logic Coverage
# ---------------------------------------------------------------------------

def validate_logic_coverage(spec: dict, ir: dict) -> list[dict]:
    """Check every graph entry point and branch is represented."""
    issues = []

    graphs = ir.get("structuredGraphs") or []
    total_nodes = ir.get("totalNodeCount") or 0

    if total_nodes == 0:
        return issues

    if not spec["has_logic"]:
        ubergraph_nodes = 0
        for g in graphs:
            if g.get("graphType") == "Ubergraph":
                ubergraph_nodes = len(g.get("nodes", []))

        if ubergraph_nodes > 0:
            issues.append({
                "layer": "L3", "severity": "error", "field": "logic_section",
                "name": "Logic",
                "message": f"Blueprint has {total_nodes} nodes ({ubergraph_nodes} in EventGraph) but spec has no Logic section"
            })
        return issues

    for g in graphs:
        if g.get("graphType") in ("Event", "Ubergraph"):
            for node in g.get("nodes", []):
                ntype = node.get("nodeType", "")
                props = node.get("nodeProperties") or {}

                if ntype == "K2Node_Event":
                    event_name = props.get("meta.eventName") or node.get("title", "")
                    if event_name and not props.get("bIsIntermediateNode") == "True":
                        found = False
                        for ep in spec["logic_entry_points"]:
                            if event_name.lower() in ep.lower() or ep.lower() in event_name.lower():
                                found = True
                                break
                        if not found:
                            issues.append({
                                "layer": "L3", "severity": "warning", "field": "entry_point",
                                "name": event_name,
                                "message": f"Event '{event_name}' in EventGraph but not found in spec Logic section"
                            })

    ubergraph = next((g for g in graphs if g.get("graphType") == "Ubergraph"), None)
    if ubergraph:
        exec_edges = len((ubergraph.get("flows") or {}).get("execution", []))
        if exec_edges > 10 and spec["line_count"] < 30:
            issues.append({
                "layer": "L3", "severity": "warning", "field": "logic_depth",
                "name": "shallow_spec",
                "message": f"EventGraph has {exec_edges} exec edges but spec is only {spec['line_count']} lines — likely too shallow"
            })

    branch_count = sum(
        1 for g in graphs for n in g.get("nodes", [])
        if n.get("nodeType") == "K2Node_IfThenElse"
        and n.get("nodeProperties", {}).get("bIsIntermediateNode") != "True"
    )
    if branch_count > 3:
        spec_text = spec.get("full_text", "").lower()
        branch_mentions = spec_text.count("branch") + spec_text.count("if ") + spec_text.count("else")
        if branch_mentions < branch_count // 2:
            issues.append({
                "layer": "L3", "severity": "warning", "field": "branch_coverage",
                "name": "branches",
                "message": f"IR has {branch_count} branch nodes but spec only mentions ~{branch_mentions} conditions"
            })

    return issues


# ---------------------------------------------------------------------------
# Corpus-level validation
# ---------------------------------------------------------------------------

def index_json_exports(export_dir: Path) -> tuple[dict[str, list[Path]], list[dict[str, str]]]:
    """Index exports by exact serialized blueprintName metadata."""
    index: dict[str, list[Path]] = defaultdict(list)
    errors: list[dict[str, str]] = []
    for json_path in sorted(export_dir.glob("BP_SLZR_Blueprint_*.json")):
        try:
            ir = load_ir_json(json_path)
        except Exception as exc:
            errors.append({"json": str(json_path), "error": f"JSON load failed: {exc}"})
            continue
        blueprint_name = ir.get("blueprintName")
        if not isinstance(blueprint_name, str) or not blueprint_name:
            errors.append({
                "json": str(json_path),
                "error": "JSON has no nonempty blueprintName; exact pairing is impossible",
            })
            continue
        index[blueprint_name].append(json_path)
    return dict(index), errors


def find_json_for_spec(
    spec_name: str,
    export_dir: Path,
    export_index: dict[str, list[Path]] | None = None,
) -> Path | None:
    """Find one JSON whose serialized blueprintName exactly equals the spec stem."""
    if export_index is None:
        export_index, _ = index_json_exports(export_dir)
    matches = export_index.get(spec_name, [])
    if len(matches) > 1:
        candidates = ", ".join(path.name for path in matches)
        raise JsonPairingError(
            f"Spec '{spec_name}' has {len(matches)} exact JSON candidates: {candidates}"
        )
    return matches[0] if matches else None


def validate_one(spec_path: Path, json_path: Path) -> dict:
    """Validate one spec against its JSON IR."""
    spec_text = spec_path.read_text(encoding="utf-8")
    ir = load_ir_json(json_path)

    if ir.get("blueprintName") != spec_path.stem:
        raise JsonPairingError(
            f"Spec stem '{spec_path.stem}' does not exactly match serialized "
            f"blueprintName '{ir.get('blueprintName')}' in '{json_path.name}'"
        )

    spec = parse_spec(spec_text)

    issues = []
    issues.extend(validate_structural(spec, ir))
    issues.extend(validate_accuracy(spec, ir))
    issues.extend(validate_logic_coverage(spec, ir))
    issues.extend(validate_relative_links(spec_path, spec_text))

    errors = [i for i in issues if i["severity"] == "error"]
    warnings = [i for i in issues if i["severity"] == "warning"]

    return {
        "spec": spec_path.name,
        "json": json_path.name,
        "blueprint": ir.get("blueprintName", spec_path.stem),
        "result": "STRUCTURAL_PASS" if len(errors) == 0 else "STRUCTURAL_FAIL",
        "structural_pass": len(errors) == 0,
        "pass": len(errors) == 0,
        "runtime_validated": False,
        "roundtrip_validated": False,
        "errors": len(errors),
        "warnings": len(warnings),
        "issues": issues,
        "stats": {
            "ir_node_count": ir.get("totalNodeCount", 0),
            "ir_variable_count": len(ir.get("detailedVariables") or []),
            "ir_function_count": len(ir.get("detailedFunctions") or []),
            "ir_component_count": len(ir.get("detailedComponents") or []),
            "ir_graph_count": len(ir.get("structuredGraphs") or []),
            "ir_unsupported_node_types": _support_types(ir, "unsupportedNodeTypes"),
            "ir_partially_supported_node_types": _support_types(ir, "partiallySupportedNodeTypes"),
            "ir_compiler_diagnostic_count": len(collect_compiler_diagnostics(ir)),
            "ir_nonzero_error_type_marker_count": count_nonzero_error_type_markers(ir),
            "spec_line_count": spec["line_count"],
            "spec_has_logic": spec["has_logic"],
            "spec_has_variables": spec["has_variables"],
            "spec_has_functions": spec["has_functions"],
        }
    }


def generate_fix_list(
    results: list[dict],
    pairing_issues: list[dict[str, str]] | None = None,
) -> str:
    """Generate markdown fix-list for specs that need re-generation."""
    pairing_issues = pairing_issues or []
    lines = ["# Spec Structural Validation Fix List", "",
             f"**Generated:** {__import__('datetime').datetime.now().isoformat()}", ""]
    lines.extend([
        f"**Validator:** `{VALIDATOR_VERSION}` (`{REPORT_SCHEMA}`)",
        "",
        "> A structural pass covers the static L1-L3 checks implemented by this "
        "tool. It does not prove Blueprint compile cleanliness, editor roundtrip, "
        "PIE/runtime behavior, multiplayer correctness, persistence correctness, "
        "or C++ implementation.",
        "",
    ])

    failures = [r for r in results if not r["pass"]]
    warnings_only = [r for r in results if r["pass"] and r["warnings"] > 0]

    lines.append("## Summary")
    lines.append(f"- **Total specs validated:** {len(results)}")
    lines.append(f"- **Structural pass (no errors):** {len(results) - len(failures)}")
    lines.append(f"- **Structural fail (has errors):** {len(failures)}")
    lines.append(f"- **Structural pass with warnings:** {len(warnings_only)}")
    lines.append(f"- **Pairing issues:** {len(pairing_issues)}")
    lines.append("")

    if pairing_issues:
        lines.append("## Pairing failures")
        lines.append("")
        for issue in pairing_issues:
            lines.append(f"- `{issue.get('spec', issue.get('json', 'unknown'))}`: {issue['error']}")
        lines.append("")

    if failures:
        lines.append("## Structural failures (require evidence/spec repair)")
        lines.append("")
        lines.append("| Blueprint | Errors | Top Issue |")
        lines.append("|-----------|--------|-----------|")
        for r in sorted(failures, key=lambda x: -x["errors"]):
            top_issue = r["issues"][0]["message"] if r["issues"] else ""
            lines.append(f"| {r['blueprint']} | {r['errors']} | {top_issue[:80]} |")
        lines.append("")

        lines.append("### Detailed Error Log")
        lines.append("")
        for r in sorted(failures, key=lambda x: x["blueprint"]):
            lines.append(f"#### {r['blueprint']}")
            for issue in r["issues"]:
                if issue["severity"] == "error":
                    lines.append(f"- **{issue['layer']}** {issue['field']}: {issue['message']}")
            lines.append("")

    if warnings_only:
        lines.append("## Warnings (should review)")
        lines.append("")
        for r in sorted(warnings_only, key=lambda x: -x["warnings"])[:20]:
            lines.append(f"### {r['blueprint']} ({r['warnings']} warnings)")
            for issue in r["issues"][:5]:
                lines.append(f"- {issue['layer']} {issue['field']}: {issue['message']}")
            lines.append("")

    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def find_latest_export_dir(project_root: Path) -> Path | None:
    exports = project_root / "Saved" / "BlueprintExports"
    if not exports.exists():
        return None
    runs = sorted([d for d in exports.iterdir()
                   if d.is_dir() and d.name.startswith("BP_SLZR_All_")], reverse=True)
    return runs[0] if runs else None


def main():
    script_dir = Path(__file__).resolve().parent
    plugin_dir = script_dir.parent
    project_root = plugin_dir.parent.parent

    if len(sys.argv) >= 2:
        spec_dir = Path(sys.argv[1])
    else:
        spec_dir = project_root / "docs_Lyra" / "BlueprintSpecs"

    if len(sys.argv) >= 3:
        export_dir = Path(sys.argv[2])
    else:
        export_dir = find_latest_export_dir(project_root)
        if not export_dir:
            print("ERROR: No export directory found.")
            sys.exit(1)

    report_path = None
    fix_list_path = None
    for i, arg in enumerate(sys.argv):
        if arg == "--report" and i + 1 < len(sys.argv):
            report_path = Path(sys.argv[i + 1])
        if arg == "--fix-list" and i + 1 < len(sys.argv):
            fix_list_path = Path(sys.argv[i + 1])

    spec_files = sorted(spec_dir.glob("*.md"))
    spec_files = [f for f in spec_files if not f.name.startswith("_")]

    print(f"PRIME DIRECTIVE — Spec Structural Validation Engine v{VALIDATOR_VERSION}")
    print(f"  Specs:   {spec_dir} ({len(spec_files)} files)")
    print(f"  Exports: {export_dir}")
    print("  Scope:   static L1-L3 only; runtime and roundtrip are not executed")
    print()

    results = []
    matched = 0
    export_index, export_index_errors = index_json_exports(export_dir)
    pairing_issues: list[dict[str, str]] = list(export_index_errors)

    for sf in spec_files:
        spec_name = sf.stem
        try:
            json_path = find_json_for_spec(spec_name, export_dir, export_index)
        except JsonPairingError as exc:
            pairing_issues.append({"spec": sf.name, "error": str(exc)})
            print(f"  PAIRING FAIL: {spec_name}: {exc}")
            continue

        if not json_path:
            pairing_issues.append({
                "spec": sf.name,
                "error": (
                    f"No JSON export has serialized blueprintName exactly equal "
                    f"to '{spec_name}'"
                ),
            })
            continue

        try:
            result = validate_one(sf, json_path)
            results.append(result)
            matched += 1

            status = (
                "STRUCTURAL_PASS"
                if result["pass"]
                else f"STRUCTURAL_FAIL ({result['errors']}e/{result['warnings']}w)"
            )
            if matched % 50 == 0 or not result["pass"]:
                print(f"  [{matched}/{len(spec_files)}] {spec_name}: {status}")

        except Exception as e:
            pairing_issues.append({
                "spec": sf.name,
                "error": f"Validation could not complete: {e}",
            })
            print(f"  VALIDATION FAIL: {spec_name}: {e}")

    total_pass = sum(1 for r in results if r["pass"])
    total_fail = len(results) - total_pass
    total_errors = sum(r["errors"] for r in results)
    total_warnings = sum(r["warnings"] for r in results)
    unmatched = len(spec_files) - matched
    structural_pass = (
        bool(spec_files)
        and total_pass == len(spec_files)
        and not pairing_issues
    )

    print()
    print("Structural results:")
    print(f"  Matched:  {matched} / {len(spec_files)} specs had matching JSON exports")
    print(f"  Pass:     {total_pass} / {len(spec_files)}")
    print(f"  Fail:     {total_fail} validated + {len(pairing_issues)} pairing/processing")
    print(f"  Errors:   {total_errors}")
    print(f"  Warnings: {total_warnings}")
    print(f"  Result:   {'STRUCTURAL_PASS' if structural_pass else 'STRUCTURAL_FAIL'}")
    print("  Runtime:  NOT_VALIDATED")
    print("  Roundtrip: NOT_VALIDATED")

    report = {
        "schema": REPORT_SCHEMA,
        "validator_version": VALIDATOR_VERSION,
        "timestamp": __import__("datetime").datetime.now().isoformat(),
        "result": "STRUCTURAL_PASS" if structural_pass else "STRUCTURAL_FAIL",
        "structural_pass": structural_pass,
        "scope": {
            "layers_executed": ["L1", "L2", "L3"],
            "blueprint_compile_validated": False,
            "editor_roundtrip_validated": False,
            "pie_runtime_validated": False,
            "multiplayer_validated": False,
            "persistence_validated": False,
            "cpp_implementation_validated": False,
        },
        "spec_dir": str(spec_dir),
        "export_dir": str(export_dir),
        "total_specs": len(spec_files),
        "matched": matched,
        "unmatched": unmatched,
        "pass": total_pass,
        "fail": total_fail,
        "total_errors": total_errors,
        "total_warnings": total_warnings,
        "pairing_errors": len(pairing_issues),
        "pass_rate": round(total_pass / len(spec_files) * 100, 1) if spec_files else 0,
        "pairing_issues": pairing_issues,
        "results": results,
    }

    if report_path:
        report_path.parent.mkdir(parents=True, exist_ok=True)
        report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
        print(f"  Report: {report_path}")

    if fix_list_path:
        fix_md = generate_fix_list(results, pairing_issues)
        fix_list_path.parent.mkdir(parents=True, exist_ok=True)
        fix_list_path.write_text(fix_md, encoding="utf-8")
        print(f"  Fix list: {fix_list_path}")

    if not report_path:
        default_report = project_root / "Saved" / "BlueprintSpecs" / "_VALIDATION_REPORT.json"
        default_report.parent.mkdir(parents=True, exist_ok=True)
        default_report.write_text(json.dumps(report, indent=2), encoding="utf-8")
        print(f"  Report: {default_report}")

    if not fix_list_path:
        default_fix = project_root / "docs_Lyra" / "BlueprintSpecs" / "_VALIDATION_FAILURES.md"
        fix_md = generate_fix_list(results, pairing_issues)
        default_fix.write_text(fix_md, encoding="utf-8")
        print(f"  Fix list: {default_fix}")

    sys.exit(0 if structural_pass else 1)


if __name__ == "__main__":
    main()
