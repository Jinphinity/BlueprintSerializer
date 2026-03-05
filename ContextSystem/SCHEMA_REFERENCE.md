# BlueprintSerializer JSON Export — Schema Reference

> This document describes every field in the JSON files produced by BlueprintSerializer.
> Use this to understand the data you are reading when producing specs for a UE project.

---

## File Naming

Each file follows: `BP_SLZR_Blueprint_<Name>_<Hash>_<Timestamp>.json`

One file per Blueprint asset in the project.

---

## Top-Level Fields

### Identity

| Field | Type | Description |
|-------|------|-------------|
| `blueprintName` | string | Short name (e.g. "B_Weapon") |
| `blueprintPath` | string | Full asset path (e.g. "/Game/Weapons/B_Weapon.B_Weapon") |
| `parentClassName` | string | Parent C++ or Blueprint class name |
| `parentClassPath` | string | Full class path of parent |
| `generatedClassName` | string | UE-generated class name (e.g. "B_Weapon_C") |
| `blueprintNamespace` | string | Blueprint namespace (usually empty) |
| `blueprintDescription` | string | Author-provided description |
| `blueprintCategory` | string | Author-provided category |

### Type Flags

| Field | Type | Description |
|-------|------|-------------|
| `isInterface` | bool | True if this is a Blueprint Interface |
| `isMacroLibrary` | bool | True if this is a Macro Library |
| `isFunctionLibrary` | bool | True if this is a Function Library |

### Class Specifiers

| Field | Type | Description |
|-------|------|-------------|
| `classSpecifiers` | string[] | UHT class specifiers: Blueprintable, BlueprintType, etc. |
| `classConfigFlags` | object | Config flags: isConfigClass, hasConfigName, etc. |
| `classConfigName` | string | Config category name (e.g. "Engine", "Game") |
| `classDefaultValues` | object | All CDO property defaults (key→string value) |
| `classDefaultValueDelta` | object | Only properties that differ from parent CDO |

### Interfaces

| Field | Type | Description |
|-------|------|-------------|
| `implementedInterfaces` | string[] | Short names of implemented interfaces |
| `implementedInterfacePaths` | string[] | Full paths of implemented interfaces |

---

## `detailedVariables[]` — Variable Declarations

Each entry represents a Blueprint variable (UPROPERTY equivalent).

> **⚠ Trailing-Space Name Artifact:** UE Blueprint metadata occasionally stores variable
> `DisplayName` values with a trailing space (e.g. `"Follow Player "`). These are UE editor
> artifacts preserved faithfully in the IR for reconstruction completeness. The validator
> normalizes names with `rstrip()` before comparison. When writing specs, omit the trailing
> space in table entries and document the artifact in `## IR Notes`. Detect with:
> `name.rstrip() != name`.

| Field | Type | Description |
|-------|------|-------------|
| `name` | string | Variable name (may have trailing-space artifact — see note above) |
| `type` | string | Human-readable type string (e.g. "object (/Script/Engine.Pawn)") |
| `typeCategory` | string | Base type: bool, int, float, object, struct, enum, class, string, text, name, etc. |
| `typeSubCategory` | string | Sub-type info (usually "None") |
| `typeObjectPath` | string | Full path for object/struct/enum types |
| `isArray` | bool | TArray container |
| `isMap` | bool | TMap container |
| `isSet` | bool | TSet container |
| `defaultValue` | string | Default value as string |
| `category` | string | Editor category (e.g. "Weapons", "Movement") |
| `tooltip` | string | Editor tooltip |
| `isPublic` | bool | Public visibility |
| `isEditable` | bool | Editable in editor |
| `isReplicated` | bool | **Network replicated** |
| `replicationCondition` | string | COND_OwnerOnly, COND_SimulatedOnly, etc. |
| `repNotifyFunction` | string | OnRep function name |
| `isExposedOnSpawn` | bool | Exposed on SpawnActor |
| `declarationSpecifiers` | string[] | UPROPERTY specifiers: BlueprintReadWrite, EditAnywhere, Category="X", etc. |

### Reading Variable Types

- `typeCategory` = "object", `typeObjectPath` = "/Script/Engine.Pawn" → `APawn*`
- `typeCategory` = "struct", `typeObjectPath` = "/Script/CoreUObject.Vector" → `FVector`
- `typeCategory` = "bool" → `bool`
- `typeCategory` = "int" → `int32`
- `isArray` = true → wrap in `TArray<>`
- `isMap` = true → `TMap<KeyType, ValueType>`

---

## `detailedFunctions[]` — Function Declarations

Each entry represents a Blueprint function (UFUNCTION equivalent).

> **⚠ Trailing-Space Name Artifact:** Same as variables above — function `DisplayName`
> values may have trailing spaces (e.g. `"Update "`, `"Initialize Size "`). Occasionally
> two entries appear whose names differ only by a trailing space (e.g. `"Update"` and
> `"Update "`) — these are the same function with a metadata duplicate. Both appear in
> `detailedFunctions[]`; the validator deduplicates them after `rstrip()` normalization.

