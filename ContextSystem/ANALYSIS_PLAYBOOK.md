# ANALYSIS PLAYBOOK — AI Guide for Blueprint Spec Generation

> This document tells you, an AI agent, exactly how to read raw BlueprintSerializer
> JSON exports and produce per-Blueprint specs. Read this after PRIME_DIRECTIVE.md
> and SCHEMA_REFERENCE.md.

---

## CRITICAL RULES — Spec Validation Compliance

> These rules encode patterns confirmed by the corpus-wide validation run.
> Violating them causes L1/L2/L3 errors. The validator (`validate_specs.py`)
> enforces them automatically. Read these BEFORE you write a single line.

### RULE 1 — Identity Section Is Mandatory
Every spec MUST begin with an `## Identity` section heading containing at
minimum: **Path**, **Parent**, **Generated Class**, **Type**. Placing these
fields as bold text WITHOUT the `## Identity` heading is accepted by the
validator for backwards-compatibility, but explicit heading is preferred.

```markdown
## Identity
**Path:** `/Game/Path/To/Blueprint`
**Parent:** `ParentClass` (`/Script/Module.ParentClass`)
**Generated Class:** `BlueprintName_C`
**Type:** Standard Blueprint | AnimBP | Gameplay Ability | etc.
```

### RULE 2 — Exhaustive Variable Listing (No Partial Lists)
**EVERY entry in `detailedVariables[]`** (except `UberGraphFrame`) MUST appear
in the Variables table. Do not abbreviate, summarize, or skip variables.
If there are 30 variables, the table has 30 rows. No exceptions.

Common failure: Cursor's batch-processing often listed only "key" variables
and omitted others. The validator checks every IR variable by name.

### RULE 3 — CDO Section Is Mandatory When Delta Exists
If `classDefaultValueDelta` has ANY entries, the `## CDO Overrides` section
is REQUIRED. An empty CDO section is an L1 error. If the delta is empty,
write: "Inherits all defaults from parent."

Do NOT conflate `classDefaultValueDelta` (the delta, what you want) with
`classDefaultValues` (the full CDO dump, which is noise).

### RULE 4 — UserConstructionScript Handling
`UserConstructionScript` is auto-present on all Actor-derived Blueprints.
- **If it has NO authored nodes** (empty graph, bytecodeSize ≤ 4): OMIT it.
  The validator will NOT flag its absence. It is noise.
- **If it has authored logic** (visible nodes, bytecodeSize > 4): Include it
  in the Functions section AND document its logic in the Logic section.

The validator skips `UserConstructionScript` from L1 mandatory checks.
L3 coverage checks may still flag it if the graph has content.

### RULE 5 — All Modules in Dependencies
**EVERY module in `dependencyClosure.moduleNames`** must appear in the
Dependencies `**Modules:**` list. Do not abbreviate. If the spec has a
Dependencies section at all, it must be complete.

### RULE 6 — Purpose and IR Notes Are Required
Every spec MUST have:
- `## Purpose` — 1-3 sentences describing what the Blueprint does and why it
  exists. Without this, a spec reader cannot quickly orient themselves.
- `## IR Notes` — at minimum one line: "Full coverage — no unsupported nodes."
  If `compilerIRFallback.hasUnsupportedNodes` is true, list the node types.

### RULE 7 — Logic Section Required When Graphs Have Content
If `totalNodeCount > 0` AND the EventGraph (Ubergraph) has > 0 nodes,
a `## Logic` section is REQUIRED. Data-only Blueprints with zero graph nodes
are exempt. Never omit Logic for a Blueprint with authored event/function graphs.

### RULE 8 — Functions: All IR Functions (Except Compiler-Generated) Required
Every function in `detailedFunctions[]` must appear in the spec, EXCEPT:
- `ExecuteUbergraph` and `ExecuteUbergraph_*` — always skip
- `UserConstructionScript` — skip if trivial (see Rule 4)

Named user functions, overrides, RPCs, BT event overrides (`ReceiveActivateAI`,
`ReceiveExecuteAI`, etc.) — ALL must be listed.

### RULE 9 — Trailing-Space Name Artifacts
UE Blueprint metadata sometimes stores variable or function `DisplayName` values with
a **trailing space** (e.g. `"Follow Player "`, `"Update "`, `"Initialize Size "`).
These are UE editor artifacts — not intentional parts of the identifier.

