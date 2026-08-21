# BlueprintSerializer — Universal Core Context

> **This file is the single source of truth for any AI agent (Claude, ChatGPT, Gemini, Copilot, or any other) working on the BlueprintSerializer plugin.**
> All agent config files (`AGENTS.md`, `CLAUDE.md`, `GEMINI.md`, etc.) point here.
> Read this entire file before touching any code or running any build command.

---

## What This Plugin Is

BlueprintSerializer is an Unreal Engine 5 editor plugin that serializes Blueprint assets to structured JSON for automated C++ conversion. Its primary mandate is **C++ conversion fidelity** — producing a complete IR (intermediate representation) that contains every piece of information needed to generate a correct, idiomatic C++ header AND implementation from a Blueprint class.

**Current status (v1.0-converter-ready, 2026-02-21):** The IR is complete. A downstream codegen tool that reads this JSON and emits `.h`/`.cpp` text is the next layer — it does not live in this plugin. Do not confuse "the codegen tool doesn't exist yet" with "the IR is insufficient." The IR has been audited and verified sufficient (see IR Completeness section below).

**Plugin location:** `Plugins/BlueprintSerializer/`
**Live work queue:** `gh issue list --repo Jinphinity/BlueprintSerializer --label implementation`
**Regression baseline:** `REGRESSION_BASELINE.json`
**Historical archive:** `ARCHIVE.md` (read-only; replaces CONVERTER_READY_TODO.md, REGRESSION_LOG.md, and spec docs)

---

## 🔴 CRITICAL: How to Build This Plugin — Read Before Every Build

### Plugins Have No Standalone Build Target. Always Build the Host Project.

A UE plugin's `.uplugin` file declares modules but defines **no build target**.
UBT requires a `.Target.cs` file to know the root of the build graph.
That file lives in the **host game project**, not in the plugin.

```
✅ CORRECT:
Build.bat <ProjectName>Editor Win64 Development "<ProjectName>.uproject"

❌ ALWAYS FAILS — plugins have no .Target.cs:
Build.bat BlueprintSerializerEditor Win64 Development ...
Build.bat BlueprintSerializer Win64 Development ...
```

The plugin DLL (`UnrealEditor-BlueprintSerializer.dll`) is produced as a
side-effect of building the host editor target. It is never built on its own.

### How to Derive the Target Name for Any Host Project

1. Look in `<ProjectRoot>/Source/` for any file matching `*.Target.cs`
2. The filename without `.Target.cs` is the base target name
3. Append `Editor` for editor builds

**Examples:**
| `.Target.cs` file found | Correct build target |
|---|---|
| `Source/LyraEditor.Target.cs` | `LyraEditor` |
| `Source/MyGameEditor.Target.cs` | `MyGameEditor` |
| `Source/ShooterEditor.Target.cs` | `ShooterEditor` |

### Full Windows Command Pattern

```bash
powershell -Command "& 'C:\\Program Files\\Epic Games\\UE_5.X\\Engine\\Build\\BatchFiles\\Build.bat' \
  <ProjectName>Editor Win64 Development \
  '\"<AbsolutePath>\\<ProjectName>.uproject\"' \
  -waitmutex 2>&1"
```

For the current Lyra project specifically:
```bash
powershell -Command "& 'C:\\Program Files\\Epic Games\\UE_5.6\\Engine\\Build\\BatchFiles\\Build.bat' \
  LyraEditor Win64 Development \
  '\"C:\\Users\\jinph\\Documents\\Unreal Projects\\LyraStarterGame\\LyraStarterGame.uproject\"' \
  -waitmutex 2>&1"
```

**This rule applies in every project this plugin is ever used in. Do not skip it.**

---

## Regression Workflow (Required After Every Non-Trivial Change)

```
1. Build the host project editor target (see above)
2. Run the regression suite:
   powershell -ExecutionPolicy Bypass -File Plugins/BlueprintSerializer/Scripts/Run-RegressionSuite.ps1
3. Wait for suitePass=True in the output
4. Verify artifact files exist:
   Saved/BlueprintExports/BP_SLZR_RegressionRun_<timestamp>.json      ← suitePass field
   Saved/BlueprintExports/BP_SLZR_All_<timestamp>/BP_SLZR_ValidationReport_<timestamp>.json
   Saved/BlueprintExports/BP_SLZR_AnimCurveAudit_<timestamp>.json
5. Update REGRESSION_BASELINE.json if new metrics were added
6. Close GitHub Issue with metric evidence: `gh issue close <N> --repo Jinphinity/BlueprintSerializer --comment "..."`
7. Commit plugin + superproject
```

