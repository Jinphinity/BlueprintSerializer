#!/usr/bin/env python3
"""
LYRA-PRIME Phase 1b — Blueprint Spec Generator
===============================================
Converts BlueprintSerializer JSON exports into token-efficient, AI-readable markdown specs.

Usage:
    python generate_specs.py [export_dir] [output_dir]

    export_dir  : Path to a BP_SLZR_All_* export directory.
                  Defaults to the most recent run in Saved/BlueprintExports/.
    output_dir  : Where to write .md spec files.
                  Defaults to Saved/BlueprintSpecs/.

Output:
    <output_dir>/<BlueprintName>.md  — one spec per Blueprint
    <output_dir>/_INDEX.md           — master index grouped by system

Mandate: LYRA-PRIME — every spec must be human+AI readable, token-efficient,
and contain complete technical detail needed to understand and replicate
the Blueprint's behaviour in a new Lyra game.
"""

import json
import os
import sys
import re
from pathlib import Path
from collections import defaultdict


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def short_path(path: str) -> str:
    """Trim /Game/ or /Script/ prefix for readability."""
    if not path or path == "None":
        return ""
    path = re.sub(r'^/Game/', '', path)
    path = re.sub(r'^/Script/', '', path)
    return path


def type_str(var: dict) -> str:
    """Produce a readable C++-style type string from a detailedVariable entry."""
    cat = var.get("typeCategory", "")
    obj = var.get("typeObjectPath", "") or ""
    subcat = var.get("typeSubCategory", "") or ""
    is_array = var.get("isArray", False)
    is_map = var.get("isMap", False)
    is_set = var.get("isSet", False)

    if cat in ("bool", "int", "int64", "float", "double", "byte", "string", "text", "name"):
        base = {"bool": "bool", "int": "int32", "int64": "int64", "float": "float",
                "double": "double", "byte": "uint8", "string": "FString",
                "text": "FText", "name": "FName"}.get(cat, cat)
    elif cat == "object":
        cls = obj.split(".")[-1] if "." in obj else obj.split("/")[-1]
        base = f"{cls}*" if cls else "UObject*"
    elif cat == "struct":
        cls = obj.split(".")[-1] if "." in obj else obj.split("/")[-1]
        base = cls or "FStruct"
    elif cat == "class":
        cls = obj.split(".")[-1] if "." in obj else obj.split("/")[-1]
        base = f"TSubclassOf<{cls}>" if cls else "TSubclassOf<UObject>"
    elif cat == "enum":
        cls = obj.split(".")[-1] if "." in obj else obj.split("/")[-1]
        base = cls or "uint8"
    elif cat == "interface":
        cls = obj.split(".")[-1] if "." in obj else obj.split("/")[-1]
        base = f"TScriptInterface<{cls}>" if cls else "TScriptInterface<IInterface>"
    elif cat == "softobject":
        cls = obj.split(".")[-1] if "." in obj else obj.split("/")[-1]
        base = f"TSoftObjectPtr<{cls}>" if cls else "TSoftObjectPtr<UObject>"
    elif cat == "softclass":
        cls = obj.split(".")[-1] if "." in obj else obj.split("/")[-1]
        base = f"TSoftClassPtr<{cls}>" if cls else "TSoftClassPtr<UObject>"
    elif cat == "delegate":
        base = subcat or "FDelegate"
    else:
        base = cat or "?"

    if is_array:
        return f"TArray<{base}>"
    if is_set:
        return f"TSet<{base}>"
    if is_map:
        val_cat = var.get("valueTypeCategory", "")
        return f"TMap<{base}, {val_cat or '?'}>"
    return base


def repl_str(var: dict) -> str:
    cond = var.get("replicationCondition", "None") or "None"
    notify = var.get("repNotifyFunction", "None") or "None"
    if not var.get("isReplicated", False):
        return ""
    s = cond if cond != "None" else "Replicated"
    if notify != "None":
        s += f" / RepNotify={notify}"
    return s


def net_str(fn: dict) -> str:
    parts = []
    if fn.get("isNetServer"):
        parts.append("Server")
    if fn.get("isNetClient"):
        parts.append("Client")
    if fn.get("isNetMulticast"):
        parts.append("Multicast")
    if fn.get("isReliable"):
        parts.append("Reliable")
    if fn.get("blueprintAuthorityOnly"):
        parts.append("AuthOnly")
    if fn.get("blueprintCosmetic"):
        parts.append("Cosmetic")
    return ", ".join(parts)