**How the validator handles them:** `validate_specs.py` strips trailing whitespace
from IR names before comparing against spec names. So a spec entry `Follow Player`
(no trailing space) correctly satisfies an IR variable named `"Follow Player "`.

**What you MUST do in the spec:**
1. List the name WITHOUT the trailing space in the table (e.g. `| Follow Player | ...`).
   The validator will match it correctly.
2. Add a note in `## IR Notes` documenting the artifact:
   ```
   **Trailing-space name artifact:** Variable `Follow Player` has a trailing space
   in the UE source metadata (`"Follow Player "` in JSON IR). This is a UE editor
   artifact; reconstruct using the stripped form.
   ```
3. If two IR entries differ only by a trailing space (e.g. `"Update"` and `"Update "`),
   they are the same function with a metadata duplicate. List both in the Functions table
   (the validator deduplicates after normalization), and note the duplication in IR Notes.

**Detection:** When iterating `detailedVariables[]` or `detailedFunctions[]`, check
`name.rstrip() != name` to identify trailing-space names.

---

## Before You Start

### Required Reading
1. **PRIME_DIRECTIVE.md** — understand the mandate and pipeline
2. **SCHEMA_REFERENCE.md** — understand every field in the JSON exports

### What You Need
- Access to the raw JSON export directory (`Saved/BlueprintExports/BP_SLZR_All_<timestamp>/`)
- A place to write specs (`docs_Lyra/BlueprintSpecs/`) — this is the canonical location
- Validation script: `Plugins/BlueprintSerializer/Scripts/validate_specs.py`
- Git access to commit progress incrementally

### Validation Command
Run after each batch to measure progress:
```bash
python Plugins/BlueprintSerializer/Scripts/validate_specs.py \
  docs_Lyra/BlueprintSpecs/ \
  Saved/BlueprintExports/BP_SLZR_All_<timestamp>/ \
  --fix-list docs_Lyra/BlueprintSpecs/_VALIDATION_FAILURES.md
```
Read `_VALIDATION_FAILURES.md` to see which specs need regeneration and why.
Target: L1 pass rate ≥ 95%, L2 ≥ 90%, L3 ≥ 85%. See `QUALITY_GATES.json`.

### How This Works
You read one raw JSON export at a time. You produce one spec markdown file per Blueprint.
You commit after each spec (or small batch). You track progress in a checklist file.
You do NOT need to hold the entire corpus in memory. Each Blueprint is independent work.

---

## Session Workflow

### Starting a New Session
1. Check git log to see what specs have already been produced
2. Read the checklist file (`Saved/BlueprintSpecs/_CHECKLIST.md`) if it exists
3. Pick the next unprocessed Blueprint
4. Read its JSON export file
5. Produce the spec following this playbook
6. Write the spec to `Saved/BlueprintSpecs/<BlueprintName>.md`
7. Update the checklist
8. Commit

### Continuing a Previous Session
1. `git log --oneline -20` to see recent spec commits
2. Read `_CHECKLIST.md` to find where you left off
3. Continue from the next unprocessed Blueprint

### First Session — Bootstrap
On the very first session, before producing any specs:
1. List all JSON files in the export directory
2. Create `_CHECKLIST.md` with every Blueprint name, marked as `[ ]` (pending)
3. Optionally create `_INDEX.md` as a stub (will be populated as specs are produced)
4. Commit this bootstrap

---

## Reading a Blueprint Export

Read the JSON file top-to-bottom. Here is what to extract from each section and
what to ignore.

### 1. Identity — Always Include

Extract from top-level fields:
- `blueprintName` — the Blueprint's name
- `blueprintPath` — full asset path (important for cross-references)
- `parentClassName` / `parentClassPath` — what this Blueprint extends
- `generatedClassName` — the UE-generated class name (used in C++ references)
- `isInterface`, `isMacroLibrary`, `isFunctionLibrary` — what KIND of Blueprint

**Classify the Blueprint type:**
- If `parentClassPath` contains `AnimInstance` or `isAnimBlueprint` is true → **AnimBP**
- If `isInterface` → **Interface**
- If `isMacroLibrary` → **Macro Library**
- If `isFunctionLibrary` → **Function Library**
- If parentClass is a BT node (`BTTask_`, `BTService_`, `BTDecorator_`) → **Behavior Tree node**
- If parentClass is a GameplayAbility → **Gameplay Ability**
- Otherwise → **Standard Blueprint**