**Never claim success from shell exit code alone.** Always verify `suitePass=True` in the JSON artifact.
**Never close a GitHub Issue without corpus evidence from a passing regression run.**

Harness internals are documented in `ARCHIVE.md` (Part 5).

---

## Key Source Files

| File | Role |
|---|---|
| `Source/BlueprintSerializer/Private/BlueprintAnalyzer.cpp` | Core extraction logic — `AnalyzeNodeToStruct`, `IsNodeTypeKnownSupported`, all K2Node handlers |
| `Source/BlueprintSerializer/Public/BlueprintAnalyzer.h` | Data structs: `FBS_NodeData`, `FBS_FunctionInfo`, `FBS_ParamInfo`, `FBS_LocalVarInfo`, etc. |
| `Source/BlueprintSerializer/Private/BlueprintExtractorCommands.cpp` | Console commands, regression suite orchestration, validator metric counters |
| `Source/BlueprintSerializer/BlueprintSerializer.Build.cs` | Module dependencies — add new external module headers here |
| `Scripts/Run-RegressionSuite.ps1` | Host-side regression harness (launches UE, waits, closes, reports) |
| `REGRESSION_BASELINE.json` | Minimum metric thresholds — gates that must pass every run |
| `ARCHIVE.md` | Read-only historical record (replaces REGRESSION_LOG.md, CONVERTER_READY_TODO.md, spec docs) |

---

## IR Completeness — What structuredGraphs Actually Contains

**DO NOT assume function body generation is impossible. The IR is complete. Verify against data before claiming gaps.**

Every node in `structuredGraphs[].nodes[]` carries:
- `nodeType` — exact K2Node class name (e.g. `K2Node_CallFunction`)
- `title` — display name
- `nodeProperties` — `TMap<FString,FString>` of all `meta.*` enrichment fields (see below)
- `pins[]` — full pin data: name, direction, category, objectPath, defaultValue, connected, etc.

Schema 1.7 also requires the legacy JSON-string `graphNodes[]` surface to contain
the exact same reachable node GUID set as `structuredGraphs[].nodes[]`. Both surfaces
recursively traverse every `UK2Node_Composite::BoundGraph` with duplicate/cycle guards.

Raw `UFunction::Script` MD5 values are exposed only as
`rawInMemoryBytecodeDiagnostic` with `canonical=false`; engine bytecode buffers embed
process-local name IDs and object/field addresses and therefore cannot prove change,
equivalence, or deterministic reconstruction.

Exec edges in `structuredGraphs[].flows.exec[]`:
- `sourceNodeGuid + sourcePinName → targetNodeGuid + targetPinName`
- Branch pins are labeled `'then'/'else'` or `'True'/'False'`; sequence nodes use `'Then 0'`, `'Then 1'`; fully unambiguous

Data edges in `structuredGraphs[].flows.data[]`:
- `sourceNodeGuid + sourcePinName → targetNodeGuid + targetPinName + pinCategory + pinSubCategoryObjectPath`
- Unconnected input pins carry `defaultValue` / `defaultObjectPath` directly on the pin object

### What Each Key Node Type Contains

| Node Type | Key meta.* fields | What you can generate |
|---|---|---|
| `K2Node_CallFunction` | `meta.functionName`, `meta.functionOwner`, `meta.isStatic`, `meta.isLatent`, `meta.isPure`, `meta.bytecodeCorrelation` | `Target->FunctionName(args)` or `UClass::StaticFunc(args)` |
| `K2Node_VariableGet` | `meta.variableName`, `meta.isSelfContext` | `this->VarName` or local variable read |
| `K2Node_VariableSet` | `meta.variableName`, `meta.isSelfContext` | `this->VarName = expr` |
| `K2Node_IfThenElse` | `meta.branchConditionPinId`, `meta.branchTruePinId`, `meta.branchFalsePinId` | `if (cond) { ... } else { ... }` |
| `K2Node_MacroInstance` | `meta.macroGraphName`, `meta.macroGraphPath`, `meta.macroBlueprintPath` | Inline expansion or opaque call |
| `K2Node_SpawnActorFromClass` | function + class pin data edge | `GetWorld()->SpawnActor<T>(Class, Transform)` |
| `K2Node_CreateDelegate` | function name + target object data edge | `FDelegate::CreateUObject(this, &ThisClass::Func)` |
| `K2Node_ExecutionSequence` | exec pins labeled `'Then 0'`, `'Then 1'`, … | Sequential statements |
| `K2Node_Knot` | passthrough reroute — ignore, follow data edge | (transparent) |