def param_str(params: list) -> str:
    if not params:
        return ""
    parts = []
    for p in params:
        ptype = p.get("type", "") or p.get("typeCategory", "?")
        parts.append(f"{p.get('name','?')}: {ptype}")
    return ", ".join(parts)


# ---------------------------------------------------------------------------
# Graph pseudo-code generation
# ---------------------------------------------------------------------------

def build_node_map(nodes: list) -> dict:
    """nodeGuid → node dict."""
    return {n["nodeGuid"]: n for n in nodes}


def build_exec_adjacency(flows: dict) -> dict:
    """sourcePinId → list of (targetNodeGuid, targetPinName, sourcePinName)."""
    adj = defaultdict(list)
    for e in flows.get("execution", []):
        key = (e["sourceNodeGuid"], e["sourcePinName"])
        adj[key].append((e["targetNodeGuid"], e["targetPinName"]))
    return adj


def build_data_map(flows: dict) -> dict:
    """targetNodeGuid → list of (sourcePinName, targetPinName, sourceNodeGuid, type)."""
    dm = defaultdict(list)
    for d in flows.get("data", []):
        dm[d["targetNodeGuid"]].append({
            "srcNode": d["sourceNodeGuid"],
            "srcPin": d["sourcePinName"],
            "tgtPin": d["targetPinName"],
            "type": d.get("pinSubCategoryObjectPath") or d.get("pinCategory", ""),
        })
    return dm


def get_node_meta(node: dict, key: str) -> str:
    props = node.get("nodeProperties", {})
    if isinstance(props, dict):
        return props.get(key, "")
    return ""


def node_label(node: dict, nmap: dict, data_map: dict) -> str:
    """Produce a concise human-readable label for a node."""
    ntype = node.get("nodeType", "")
    title = node.get("title", "")
    guid = node.get("nodeGuid", "")
    props = node.get("nodeProperties") or {}

    if ntype == "K2Node_CallFunction":
        fn = props.get("meta.functionName") or title
        owner = props.get("meta.functionOwner", "")
        owner_short = owner.split(".")[-1] if owner else ""
        is_static = props.get("meta.isStatic", "") == "true"
        if is_static and owner_short:
            return f"{owner_short}::{fn}()"
        return f"{fn}()"
    elif ntype in ("K2Node_VariableGet", "K2Node_VariableSet"):
        var = props.get("meta.variableName") or title
        self_ctx = props.get("meta.isSelfContext", "") == "true"
        prefix = "self." if self_ctx else ""
        if ntype == "K2Node_VariableGet":
            return f"GET {prefix}{var}"
        return f"SET {prefix}{var}"
    elif ntype == "K2Node_IfThenElse":
        # find condition source
        cond_pin_id = props.get("meta.branchConditionPinId", "")
        cond_src = ""
        for d in data_map.get(guid, []):
            if d["tgtPin"].lower() in ("condition", "b"):
                src = nmap.get(d["srcNode"], {})
                cond_src = node_label(src, nmap, {}) if src else "?"
                break
        return f"BRANCH({cond_src or 'cond'})"
    elif ntype == "K2Node_MacroInstance":
        macro = props.get("meta.macroGraphName") or title
        return f"MACRO:{macro}"
    elif ntype == "K2Node_Event":
        return f"EVENT:{props.get('meta.eventName') or title}"
    elif ntype == "K2Node_CustomEvent":
        return f"EVENT:{title}"
    elif ntype == "K2Node_FunctionEntry":
        return f"ENTRY:{title}"
    elif ntype == "K2Node_FunctionResult":
        return "RETURN"
    elif ntype == "K2Node_ExecutionSequence":
        return "SEQUENCE"
    elif ntype == "K2Node_Knot":
        return None  # transparent reroute — skip
    elif ntype == "K2Node_SpawnActorFromClass":
        return f"SpawnActor({title.replace('SpawnActor ', '')})"
    elif ntype == "K2Node_CreateDelegate":
        return f"CreateDelegate({props.get('meta.functionName') or title})"
    elif ntype == "K2Node_AssignmentStatement":
        return f"ASSIGN"
    else:
        return title or ntype.replace("K2Node_", "")