This classification affects how you frame the spec.

### 2. Class Specifiers and Config — Include If Non-Default

- `classSpecifiers` — always include (Blueprintable, BlueprintType, etc.)
- `classConfigName` — include if the Blueprint uses config
- `implementedInterfaces` / `implementedInterfacePaths` — always include if non-empty

### 3. Class Default Values — Include the Delta

- `classDefaultValues` — the FULL CDO (Class Default Object). This is verbose.
- `classDefaultValueDelta` — ONLY the properties that DIFFER from the parent class.

**Use the delta, not the full CDO.** The delta is what makes this Blueprint unique
compared to its parent. If the delta is empty, say "No CDO overrides (inherits all
defaults from parent)."

For complex values (curves, struct literals, asset references), interpret them into
human-readable descriptions. For example:
- `"DistanceDamageFalloff": "(EditorCurveData=(Keys=((Value=1.0),...)))"` →
  "Distance damage falloff: 100% at 0-2000 units, drops to 50% at 2001+ units"
- `"MaterialDamageMultiplier": "(((TagName=\"Gameplay.Zone.WeakSpot\"), 2.0))"` →
  "2x damage multiplier on weak spots (Gameplay.Zone.WeakSpot tag)"

### 4. Variables (`detailedVariables[]`) — Always Include

For each variable, capture:
- Name, type (human-readable), default value
- Visibility: public/private, editable, exposed on spawn
- **Replication**: replicated? condition? OnRep function? (CRITICAL for multiplayer)
- Category (editor organization)
- Declaration specifiers (the UPROPERTY specifiers)

**Format as a table** for density. Group by category if there are many.

**Skip these fields** (noise): `typeSubCategory` when "None", tooltip when empty.

### 5. Functions (`detailedFunctions[]`) — Always Include

For each function, capture:
- Name, access specifier (public/protected/private)
- Pure? Static? Override? Calls parent?
- **Network flags**: Server RPC? Client RPC? Multicast? Reliable? (CRITICAL)
- Authority-only? Cosmetic-only?
- Input/output parameters (use `detailedInputParams` for structured data)
- Local variables
- Return type
- Declaration specifiers (the UFUNCTION specifiers)
- `bytecodeSize` — note if large (indicates complex logic)

**Format as a table** for the signature summary, then detail complex functions
individually in the Logic section.

### 6. Components (`detailedComponents[]`) — Always Include

For each component:
- Name, class, parent in hierarchy
- Whether it's root, inherited, or added by this Blueprint
- Transform (if meaningful — skip if default 0,0,0)
- Asset references (meshes, materials, sounds)
- Property overrides vs inherited values

**Present as a tree** to show the hierarchy.

### 7. Structured Graphs — THE CORE

This is where the actual logic lives. This is the most important part of the spec.

#### How to Read a Graph

Each `structuredGraphs[]` entry has:
- `name` — graph name ("EventGraph", function name, "AnimGraph", etc.)
- `graphType` — "Ubergraph", "Function", "Macro", "AnimGraph", "StateMachine"
- `nodes[]` — all nodes
- `flows.execution[]` — execution edges (white wires)
- `flows.data[]` — data edges (colored wires)

#### Walk the Execution Flow

**Start from entry points:**
- In an EventGraph: find `K2Node_Event` nodes — these are the entry points
- In a Function graph: find `K2Node_FunctionEntry` — this is the entry point

**Follow execution edges** from `sourcePinName` → `targetNodeGuid`:
- `"then"` → the next statement
- `"then"` / `"else"` on a Branch → if/else paths
- `"Then 0"`, `"Then 1"` on a Sequence → sequential statements
- `"Completed"` on a latent node → the continuation after the async operation

**For each node in the chain**, describe what it does:
- `K2Node_CallFunction` → function call. Use `meta.functionName` + `meta.functionOwner`
  to write it as `Owner::FunctionName(args)` or `Target->FunctionName(args)`
- `K2Node_IfThenElse` → branch. What's the condition? (follow the data edge to the
  condition pin)
- `K2Node_VariableGet` / `K2Node_VariableSet` → variable read/write. Use `meta.variableName`
- `K2Node_MacroInstance` → macro call. Use `meta.macroGraphName`
- `K2Node_ExecutionSequence` → sequential block of statements
- `K2Node_SpawnActorFromClass` → actor spawn. What class? (follow data edge to class pin)