**Concrete B_Weapon evidence (audit_graph_ir.py, 2026-02-21):**
```
K2Node_CallFunction 'Attach Actor To Component':
  meta.functionName        = 'K2_AttachToComponent'
  meta.functionOwner       = '/Script/Engine.Actor'
  meta.isStatic            = 'false'
  meta.isLatent            = 'false'
  meta.bytecodeCorrelation = 'Actor::K2_AttachToComponent'
  → all argument pins wired via data edges or have defaultValue on the pin

K2Node_IfThenElse 'Branch':
  meta.branchConditionPinId = '79C77A09...'
  exec 'then' → K2Node_CallFunction('Add Fake Projectile Data')
  exec 'else' → K2Node_Knot('Reroute Node')
  CONDITION SOURCE: K2Node_VariableGet('NeedsFakeProjectileData') [bool]

K2Node_VariableGet 'SkeletalMesh':
  meta.variableName  = 'SkeletalMesh'
  meta.isSelfContext = 'true'
  → emit: this->SkeletalMesh
```

**B_Weapon has zero unsupported nodes.** `compilerIRFallback.hasUnsupportedNodes = false`.

The plugin's job (producing this IR) is done. The next layer is a **codegen consumer** — a separate tool that reads this JSON and emits `.h`/`.cpp` text. That tool does not live in this plugin.

---

## Key Architectural Patterns

### IsNodeTypeKnownSupported()
Static `TSet<FName>` in `BlueprintAnalyzer.cpp`. Classifies a node type as "supported" so it does not fall through to the unsupported-node fallback path. Add new node type names here when adding a handler.

### AnalyzeNodeToStruct()
Per-node enrichment function. Uses `Cast<UK2Node_Foo>(K2)` blocks to populate `meta.*` fields on the node's `nodeProperties` JSON object. This is where all K2Node-specific metadata lives.

### Accessing Protected/Private UE Members
Many K2Node UPROPERTYs are declared `protected:` or in private headers. The established workaround is UE reflection:
```cpp
// For a protected FName UPROPERTY named "SomeField":
if (FNameProperty* P = FindFProperty<FNameProperty>(Node->GetClass(), TEXT("SomeField")))
{
    FName Val = P->GetPropertyValue_InContainer(Node);
    if (!Val.IsNone()) AddMeta(TEXT("meta.someField"), Val.ToString());
}

// For a protected TSubclassOf<> / FClassProperty:
if (FClassProperty* P = FindFProperty<FClassProperty>(Node->GetClass(), TEXT("SomeClass")))
{
    UClass* Val = Cast<UClass>(P->GetObjectPropertyValue_InContainer(Node));
    if (Val) AddMeta(TEXT("meta.someClass"), Val->GetPathName());
}
```

### FMemberReference — GetMemberParentPackage() Returns UPackage*, Not FName
```cpp
// ✅ CORRECT:
if (UPackage* Pkg = Ref.GetMemberParentPackage())
    AddMeta(TEXT("meta.ownerPackage"), Pkg->GetName());

// ❌ WRONG — pointer does not have .IsNone() or .ToString():
if (!Ref.GetMemberParentPackage().IsNone()) ...
```

### Adding a New Module Dependency
Edit `BlueprintSerializer.Build.cs` → `PrivateDependencyModuleNames`. Then rebuild. Common modules needed for node type headers:
- `InputCore` — FKey methods (GetFName, GetDisplayName)
- `GameplayAbilitiesEditor` — K2Node_LatentAbilityCall
- `GameplayTasksEditor` — K2Node_LatentGameplayTaskCall
- `InputBlueprintNodes` — K2Node_EnhancedInputAction
- `EnhancedInput` — UInputAction full definition

---

## Regression Baseline (Current)

485 Blueprint files, 0 parse errors, 14 validation gates (all must pass).