def walk_exec_chain(start_guid: str, start_pin: str, nmap: dict,
                    exec_adj: dict, data_map: dict,
                    visited: set, depth: int, max_depth: int = 40) -> list[str]:
    """Recursively walk exec chain from a node+pin, return lines of pseudo-code."""
    if depth > max_depth:
        return ["  " * depth + "..."]
    lines = []
    key = (start_guid, start_pin)
    nexts = exec_adj.get(key, [])
    for (tgt_guid, tgt_pin) in nexts:
        if tgt_guid in visited:
            lines.append("  " * depth + f"→ (loop back)")
            continue
        visited.add(tgt_guid)
        node = nmap.get(tgt_guid)
        if not node:
            continue
        label = node_label(node, nmap, data_map)
        if label is None:
            # Knot — follow through transparently
            sub = walk_exec_chain(tgt_guid, "Output", nmap, exec_adj, data_map, visited, depth, max_depth)
            lines.extend(sub)
            continue

        indent = "  " * depth
        ntype = node.get("nodeType", "")

        if ntype == "K2Node_IfThenElse":
            lines.append(f"{indent}if {label.replace('BRANCH(','').rstrip(')')}:")
            then_lines = walk_exec_chain(tgt_guid, "then", nmap, exec_adj, data_map, set(visited), depth + 1, max_depth)
            else_lines = walk_exec_chain(tgt_guid, "else", nmap, exec_adj, data_map, set(visited), depth + 1, max_depth)
            if then_lines:
                lines.extend(then_lines)
            else:
                lines.append(f"{indent}  (no-op)")
            if else_lines:
                lines.append(f"{indent}else:")
                lines.extend(else_lines)
        elif ntype == "K2Node_ExecutionSequence":
            lines.append(f"{indent}SEQUENCE:")
            for i in range(10):
                pin = f"Then {i}"
                sub = walk_exec_chain(tgt_guid, pin, nmap, exec_adj, data_map, set(visited), depth + 1, max_depth)
                if sub:
                    lines.append(f"{indent}  [{i}]:")
                    lines.extend(sub)
                else:
                    break
        else:
            lines.append(f"{indent}→ {label}")
            # Recurse on 'then' or default exec out pin
            out_pins = ["then", "Completed", "Out", "execute"]
            for pin in out_pins:
                sub = walk_exec_chain(tgt_guid, pin, nmap, exec_adj, data_map, visited, depth, max_depth)
                if sub:
                    lines.extend(sub)
                    break

    return lines


def graph_pseudocode(graph: dict) -> list[str]:
    """Convert one structuredGraph to human-readable pseudo-code lines."""
    nodes = graph.get("nodes", [])
    flows = graph.get("flows", {})
    nmap = build_node_map(nodes)
    exec_adj = build_exec_adjacency(flows)
    data_map = build_data_map(flows)

    # Find entry points: Event nodes, CustomEvent, FunctionEntry
    entry_types = {"K2Node_Event", "K2Node_CustomEvent", "K2Node_FunctionEntry"}
    entries = [n for n in nodes if n.get("nodeType") in entry_types]

    if not entries:
        return []

    lines = []
    for entry in entries:
        guid = entry["nodeGuid"]
        label = node_label(entry, nmap, data_map)
        lines.append(f"**{label}**")
        visited = {guid}
        chain = walk_exec_chain(guid, "then", nmap, exec_adj, data_map, visited, 1)
        if not chain:
            # try 'execute' out
            chain = walk_exec_chain(guid, "execute", nmap, exec_adj, data_map, visited, 1)
        if chain:
            lines.extend(chain)
        else:
            lines.append("  (no exec output)")
        lines.append("")

    return lines


# ---------------------------------------------------------------------------
# Spec generation for one Blueprint
# ---------------------------------------------------------------------------