**Resolve data inputs** for each node:
- If a data pin has `connected: true`, follow the data edge to find the source
- If a data pin is unconnected, its value comes from `defaultValue` on the pin

#### Translate to Pseudo-Code

Convert the graph walk into readable pseudo-code or structured English. Example:

```
Event: ReceiveActivationAI(OwnerController, ControlledPawn)
  → SendGameplayEventToActor(
      Actor: ControlledPawn,
      EventTag: "InputTag.Weapon.Reload",
      Payload: default GameplayEventData
    )
```

This is MORE VALUABLE than listing raw nodes. The spec reader needs to understand
WHAT THE LOGIC DOES, not what nodes exist.

#### Ignore These in Graphs
- `posX`, `posY`, `width`, `height` — editor layout, cosmetic only
- `K2Node_Knot` (Reroute) — transparent passthrough, skip it and follow the data edge
- `K2Node_SetVariableOnPersistentFrame` — compiler-generated frame storage, not authored
  logic. Note its existence but don't describe it as a user-authored step.
- `bIsIntermediateNode: "True"` nodes — compiler-generated. Include only if they contain
  meaningful routing (like ExecuteUbergraph calls).
- Comment bubble fields (`bCommentBubbleVisible`, etc.)
- `DeprecatedPins`, `ErrorMsg`, `ErrorType`, `NodeUpgradeMessage`

### 8. Dependency Closure — Always Include

- `moduleNames` — UE modules this Blueprint depends on
- `includeHints` / `nativeIncludeHints` — C++ headers needed
- `classPaths`, `structPaths`, `enumPaths` — C++ types referenced
- `interfacePaths` — interfaces referenced
- `assetPaths` — other assets referenced

**Present as a grouped list.** This is essential for C++ conversion and for
understanding what systems this Blueprint touches.

### 9. IR Completeness — Note If Issues

- `compilerIRFallback.hasUnsupportedNodes` — if true, note which node types are
  unhandled. This means the spec will have gaps for those nodes.
- `coverage.unsupportedNodeTypes` — list them

If `hasUnsupportedNodes` is false (the common case in a mature extraction),
you can note "Full IR coverage — no unsupported nodes."

### 10. User-Defined Types — Include If Present

- `userDefinedStructSchemas[]` — structs defined in this Blueprint
- `userDefinedEnumSchemas[]` — enums defined in this Blueprint

Document their fields/values fully. These are important for understanding the
Blueprint's data model.

### 11. Timelines — Include If Present

- `timelines[]` — timeline tracks with keyframes
- Document the curve shape, duration, looping behavior

### 12. AnimBP-Specific — Include for AnimBPs

If `blueprintInfo.isAnimBlueprint` is true:
- `targetSkeleton` — the skeleton this AnimBP targets
- Look for AnimGraph and StateMachine graph types in `structuredGraphs`
- Document state machine states and transitions
- Note animation asset references

---

## Spec Output Format

Each spec should follow this structure. Adapt section depth based on Blueprint
complexity — a data-only Blueprint needs far less than a logic-heavy one.

```markdown
# <BlueprintName>

## Identity
**Path:** `/Game/Path/To/Blueprint`
**Parent:** `ParentClassName` (`/Script/Module.ParentClassName`)
**Generated Class:** `BlueprintName_C`
**Type:** Standard Blueprint | AnimBP | Interface | Macro Library | Gameplay Ability | etc.

## Purpose
One to three sentences. What does this Blueprint do? Why does it exist?
What system owns it and how does it interact with other systems?

## Interfaces
- InterfaceName1 (`/Script/Module.InterfaceName1`)
- InterfaceName2

## Class Specifiers
Blueprintable, BlueprintType, ...

## CDO Overrides (vs Parent)
| Property | Value | Notes |
|----------|-------|-------|
| PropertyName | value | human-readable interpretation |

(If no overrides: "Inherits all defaults from parent.")

## Variables
| Name | Type | Default | Replicated | Specifiers |
|------|------|---------|------------|------------|
| ... | ... | ... | Yes/No | BlueprintReadWrite, etc. |

**RULE: Every `detailedVariables[]` entry (except UberGraphFrame) must have a row.**

## Functions
| Name | Access | Pure | Net | Params | Notes |
|------|--------|------|-----|--------|-------|
| ... | Public | No | Server RPC | ... | Override |

**RULE: Every `detailedFunctions[]` entry (except ExecuteUbergraph_* and trivial UCS) must have a row.**

## Components
```
RootComponent (SceneComponent)
├── MeshComponent (SkeletalMeshComponent) [Mesh: /Path/To/Mesh]
├── CollisionComponent (CapsuleComponent)
└── EffectsComponent (NiagaraComponent)
```

## Logic

### EventGraph

#### Event: BeginPlay
[Pseudo-code or structured description of what happens]

#### Event: OnWeaponFired
[Pseudo-code or structured description]

### Function: CalculateDamage
[Pseudo-code or structured description]

## Dependencies
**Modules:** Engine, GameplayAbilities, GameplayTags, ...
**C++ Types:** AActor, FGameplayTag, ...
**Assets:** /Path/To/Mesh, /Path/To/Material, ...

**RULE: Every module in `dependencyClosure.moduleNames` must appear in Modules list.**

## User-Defined Types

### Struct: FMyCustomStruct
| Field | Type | Default |
|-------|------|---------|
| ... | ... | ... |

## IR Notes
Full coverage — no unsupported nodes.
(Or: "Unsupported nodes: K2Node_SomethingExotic. Spec gaps exist for these nodes.")
```