| Field | Type | Description |
|-------|------|-------------|
| `name` | string | Function name (may have trailing-space artifact — see note above) |
| `functionPath` | string | Full path including class (e.g. "B_Weapon_C::Fire") |
| `accessSpecifier` | string | "public", "protected", "private" |
| `isPure` | bool | Pure function (no side effects, no exec pin) |
| `isStatic` | bool | Static function |
| `isOverride` | bool | Overrides parent function |
| `callsParent` | bool | Calls Super implementation |
| `isLatent` | bool | Latent async function |
| `callInEditor` | bool | Callable in editor |
| **Network Flags** | | |
| `isNet` | bool | Has any network behavior |
| `isNetServer` | bool | Server RPC |
| `isNetClient` | bool | Client RPC |
| `isNetMulticast` | bool | Multicast RPC |
| `isReliable` | bool | Reliable RPC |
| `blueprintAuthorityOnly` | bool | Authority-only execution |
| `blueprintCosmetic` | bool | Cosmetic-only execution |
| **Signature** | | |
| `returnType` | string | Return type ("void" if none) |
| `returnTypeObjectPath` | string | Full path for return type |
| `inputParameters` | string[] | Human-readable param strings: "ParamName: type" |
| `outputParameters` | string[] | Output params (by-ref out params) |
| `localVariables` | string[] | Local variable declarations |
| `detailedInputParams` | object[] | Structured: paramName, typeCategory, containerType, etc. |
| `detailedLocalVariables` | object[] | Structured: varName, typeCategory, etc. |
| **Compilation** | | |
| `bytecodeSize` | int | Compiled bytecode size (correlates with function complexity) |
| `bytecodeHash` | string | Bytecode hash (change detection) |
| `declarationSpecifiers` | string[] | UFUNCTION specifiers: BlueprintCallable, BlueprintEvent, etc. |

---

## `detailedComponents[]` — Component Hierarchy

Each entry represents a component in the Blueprint's component tree.

| Field | Type | Description |
|-------|------|-------------|
| `name` | string | Component name |
| `class` | string | Component class path (e.g. "/Script/Engine.SkeletalMeshComponent") |
| `parentComponent` | string | Parent component name in hierarchy |
| `isRootComponent` | bool | Is this the root component |
| `isInherited` | bool | Inherited from parent class |
| `isParentComponentNative` | bool | Parent component defined in C++ |
| `transform` | string | Position, rotation, scale as string |
| `properties` | object | Component property overrides (key→value) |
| `assetReferences` | string[] | Referenced assets (meshes, materials, etc.) |
| `hasInheritableOverride` | bool | Has property overrides from parent |
| `inheritableOverrideProperties` | string[] | Which properties are overridden |
| `inheritableOverrideValues` | object | Override values |
| `inheritableParentValues` | object | Original parent values (for diff) |

---

## `structuredGraphs[]` — Logic Graphs (THE CORE)

This is the most important section. Each entry is a complete Blueprint graph — the actual logic.

### Graph Level

| Field | Type | Description |
|-------|------|-------------|
| `name` | string | Graph name ("EventGraph", function name, etc.) |
| `graphType` | string | "Ubergraph", "Function", "Macro", "AnimGraph", "StateMachine", etc. |
| `nodes` | array | All nodes in the graph |
| `flows` | object | `execution` and `data` edge arrays |

### Node Level (`structuredGraphs[].nodes[]`)

| Field | Type | Description |
|-------|------|-------------|
| `nodeGuid` | string | Unique identifier (used in flow edges) |
| `nodeType` | string | K2Node class: K2Node_CallFunction, K2Node_IfThenElse, etc. |
| `title` | string | Display title (e.g. "SpawnActor B Weapon Fire") |
| `posX`, `posY` | int | Editor position (ignore for logic — cosmetic only) |
| `enabledState` | string | "Enabled", "Disabled" |
| `pins` | array | Input/output pins |
| `nodeProperties` | object | **KEY: meta.* enrichment fields** (see below) |

### Node Properties — The Metadata That Makes Codegen Possible

`nodeProperties` is a key→value dict. Important `meta.*` fields by node type:

**K2Node_CallFunction:**
- `meta.functionName` — the C++ function being called
- `meta.functionOwner` — the class that owns the function
- `meta.isStatic` — "true"/"false"
- `meta.isLatent` — "true"/"false"
- `meta.isPure` — "true"/"false"
- `meta.bytecodeCorrelation` — "Class::FunctionName" form

**K2Node_VariableGet / K2Node_VariableSet:**
- `meta.variableName` — variable being accessed
- `meta.isSelfContext` — "true" = this->Variable, "false" = external

**K2Node_IfThenElse:**
- `meta.branchConditionPinId` — pin ID of condition input
- Exec edges labeled "then" / "else"

**K2Node_MacroInstance:**
- `meta.macroGraphName` — macro name
- `meta.macroGraphPath` — full macro graph path
- `meta.macroBlueprintPath` — Blueprint containing the macro