def generate_spec(data: dict) -> str:
    lines = []

    name = data.get("blueprintName", "Unknown")
    bp_path = data.get("blueprintPath", "")
    parent = data.get("parentClassName", "")
    parent_path = data.get("parentClassPath", "")
    category = data.get("blueprintCategory", "")
    description = data.get("blueprintDescription", "")
    is_interface = data.get("isInterface", False)
    is_macro_lib = data.get("isMacroLibrary", False)
    is_func_lib = data.get("isFunctionLibrary", False)
    interfaces = data.get("implementedInterfaces", []) or []
    interface_paths = data.get("implementedInterfacePaths", []) or []
    total_nodes = data.get("totalNodeCount", 0)

    bp_type = "Interface" if is_interface else ("MacroLibrary" if is_macro_lib else
              ("FunctionLibrary" if is_func_lib else "Blueprint"))

    # --- Header ---
    lines.append(f"# {name}")
    lines.append("")
    meta_parts = [f"**Type:** {bp_type}"]
    if category:
        meta_parts.append(f"**Category:** {category}")
    if parent:
        parent_short = parent.split(".")[-1] if "." in parent else parent
        meta_parts.append(f"**Parent:** {parent_short}")
    if bp_path:
        lines.append(f"**Path:** `{bp_path}`")
    lines.append("  ".join(meta_parts))
    if description:
        lines.append(f"> {description}")
    if interfaces:
        iface_list = []
        for iface in interfaces:
            short = iface.split(".")[-1] if "." in iface else iface
            iface_list.append(short)
        lines.append(f"**Implements:** {', '.join(iface_list)}")
    lines.append("")

    # --- Variables ---
    dvars = data.get("detailedVariables", []) or []
    if dvars:
        lines.append("## Variables")
        lines.append("")
        lines.append("| Name | Type | Default | Replicated | Access | Specifiers |")
        lines.append("|------|------|---------|------------|--------|------------|")
        for v in dvars:
            vname = v.get("name", "")
            vtype = type_str(v)
            default = v.get("defaultValue", "") or ""
            if default in ("None", "0", "false", "()", ""):
                default = ""
            repl = repl_str(v)
            access = "public" if v.get("isPublic") else ("editable" if v.get("isEditable") else "private")
            specs = ", ".join(s for s in (v.get("declarationSpecifiers") or [])
                              if s not in ("BlueprintReadWrite", "BlueprintReadOnly",
                                           "EditDefaultsOnly", "EditAnywhere", "EditInstanceOnly",
                                           "VisibleDefaultsOnly", "VisibleAnywhere"))
            # Keep only the important category specifier if present
            cat_spec = next((s for s in (v.get("declarationSpecifiers") or []) if s.startswith("Category=")), "")
            tooltip = v.get("tooltip", "") or ""
            row_parts = [vname, vtype, default[:40] if default else "", repl, access,
                         (cat_spec or specs)[:50]]
            lines.append("| " + " | ".join(row_parts) + " |")
        lines.append("")

    # --- Functions ---
    dfuncs = data.get("detailedFunctions", []) or []
    if dfuncs:
        lines.append("## Functions")
        lines.append("")
        lines.append("| Name | Access | Network | Return | Params | Flags |")
        lines.append("|------|--------|---------|--------|--------|-------|")
        for fn in dfuncs:
            fname = fn.get("name", "")
            access = fn.get("accessSpecifier", "") or ("public" if fn.get("isPublic") else
                     "protected" if fn.get("isProtected") else "private")
            net = net_str(fn)
            ret = fn.get("returnType", "void") or "void"
            # Return type short form
            ret_obj = fn.get("returnTypeObjectPath", "") or ""
            if ret_obj and ret_obj != "None":
                ret_short = ret_obj.split(".")[-1] if "." in ret_obj else ret_obj.split("/")[-1]
                ret = ret_short + ("*" if "object" in ret.lower() else "")
            params_in = fn.get("detailedInputParams") or fn.get("inputParameters") or []
            params_str = ""
            if isinstance(params_in, list) and params_in:
                if isinstance(params_in[0], dict):
                    params_str = param_str(params_in)
                else:
                    params_str = ", ".join(str(p) for p in params_in[:4])
            flags = []
            if fn.get("isPure"): flags.append("Pure")
            if fn.get("isStatic"): flags.append("Static")
            if fn.get("isOverride"): flags.append("Override")
            if fn.get("callsParent"): flags.append("Super")
            if fn.get("isLatent"): flags.append("Latent")
            if fn.get("callInEditor"): flags.append("CallInEditor")
            lines.append(f"| {fname} | {access} | {net} | {ret} | {params_str[:60]} | {', '.join(flags)} |")
        lines.append("")

    # --- Components ---
    dcomps = data.get("detailedComponents", []) or []
    if dcomps:
        lines.append("## Components")
        lines.append("")
        lines.append("| Name | Class | Root | Inherited | Asset |")
        lines.append("|------|-------|------|-----------|-------|")
        for c in dcomps:
            cname = c.get("name", "")
            cls = (c.get("class") or "").split(".")[-1]
            is_root = "✓" if c.get("isRootComponent") else ""
            is_inh = "✓" if c.get("isInherited") else ""
            # Find asset reference in properties
            asset = ""
            props = c.get("properties") or {}
            if isinstance(props, dict):
                for k, v in props.items():
                    if "asset" in k.lower() or "mesh" in k.lower() or "texture" in k.lower():
                        asset = str(v)[:40]
                        break
            lines.append(f"| {cname} | {cls} | {is_root} | {is_inh} | {asset} |")
        lines.append("")

    # --- Timelines ---
    timelines = data.get("timelines", []) or []
    if timelines:
        lines.append("## Timelines")
        lines.append("")
        for tl in timelines:
            tname = tl.get("name", "")
            length = tl.get("length", "")
            looping = " (looping)" if tl.get("looping") else ""
            tracks = tl.get("tracks") or []
            lines.append(f"- **{tname}** — length={length}{looping}, tracks={len(tracks)}")
        lines.append("")

    # --- User-Defined Structs ---
    structs = data.get("userDefinedStructSchemas", []) or []
    if structs:
        lines.append("## User-Defined Structs")
        lines.append("")
        for s in structs:
            sname = s.get("name") or s.get("structName", "")
            fields = s.get("fields") or s.get("members") or []
            lines.append(f"**{sname}** ({len(fields)} fields)")
            for f in fields[:8]:
                fname2 = f.get("name", "")
                ftype = f.get("type") or f.get("typeCategory", "")
                fdefault = f.get("defaultValue", "") or ""
                ftooltip = f.get("tooltip", "") or ""
                lines.append(f"  - {fname2}: {ftype}" + (f" = {fdefault}" if fdefault and fdefault != "None" else "") +
                              (f" // {ftooltip}" if ftooltip else ""))
            if len(fields) > 8:
                lines.append(f"  - ... ({len(fields) - 8} more)")
        lines.append("")

    # --- User-Defined Enums ---
    enums = data.get("userDefinedEnumSchemas", []) or []
    if enums:
        lines.append("## User-Defined Enums")
        lines.append("")
        for e in enums:
            ename = e.get("name") or e.get("enumName", "")
            values = e.get("values") or e.get("enumerators") or []
            lines.append(f"**{ename}**: {', '.join(str(v.get('name', v) if isinstance(v, dict) else v) for v in values[:12])}")
            if len(values) > 12:
                lines.append(f"  ... ({len(values) - 12} more)")
        lines.append("")

    # --- Event Graph / Function Graphs ---
    sgs = data.get("structuredGraphs", []) or []
    if sgs:
        lines.append("## Logic Graphs")
        lines.append("")
        # Sort: function graphs first (they have names), then ubergraph
        named = [g for g in sgs if g.get("name") and g.get("graphType") != "Ubergraph"]
        uber = [g for g in sgs if not g.get("name") or g.get("graphType") == "Ubergraph"]

        for g in named + uber:
            gname = g.get("name") or "EventGraph"
            gtype = g.get("graphType", "")
            node_count = len(g.get("nodes", []))
            flows = g.get("flows", {})
            exec_count = len(flows.get("execution", []))

            lines.append(f"### {gname} ({gtype}, {node_count} nodes, {exec_count} exec edges)")
            lines.append("")

            pseudo = graph_pseudocode(g)
            if pseudo:
                lines.append("```")
                lines.extend(pseudo[:80])  # cap at 80 lines per graph
                if len(pseudo) > 80:
                    lines.append(f"... ({len(pseudo) - 80} more lines)")
                lines.append("```")
            else:
                # Fallback: list node types if no exec chain
                node_types = {}
                for n in g.get("nodes", []):
                    nt = n.get("nodeType", "Unknown")
                    node_types[nt] = node_types.get(nt, 0) + 1
                if node_types:
                    lines.append("_No exec chain — node type summary:_")
                    for nt, cnt in sorted(node_types.items(), key=lambda x: -x[1])[:10]:
                        lines.append(f"- {nt}: {cnt}")
            lines.append("")

    # --- Interfaces implemented (detail) ---
    if interfaces and interface_paths:
        lines.append("## Interfaces Detail")
        lines.append("")
        for iface, ipath in zip(interfaces, interface_paths):
            short = iface.split(".")[-1] if "." in iface else iface
            lines.append(f"- `{short}` — `{ipath}`")
        lines.append("")

    # --- Dependency Closure ---
    dep = data.get("dependencyClosure", {}) or {}
    if isinstance(dep, dict):
        includes = dep.get("includeHints") or dep.get("includes") or []
        modules = dep.get("moduleNames") or dep.get("modules") or []
    elif isinstance(dep, list):
        includes = dep
        modules = []
    else:
        includes = []
        modules = []

    if includes or modules:
        lines.append("## C++ Dependencies")
        lines.append("")
        if modules:
            lines.append(f"**Modules:** {', '.join(str(m) for m in modules[:12])}")
        if includes:
            lines.append("**Includes:**")
            for inc in includes[:20]:
                lines.append(f"  - `{inc}`")
            if len(includes) > 20:
                lines.append(f"  - ... ({len(includes) - 20} more)")
        lines.append("")

    # --- Replication summary (if any replicated vars) ---
    repl_vars = [v for v in dvars if v.get("isReplicated")]
    if repl_vars:
        lines.append("## Replication")
        lines.append("")
        for v in repl_vars:
            vname = v.get("name", "")
            vtype = type_str(v)
            cond = v.get("replicationCondition", "None") or "None"
            notify = v.get("repNotifyFunction", "None") or "None"
            notify_str = f" → `{notify}()`" if notify != "None" else ""
            lines.append(f"- `{vname}` ({vtype}) — {cond}{notify_str}")
        lines.append("")

    # --- Network functions ---
    net_funcs = [fn for fn in dfuncs if fn.get("isNet") or fn.get("isNetServer") or
                 fn.get("isNetClient") or fn.get("isNetMulticast")]
    if net_funcs:
        lines.append("## Network Functions")
        lines.append("")
        for fn in net_funcs:
            fname = fn.get("name", "")
            net = net_str(fn)
            lines.append(f"- `{fname}()` — {net}")
        lines.append("")

    # --- Stats footer ---
    lines.append("---")
    lines.append(f"_Extracted by BlueprintSerializer v{data.get('plugin_version','?')} · "
                 f"{total_nodes} total nodes · {len(sgs)} graphs · "
                 f"{len(dvars)} variables · {len(dfuncs)} functions · {len(dcomps)} components_")

    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Index generation
