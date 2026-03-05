#!/usr/bin/env python3
"""
PRIME DIRECTIVE — Spec Validation Engine
=========================================
Validates AI-generated Blueprint specs against their source JSON IR exports.

5-layer validation:
  L1: Structural completeness — every IR field has a spec counterpart
  L2: Value accuracy — types, defaults, flags match exactly
  L3: Logic coverage — every graph entry point and branch is represented
  L4: (External) Reconstruction roundtrip — AI-driven, not in this script
  L5: (External) Self-improving feedback loop

Usage:
    python validate_specs.py [spec_dir] [export_dir] [--report out.json] [--fix-list out.md]

    spec_dir   : Directory containing .md spec files (default: docs_Lyra/BlueprintSpecs/)
    export_dir : BP_SLZR_All_* export directory (default: latest in Saved/BlueprintExports/)

Output:
    JSON validation report with per-Blueprint pass/fail and per-field diagnostics.
    Optional fix-list markdown for specs that need re-generation.
"""

import json
import os
import re
import sys
from pathlib import Path
from collections import defaultdict
from typing import Any


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
            cells = [c.strip() for c in stripped.split("|")]
            if len(cells) >= 3 and cells[1] and cells[1] != "Name" and cells[1] != "---":
                result["variable_names"].add(cells[1].strip("`").strip())

        if current_section == "functions" and "|" in stripped:
            cells = [c.strip() for c in stripped.split("|")]
            if len(cells) >= 3 and cells[1] and cells[1] != "Name" and cells[1] != "---":
                result["function_names"].add(cells[1].strip("`").strip())

        if current_section == "functions" and stripped.startswith("### "):
            fn_name = stripped[4:].split("(")[0].split("—")[0].split(" ")[0].strip("`").strip()
            if fn_name:
                result["function_names"].add(fn_name)

        if current_section == "components" and "|" in stripped:
            cells = [c.strip() for c in stripped.split("|")]
            if len(cells) >= 3 and cells[1] and cells[1] != "Name" and cells[1] != "---":
                result["component_names"].add(cells[1].strip("`").strip())

        if current_section == "cdo" and "|" in stripped:
            cells = [c.strip() for c in stripped.split("|")]
            if len(cells) >= 3 and cells[1] and cells[1] != "Property" and cells[1] != "---":
                result["cdo_properties"].add(cells[1].strip("`").strip())

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
    ir_interfaces = ir.get("implementedInterfaces") or []
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

    fallback = ir.get("compilerIRFallback") or {}
    if fallback.get("hasUnsupportedNodes") and "unsupported" not in spec.get("ir_notes_text", "").lower():
        issues.append({
            "layer": "L1", "severity": "error", "field": "unsupported_nodes",
            "name": "unsupportedNodes",
            "message": "IR has unsupported nodes but spec doesn't mention them"
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
    summary = ir.get("graphsSummary") or []
    total_nodes = ir.get("totalNodeCount") or 0

    if total_nodes == 0:
        return issues

    if not spec["has_logic"]:
        non_empty = [g for g in graphs if len(g.get("nodes", [])) > 0]
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
        branch_mentions = str(spec).lower().count("branch") + str(spec).lower().count("if ") + str(spec).lower().count("else")
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

def find_json_for_spec(spec_name: str, export_dir: Path) -> Path | None:
    """Find the JSON export file matching a spec name."""
    pattern = f"BP_SLZR_Blueprint_{spec_name}_*"
    matches = list(export_dir.glob(pattern))
    return matches[0] if matches else None


def validate_one(spec_path: Path, json_path: Path) -> dict:
    """Validate one spec against its JSON IR."""
    spec_text = spec_path.read_text(encoding="utf-8")
    with open(json_path, encoding="utf-8") as f:
        ir = json.load(f)

    spec = parse_spec(spec_text)

    issues = []
    issues.extend(validate_structural(spec, ir))
    issues.extend(validate_accuracy(spec, ir))
    issues.extend(validate_logic_coverage(spec, ir))

    errors = [i for i in issues if i["severity"] == "error"]
    warnings = [i for i in issues if i["severity"] == "warning"]

    return {
        "spec": spec_path.name,
        "json": json_path.name,
        "blueprint": ir.get("blueprintName", spec_path.stem),
        "pass": len(errors) == 0,
        "errors": len(errors),
        "warnings": len(warnings),
        "issues": issues,
        "stats": {
            "ir_node_count": ir.get("totalNodeCount", 0),
            "ir_variable_count": len(ir.get("detailedVariables") or []),
            "ir_function_count": len(ir.get("detailedFunctions") or []),
            "ir_component_count": len(ir.get("detailedComponents") or []),
            "ir_graph_count": len(ir.get("structuredGraphs") or []),
            "spec_line_count": spec["line_count"],
            "spec_has_logic": spec["has_logic"],
            "spec_has_variables": spec["has_variables"],
            "spec_has_functions": spec["has_functions"],
        }
    }


def generate_fix_list(results: list[dict]) -> str:
    """Generate markdown fix-list for specs that need re-generation."""
    lines = ["# Spec Validation Fix List", "",
             f"**Generated:** {__import__('datetime').datetime.now().isoformat()}", ""]

    failures = [r for r in results if not r["pass"]]
    warnings_only = [r for r in results if r["pass"] and r["warnings"] > 0]

    lines.append(f"## Summary")
    lines.append(f"- **Total specs validated:** {len(results)}")
    lines.append(f"- **Pass (no errors):** {len(results) - len(failures)}")
    lines.append(f"- **Fail (has errors):** {len(failures)}")
    lines.append(f"- **Pass with warnings:** {len(warnings_only)}")
    lines.append("")

    if failures:
        lines.append("## Failures (require re-generation)")
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

    print(f"PRIME DIRECTIVE — Spec Validation Engine")
    print(f"  Specs:   {spec_dir} ({len(spec_files)} files)")
    print(f"  Exports: {export_dir}")
    print()

    results = []
    matched = 0
    unmatched = 0

    for sf in spec_files:
        spec_name = sf.stem
        json_path = find_json_for_spec(spec_name, export_dir)

        if not json_path:
            unmatched += 1
            continue

        try:
            result = validate_one(sf, json_path)
            results.append(result)
            matched += 1

            status = "PASS" if result["pass"] else f"FAIL ({result['errors']}e/{result['warnings']}w)"
            if matched % 50 == 0 or not result["pass"]:
                print(f"  [{matched}/{len(spec_files)}] {spec_name}: {status}")

        except Exception as e:
            print(f"  WARN: {spec_name}: {e}")

    total_pass = sum(1 for r in results if r["pass"])
    total_fail = len(results) - total_pass
    total_errors = sum(r["errors"] for r in results)
    total_warnings = sum(r["warnings"] for r in results)

    print()
    print(f"Results:")
    print(f"  Matched:  {matched} / {len(spec_files)} specs had matching JSON exports")
    print(f"  Pass:     {total_pass} / {matched}")
    print(f"  Fail:     {total_fail} / {matched}")
    print(f"  Errors:   {total_errors}")
    print(f"  Warnings: {total_warnings}")

    report = {
        "timestamp": __import__("datetime").datetime.now().isoformat(),
        "spec_dir": str(spec_dir),
        "export_dir": str(export_dir),
        "total_specs": len(spec_files),
        "matched": matched,
        "unmatched": unmatched,
        "pass": total_pass,
        "fail": total_fail,
        "total_errors": total_errors,
        "total_warnings": total_warnings,
        "pass_rate": round(total_pass / matched * 100, 1) if matched else 0,
        "results": results,
    }

    if report_path:
        report_path.parent.mkdir(parents=True, exist_ok=True)
        report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
        print(f"  Report: {report_path}")

    if fix_list_path:
        fix_md = generate_fix_list(results)
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
        fix_md = generate_fix_list(results)
        default_fix.write_text(fix_md, encoding="utf-8")
        print(f"  Fix list: {default_fix}")

    sys.exit(0 if total_fail == 0 else 1)


if __name__ == "__main__":
    main()