**K2Node_SpawnActorFromClass:**
- Class pin connected via data edge → the class being spawned

**K2Node_ExecutionSequence:**
- Exec edges labeled "Then 0", "Then 1", etc.

### Pin Level (`structuredGraphs[].nodes[].pins[]`)

| Field | Type | Description |
|-------|------|-------------|
| `name` | string | Pin name (e.g. "execute", "ReturnValue", "Target") |
| `pinId` | string | Unique pin identifier |
| `direction` | string | "Input" or "Output" |
| `category` | string | Type category (exec, object, struct, bool, int, float, etc.) |
| `is_exec` | bool | True for execution pins |
| `connected` | bool | True if wired to another pin |
| `defaultValue` | string | Default value for unconnected input pins |
| `objectPath` | string | Type path for object/struct pins |

### Execution Edges (`structuredGraphs[].flows.execution[]`)

These define the order of execution (the "white wires" in Blueprint).

| Field | Type | Description |
|-------|------|-------------|
| `sourceNodeGuid` | string | Node producing the exec signal |
| `sourcePinName` | string | Output pin name ("then", "else", "Then 0", "Completed", etc.) |
| `targetNodeGuid` | string | Node receiving the exec signal |
| `targetPinName` | string | Input pin name (usually "execute") |

**Reading execution flow:** Start from Event/Entry nodes, follow `sourcePinName` → `targetNodeGuid`.
Branch nodes have multiple named output pins ("then"/"else"). Sequence nodes have "Then 0", "Then 1", etc.

### Data Edges (`structuredGraphs[].flows.data[]`)

These define value flow (the "colored wires" in Blueprint).

| Field | Type | Description |
|-------|------|-------------|
| `sourceNodeGuid` | string | Node producing the value |
| `sourcePinName` | string | Output pin name ("ReturnValue", variable name, etc.) |
| `targetNodeGuid` | string | Node consuming the value |
| `targetPinName` | string | Input pin name (parameter name, "self", etc.) |
| `pinCategory` | string | Value type category |
| `pinSubCategoryObjectPath` | string | Full type path for object/struct types |

**Reading data flow:** For each node, check which data edges target it to understand what values feed into it.
Unconnected input pins carry their value in the pin's `defaultValue` field.

---

## `dependencyClosure` — C++ Dependencies

| Field | Type | Description |
|-------|------|-------------|
| `classPaths` | string[] | C++ class paths referenced |
| `structPaths` | string[] | C++ struct paths referenced |
| `enumPaths` | string[] | C++ enum paths referenced |
| `interfacePaths` | string[] | Interface paths referenced |
| `assetPaths` | string[] | Asset paths referenced |
| `macroGraphPaths` | string[] | Macro graph paths |
| `macroBlueprintPaths` | string[] | Macro Blueprint paths |
| `includeHints` | string[] | **Suggested C++ #include paths** |
| `moduleNames` | string[] | **UE module names needed** |

---

## `timelines[]` — Timeline Components

| Field | Type | Description |
|-------|------|-------------|
| `name` | string | Timeline name |
| `length` | float | Duration in seconds |
| `looping` | bool | Whether it loops |
| `tracks` | array | Track data with keyframes |

---

## `userDefinedStructSchemas[]` and `userDefinedEnumSchemas[]`

Structs and enums defined within this Blueprint.

**Struct fields:** name, type, defaultValue, tooltip, displayName
**Enum values:** name, displayName, tooltip

---

## `compilerIRFallback` — Extraction Completeness

| Field | Type | Description |
|-------|------|-------------|
| `hasUnsupportedNodes` | bool | **True = some nodes couldn't be fully extracted** |
| `unsupportedNodeTypes` | string[] | List of unhandled node types |
| `unsupportedNodeTypeCount` | int | Count of unhandled types |
| `hasBytecodeFallback` | bool | Whether bytecode fallback was used |

**If `hasUnsupportedNodes` is true:** The spec should note which node types are unhandled.
This means the graph pseudo-code will have gaps for those specific nodes.

---

## `coverage` — Node Type Statistics

| Field | Type | Description |
|-------|------|-------------|
| `totalNodeCount` | int | Total nodes across all graphs |
| `nodeTypeCounts` | object | Node type → count map |
| `unsupportedNodeTypes` | string[] | Types not fully handled |
| `partiallySupportedNodeTypes` | string[] | Types with partial handling |

---

## `blueprintInfo` — AnimBP-Specific

| Field | Type | Description |
|-------|------|-------------|
| `isAnimBlueprint` | bool | True if this is an Animation Blueprint |
| `targetSkeleton` | string | Skeleton asset path (for AnimBPs) |

AnimBP exports also contain:
- Animation asset references in `detailedComponents` or dedicated fields
- AnimGraph nodes (AnimGraphNode_Slot, AnimGraphNode_StateMachine, etc.) in `structuredGraphs`
- State machine transitions as graph nodes and edges