### Spec Quality Standards

1. **Reconstruction-complete** — someone reading ONLY the spec (no access to the
   original Blueprint or JSON) can recreate the Blueprint's behavior in either
   Blueprint or C++. Same variables, same interfaces, same logic, same replication,
   same dependencies. The recreation doesn't need to be identical, but it must be
   functionally equivalent — it should behave the same and could replace the original.
2. **Two-path convergence** — the C++ you'd write from this spec should be functionally
   equivalent to what a direct JSON→C++ converter would produce. If the spec loses
   information the converter would use, the spec is incomplete.
3. **Accurate** — types, default values, and replication flags match the JSON exactly
4. **Dense** — no wasted words, no repeating raw JSON keys, no cosmetic data.
   Organize to maximize information density while maintaining readability.
5. **Readable** — a developer who has never seen the Blueprint can understand what it
   does and how it works from the spec alone
6. **Cross-referenceable** — use full asset paths so specs can reference each other

### Data-Only Blueprints

Some Blueprints have no logic — no variables, no functions, empty graphs. They exist
only to set CDO values on a parent class (like weapon configurations). For these:

- The CDO Override section IS the spec. Make it thorough.
- Interpret curve data, struct literals, and asset references into readable descriptions.
- Note which parent class provides the logic.
- Keep the spec short — don't add empty sections.

---

## Handling Large Blueprints

Some exports are 1MB+ (hundreds of nodes, dozens of functions). Strategy:

1. **Don't panic.** Read in chunks using file offset/limit if needed.
2. **Start with identity and structure** — get the big picture first.
3. **Process one graph at a time** — the EventGraph, then each function graph.
4. **For very complex graphs**, focus on:
   - Entry points (events, function entries)
   - The main execution flow (follow the exec edges)
   - Branch conditions (what decisions does the logic make?)
   - Key function calls (what external systems does it interact with?)
   - Variable reads/writes (what state does it manage?)
5. **It's OK to summarize** repetitive patterns (e.g., "Sets up 12 similar particle
   effects with varying parameters" rather than listing all 12).

---

## Tracking Progress

### Checklist File (`_CHECKLIST.md`)

```markdown
# Blueprint Spec Generation Checklist

**Corpus:** 485 Blueprints
**Completed:** 12 / 485
**Last Updated:** 2026-02-25

## Completed
- [x] B_WeaponInstance_Pistol
- [x] B_WeaponInstance_Rifle
- [x] BTS_ReloadWeapon
...

## Pending
- [ ] ABP_Mannequin_Base
- [ ] B_WeaponFire
...
```

### Index File (`_INDEX.md`)

Group completed specs by system/domain. Update as you go:

```markdown
# Blueprint Spec Index

## Weapons
- [B_WeaponInstance_Pistol](B_WeaponInstance_Pistol.md) — Pistol weapon config
- [B_WeaponInstance_Rifle](B_WeaponInstance_Rifle.md) — Rifle weapon config

## AI / Bot
- [BTS_ReloadWeapon](BTS_ReloadWeapon.md) — BT service: trigger reload
```

### Commit Messages