# ---------------------------------------------------------------------------

SYSTEM_PATTERNS = [
    ("AbilitySystem", r"ability|lyraability|gameplayability|GAS|lyragameplayability"),
    ("Animation",     r"^ABP_|AnimLayer|AnimBP|^Anim"),
    ("Weapons",       r"[Ww]eapon|[Gg]un|[Pp]istol|[Rr]ifle|[Ss]hotgun|[Aa]mmo|[Pp]rojectile"),
    ("Equipment",     r"[Ee]quipment|[Gg]ear"),
    ("Inventory",     r"[Ii]nventory|[Ii]tem(?!AnimLayer)"),
    ("Character",     r"[Cc]haracter|[Mm]annequin|[Hh]ero|[Pp]layer(?!Controller|State)"),
    ("Input",         r"[Ii]nput|[Cc]ontrol"),
    ("UI",            r"[Ww]idget|HUD|UI_|W_"),
    ("AI",            r"^AI|AITest|[Bb]ot"),
    ("GameMode",      r"[Gg]ame[Mm]ode|[Gg]ame[Ss]tate|[Ee]xperience"),
    ("Teams",         r"[Tt]eam"),
    ("Messages",      r"[Mm]essage|[Nn]otif"),
    ("Audio",         r"[Aa]udio|[Ss]ound"),
    ("Effects",       r"[Ee]ffect|[Pp]article|[Nn]iagara"),
    ("Camera",        r"[Cc]amera"),
    ("Physics",       r"[Pp]hysics|[Cc]ollision"),
]