Key corpus counts as of 2026-02-20 (run BP_SLZR_RegressionRun_20260220_144944):
- `nodesWithTunnel`: 1949
- `nodesWithPromotableOp`: 1000
- `nodesWithBranch`: 1054, `nodesWithReroute`: 2949
- `nodesWithSetPersistentFrame`: 341
- `nodesWithCallMaterialParamCollection`: 209
- `nodesWithGetArrayItem`: 209, `nodesWithAddComponent`: 199
- `nodesWithPropertyAccess`: 85, `nodesWithBaseAsyncTask`: 73
- `nodesWithGetSubsystem`: 72, `nodesWithMakeArray`: 71
- `nodesWithMultiGate`: 1
- `variablesTotal`: 3133, `functionsTotal`: 936, `componentsTotal`: 818
- `exportsWithUnsupportedNodeFallback`: 0
- `variablesWithDeclarationSpecifiers`: 3133, `functionsWithDeclarationSpecifiers`: 936
- `nativeIncludeHintsNonEmpty`: 485
- `userStructSchemaNonEmpty`: 42 (with DefaultValue/DisplayName/Tooltip per field)
- `userEnumSchemaNonEmpty`: 53 (with DisplayName/Tooltip per value)
- `structuredGraphsTotal`: 2342, `structuredGraphNodesTotal`: 37365

See `REGRESSION_BASELINE.json` for the full and authoritative list.

---

## Work Priority

1. **C++ conversion fidelity** — extraction completeness, node meta enrichment, dependency closure
2. **Regression gate coverage** — new gates for newly emitted fields
3. **Animation/editor-heavy work** — human-led; automation assists but does not lead

Work queue is tracked in GitHub Issues. Use `gh issue list --repo Jinphinity/BlueprintSerializer --label implementation --state open` to find active items. Historical context is in `ARCHIVE.md`.

---

## Status Language Definitions

| Term | Meaning |
|---|---|
| `done` | Implemented AND validated with corpus evidence (`suitePass=True`) |
| `in_progress` | Implemented but validation not yet complete |
| `blocked` | Validation blocked by corpus or tooling constraints (not a code failure) |

---

## Context Engineering System — Spec Quality & Self-Improving Loops

The BlueprintSerializer plugin feeds into the PRIME DIRECTIVE pipeline: the JSON IR it
produces is consumed by AI agents to generate per-Blueprint markdown specs. The spec
quality system lives in `ContextSystem/` and uses a three-loop autonomous protocol.

**Any AI receiving this context must run all applicable loops. The loops are mandatory.**

```
LOOP A — Extraction Regression (run after any plugin code change)
  Goal: all 14 gates pass in REGRESSION_BASELINE.json
  Script: Scripts/Run-RegressionSuite.ps1
  See: ContextSystem/ITERATION_LOOP.md — "LOOP A" section

LOOP B — Spec Quality (run when L1 pass rate < 95%)
  Goal: L1 ≥ 95%, L2 ≥ 90%, L3 ≥ 85%
  Tool: Scripts/validate_specs.py
  See: ContextSystem/ITERATION_LOOP.md — "LOOP B" section

LOOP C — Meta-Improvement (run after every LOOP B cycle)
  Goal: improve the process that improves the specs
  See: ContextSystem/ITERATION_LOOP.md — "LOOP C" section
```

| File | Purpose |
|---|---|
| `ContextSystem/PRIME_DIRECTIVE.md` | The universal mandate — why this exists |
| `ContextSystem/ANALYSIS_PLAYBOOK.md` | 8 CRITICAL RULES for AI spec generation |
| `ContextSystem/ITERATION_LOOP.md` | All three loops — full step-by-step protocol |
| `ContextSystem/QUALITY_GATES.json` | Machine-readable thresholds + loop definitions |
| `ContextSystem/SCHEMA_REFERENCE.md` | JSON IR schema documentation |
| `Scripts/validate_specs.py` | Validates specs against JSON IR (LOOP B) |
| `Scripts/Run-RegressionSuite.ps1` | Extraction regression harness (LOOP A) |
| `REGRESSION_BASELINE.json` | 14-gate pass thresholds for LOOP A |

**Reading order for spec generation work:**
PRIME_DIRECTIVE → SCHEMA_REFERENCE → ANALYSIS_PLAYBOOK → generate specs → run LOOP B → after each B cycle run LOOP C.

---

*Last updated: 2026-02-21. v1.0-converter-ready: all CXX-001-008 goals met, IR audited and confirmed complete for both header and body generation. Added IR Completeness section with concrete node-level evidence to prevent future agents from incorrectly claiming the IR is insufficient. Update this file whenever architectural patterns, module deps, or build procedures change.*