Use clear, searchable commit messages:
```
Spec: B_WeaponInstance_Pistol — pistol weapon configuration
Spec: BTS_ReloadWeapon — BT service for weapon reload
Spec: batch 3 — HUD widgets (W_Crosshair, W_DamageIndicator, W_HealthBar)
```

---

## System Grouping Heuristics

When building the index, group Blueprints by their asset path and parent class:

| Path Pattern | System |
|---|---|
| `/Game/Weapons/` or `/ShooterCore/Weapons/` | Weapons |
| `/Game/Characters/` | Characters / Cosmetics |
| `/Game/UI/` or `W_` prefix | UI / HUD |
| `/Game/Input/` or `InputAction` | Input |
| `/ShooterCore/Bot/` or `BT_`/`BTS_`/`BTD_` prefix | AI / Bot |
| `/Game/Experiences/` | Experiences / Game Modes |
| `/Game/System/` | Core Systems |
| `ABP_` prefix | Animation Blueprints |
| `GA_` prefix | Gameplay Abilities |
| `GE_` prefix | Gameplay Effects |
| `GCN_` prefix | Gameplay Cue Notifies |

Adapt these heuristics to the actual project's directory structure.

---

## Example: Producing a Spec

Given the BTS_ReloadWeapon export:

1. **Identity**: BTS_ReloadWeapon, parent BTService_BlueprintBase, Behavior Tree Service
2. **CDO**: No delta (all defaults from parent)
3. **Variables**: None (just UberGraphFrame, which is compiler-generated — skip)
4. **Functions**: ReceiveActivationAI (override), ExecuteUbergraph (compiler-generated — skip)
5. **Components**: None
6. **Graph logic**: EventGraph has one event:
   - `ReceiveActivationAI(OwnerController, ControlledPawn)`
   - → `SendGameplayEventToActor(Actor: ControlledPawn, EventTag: "InputTag.Weapon.Reload")`
7. **Dependencies**: AIModule, GameplayAbilities, GameplayTags

**Resulting spec:**

```markdown
# BTS_ReloadWeapon

**Path:** `/ShooterCore/Bot/Services/BTS_ReloadWeapon`
**Parent:** `BTService_BlueprintBase` (`/Script/AIModule.BTService_BlueprintBase`)
**Generated Class:** `BTS_ReloadWeapon_C`
**Type:** Behavior Tree Service

## Purpose
BT service that triggers weapon reload on the controlled pawn by sending a
gameplay event with the `InputTag.Weapon.Reload` tag.

## CDO Overrides
Inherits all defaults from parent (Interval: 0.5s, RandomDeviation: 0.1s).

## Logic

### Event: ReceiveActivationAI
Fires when the BT service activates on an AI-controlled pawn.

```
ReceiveActivationAI(OwnerController: AAIController*, ControlledPawn: APawn*)
  → UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
      Actor: ControlledPawn,
      EventTag: FGameplayTag("InputTag.Weapon.Reload"),
      Payload: FGameplayEventData()
    )
```

The service sends a gameplay event to the pawn, which is expected to trigger
a reload gameplay ability via the Gameplay Ability System.

## Dependencies
**Modules:** AIModule, Engine, GameplayAbilities, GameplayTags
**C++ Types:** AAIController, APawn, AActor, UAbilitySystemBlueprintLibrary, FGameplayEventData, FGameplayTag

## IR Notes
Full coverage — no unsupported nodes.
```

---

## What Makes a Good Spec

A good spec lets someone:
1. **Recreate the Blueprint** in either Blueprint or C++ that behaves identically
   to the original — without ever seeing the original
2. **Understand what the Blueprint does** without opening the editor
3. **Know how to interact with it** from other Blueprints or C++
4. **Produce C++ equivalent to what a direct IR→C++ converter would generate** —
   if the spec and the converter produce different C++, one of them has a bug
5. **Find it** when searching for a specific system or capability

The quality test: take the spec and give it to an AI (or human) to produce C++.
Take the raw JSON IR and give it to a C++ converter tool. Compare the outputs.
They should be functionally equivalent. If not, the spec is missing information.

A bad spec:
- Lists raw JSON fields without interpretation
- Includes cosmetic data (positions, GUIDs, pin IDs)
- Is longer than the original JSON (defeats the purpose)
- Has empty sections instead of omitting them
- Uses vague descriptions ("does some stuff") instead of specific pseudo-code
- Loses behavioral detail that would prevent accurate C++ reconstruction