def classify_system(name: str) -> str:
    for sys_name, pattern in SYSTEM_PATTERNS:
        if re.search(pattern, name):
            return sys_name
    return "Other"


def generate_index(entries: list[dict]) -> str:
    """entries: list of {name, path, parent, system, var_count, func_count, comp_count}"""
    lines = []
    lines.append("# Lyra Blueprint Specs — Index")
    lines.append("")
    lines.append(f"**{len(entries)} Blueprints** extracted from the Lyra corpus (485 BP files).")
    lines.append("")
    lines.append("_Generated by LYRA-PRIME Phase 1b spec generator._")
    lines.append("")

    by_system = defaultdict(list)
    for e in entries:
        by_system[e["system"]].append(e)

    # Sort systems by count descending
    for sys_name in sorted(by_system.keys(), key=lambda s: -len(by_system[s])):
        group = sorted(by_system[sys_name], key=lambda e: e["name"])
        lines.append(f"## {sys_name} ({len(group)})")
        lines.append("")
        lines.append("| Blueprint | Parent | Vars | Funcs | Comps |")
        lines.append("|-----------|--------|------|-------|-------|")
        for e in group:
            parent_short = e["parent"].split(".")[-1] if "." in e["parent"] else e["parent"]
            lines.append(f"| [{e['name']}]({e['name']}.md) | {parent_short} | "
                         f"{e['var_count']} | {e['func_count']} | {e['comp_count']} |")
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
    # Determine project root (4 levels up from Scripts/)
    script_dir = Path(__file__).resolve().parent
    plugin_dir = script_dir.parent
    project_root = plugin_dir.parent.parent

    # Parse args
    if len(sys.argv) >= 2:
        export_dir = Path(sys.argv[1])
    else:
        export_dir = find_latest_export_dir(project_root)
        if not export_dir:
            print("ERROR: No export directory found. Run the regression suite first or pass a path.")
            sys.exit(1)

    if len(sys.argv) >= 3:
        output_dir = Path(sys.argv[2])
    else:
        output_dir = project_root / "Saved" / "BlueprintSpecs"

    output_dir.mkdir(parents=True, exist_ok=True)

    json_files = sorted(export_dir.glob("BP_SLZR_Blueprint_*.json"))
    if not json_files:
        print(f"ERROR: No Blueprint JSON files found in {export_dir}")
        sys.exit(1)

    print(f"LYRA-PRIME Spec Generator")
    print(f"  Input:  {export_dir} ({len(json_files)} files)")
    print(f"  Output: {output_dir}")
    print()

    index_entries = []
    ok = 0
    fail = 0

    for jf in json_files:
        try:
            raw = jf.read_bytes()
            if raw.startswith((b"\xff\xfe", b"\xfe\xff")):
                text = raw.decode("utf-16")
            else:
                text = raw.decode("utf-8-sig")
            data = json.loads(text)

            name = data.get("blueprintName", jf.stem)
            spec_md = generate_spec(data)
            out_path = output_dir / f"{name}.md"
            out_path.write_text(spec_md, encoding="utf-8")

            dvars = data.get("detailedVariables") or []
            dfuncs = data.get("detailedFunctions") or []
            dcomps = data.get("detailedComponents") or []

            index_entries.append({
                "name": name,
                "path": data.get("blueprintPath", ""),
                "parent": data.get("parentClassName", ""),
                "system": classify_system(name),
                "var_count": len(dvars),
                "func_count": len(dfuncs),
                "comp_count": len(dcomps),
            })

            ok += 1
            if ok % 50 == 0:
                print(f"  {ok}/{len(json_files)} processed...")

        except Exception as e:
            print(f"  WARN: failed {jf.name}: {e}")
            fail += 1

    # Write index
    index_md = generate_index(index_entries)
    (output_dir / "_INDEX.md").write_text(index_md, encoding="utf-8")

    print()
    print(f"Done: {ok} specs written, {fail} failures.")
    print(f"Index: {output_dir / '_INDEX.md'}")
    print(f"Specs: {output_dir}/")


if __name__ == "__main__":
    main()
