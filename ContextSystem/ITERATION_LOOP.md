# ITERATION LOOP — Autonomous Self-Improving Quality Protocol

> **Give this document to any AI.** It contains everything needed to drive
> spec quality from the current state to 100% pass rate autonomously.
> The loop also covers the extraction regression harness and includes a
> META-IMPROVEMENT layer: the loop improves itself by analyzing the quality
> of its own improvements.

---

## Three Loops in This System

```
LOOP A: Extraction Regression (run if BlueprintSerializer code changes)
  → test extractor → fix plugin → commit → loop until all 14 gates pass

LOOP B: Spec Quality (run always when pass rate < thresholds)
  → validate specs → fix specs → update playbook → commit → loop until ≥95% pass

LOOP C: Meta-Improvement (run after every LOOP B cycle)
  → analyze IF the improvements were good → update the improvement process itself
  → commit updated ITERATION_LOOP.md / ANALYSIS_PLAYBOOK.md → next LOOP B cycle
```

All three loops are **mandatory when applicable**. After any code change to
BlueprintSerializer, run A. After any spec generation or fix session, run B.
After every B cycle, run C to make B smarter.

---

## Key File Paths

| File | Purpose |
|------|---------|
| `Plugins/BlueprintSerializer/Scripts/validate_specs.py` | Spec validation engine |
| `Plugins/BlueprintSerializer/Scripts/Run-RegressionSuite.ps1` | Extraction regression harness |
| `Plugins/BlueprintSerializer/REGRESSION_BASELINE.json` | Baseline metrics for regression gates |
| `Plugins/BlueprintSerializer/ContextSystem/QUALITY_GATES.json` | Pass rate thresholds + self-improvement rules |
| `Plugins/BlueprintSerializer/ContextSystem/ANALYSIS_PLAYBOOK.md` | Spec generation rules (8 CRITICAL RULES) |
| `Plugins/BlueprintSerializer/ContextSystem/PRIME_DIRECTIVE.md` | The mandate |
| `Plugins/BlueprintSerializer/ContextSystem/ITERATION_LOOP.md` | This file — the autonomous loop protocol |
| `docs_Lyra/BlueprintSpecs/` | All spec files (*.md) |
| `docs_Lyra/BlueprintSpecs/_VALIDATION_FAILURES.md` | Current failures (auto-updated) |
| `docs_Lyra/BlueprintSpecs/_CHECKLIST.md` | Corpus checklist |
| `Saved/BlueprintExports/BP_SLZR_All_*/` | Source JSON IR for all BPs |
| `Saved/BlueprintExports/BP_SLZR_RegressionRun_*.json` | Regression suite reports (auto-generated) |

---

## LOOP A — Extraction Regression

**Trigger:** Any non-trivial change to `Plugins/BlueprintSerializer/` C++ source code.
**Purpose:** Verify the extractor still produces complete, well-formed JSON IR across all 14 gates.
**Run from:** Project root (`C:\Users\jinph\Documents\Unreal Projects\LyraStarterGame`)

### When to Run LOOP A

- After fixing a bug in the extractor
- After adding a new node type handler
- After modifying schema fields in the JSON output
- Before committing any plugin source changes
- When `_VALIDATION_FAILURES.md` shows new error types that weren't there before
  (this can indicate the JSON IR changed, breaking previously passing specs)

### LOOP A — Step by Step

#### A1. Rebuild the Plugin

```bash
powershell -Command "& 'C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat' \
  LyraEditor Win64 Development \
  'C:\Users\jinph\Documents\Unreal Projects\LyraStarterGame\LyraStarterGame.uproject' \
  -waitmutex 2>&1"
```

Fix any compile errors before proceeding. The plugin DLL is produced as a side-effect
of building `LyraEditor`. Never build `BlueprintSerializerEditor` — it has no Target.cs.

#### A2. Run the Regression Suite

```powershell
# Full run (re-exports all 485 BPs, then validates):
powershell -File "Plugins/BlueprintSerializer/Scripts/Run-RegressionSuite.ps1"

# Skip re-export (use existing Saved/BlueprintExports/ data):
powershell -File "Plugins/BlueprintSerializer/Scripts/Run-RegressionSuite.ps1" -SkipExport

# Custom export dir:
powershell -File "Plugins/BlueprintSerializer/Scripts/Run-RegressionSuite.ps1" `
  -ExportDir "Saved/BlueprintExports/BP_SLZR_All_<timestamp>"
```

The suite runs `UnrealEditor-Cmd.exe` headless and produces:
- `Saved/BlueprintExports/BP_SLZR_RegressionRun_<timestamp>.json` — suite report
- Exit code 0 = all gates pass, exit code 2 = one or more gates fail

#### A3. Read the Suite Report

```bash
# Find the latest regression report:
ls Saved/BlueprintExports/BP_SLZR_RegressionRun_*.json
# Read the most recent one
```

The report contains:
- `suitePass`: true/false
- `failures[]`: list of gate names that failed
- `validationReportPath`: path to detailed validation JSON
- `curveAuditReportPath`: path to curve audit JSON

#### A4. Check the 14 Gates

All 14 gates in `REGRESSION_BASELINE.json` must pass:

| Gate | What it checks |
|------|---------------|
| `manifestPresent` | Export manifest file exists |
| `manifestCountsMatch` | Manifest BP count ≥ 485 |
| `parseErrorsZero` | Zero BPs had JSON parse errors |
| `requiredKeysPresent` | All required top-level keys in every export |
| `variableDeclarationCoverage` | All 3133 variables have declaration specifiers |
| `functionDeclarationCoverage` | All 936 functions have declaration specifiers |
| `variableReplicationShape` | All 3133 variables have replication shape |
| `functionNetworkShape` | All 936 functions have network shape |
| `classConfigFlagShape` | All 485 BPs have config flag shape |
| `componentOverrideShape` | All 818 components present |
| `dependencyClosureCoverage` | All 485 BPs have non-empty dependency closure |
| `includeHintsCoverage` | All 485 BPs have non-empty include hints |
| `compilerIRFallbackShape` | All 485 BPs have compilerIRFallback shape |
| `macroDependencyClosureShape` | All 485 BPs have macro dependency closure shape |

#### A5. Fix Failing Gates

If any gate fails:

1. **Read the gate failure message** from `failures[]` in the suite report
2. **Read the detailed validation JSON** at `validationReportPath`
3. **Identify which BP(s) caused the failure** — look for BPs with counts below baseline
4. **Read the relevant C++ source** in `Plugins/BlueprintSerializer/Source/`
5. **Fix the extraction logic** — add missing fields, fix serialization bugs, etc.
6. **Rebuild (A1) and re-run (A2)** — loop until all 14 gates pass

Common gate failure causes:
- `parseErrorsZero`: JSON serialization bug (unescaped strings, invalid UTF-8)
- `variableDeclarationCoverage`: new variable property not being serialized
- `dependencyClosureCoverage`: module resolution failing for new BP types
- `manifestCountsMatch`: newly added BPs not being discovered by the exporter

#### A6. Update Baseline (If Intentional Count Change)

If you intentionally added new BPs, node types, or fields:

```bash
# Read the validation report to get new counts
# Then update REGRESSION_BASELINE.json with the new minimum metrics
# Commit: "Regression: update baseline for [reason]"
```

Never lower baseline numbers without explicit justification.

#### A7. Commit Passing State

```bash
git add Plugins/BlueprintSerializer/
git commit -m "BlueprintSerializer: [description] — all 14 regression gates pass"
```

The commit message must confirm gate pass status so git history is a regression log.

### LOOP A — Stopping Criteria

All 14 gates in `REGRESSION_BASELINE.json` pass. Suite exits with code 0.

---

## LOOP B — Spec Quality

**Trigger:** After any spec generation or fix session. At the start of any session where
pass rates are below thresholds. This loop runs until L1 ≥ 95%.

### STEP 0: Orient (First Iteration Only)

```bash
# Check current branch and status
git log --oneline -5
git status

# Find the latest export directory
ls Saved/BlueprintExports/

# Run the validator to get baseline
python Plugins/BlueprintSerializer/Scripts/validate_specs.py \
  docs_Lyra/BlueprintSpecs/ \
  Saved/BlueprintExports/BP_SLZR_All_20260220_200043/ \
  --fix-list docs_Lyra/BlueprintSpecs/_VALIDATION_FAILURES.md
```

Record the starting pass rate (Summary section of `_VALIDATION_FAILURES.md`).

---

### STEP 1: Run the Validator

```bash
python Plugins/BlueprintSerializer/Scripts/validate_specs.py \
  docs_Lyra/BlueprintSpecs/ \
  Saved/BlueprintExports/BP_SLZR_All_20260220_200043/ \
  --fix-list docs_Lyra/BlueprintSpecs/_VALIDATION_FAILURES.md
```

This overwrites `_VALIDATION_FAILURES.md` with current results.

---

### STEP 2: Read and Analyze Failures

Read `docs_Lyra/BlueprintSpecs/_VALIDATION_FAILURES.md`.

Check the Summary section first:
- If **Pass rate ≥ 95%** → run one more validation to confirm, then **DONE with LOOP B**.
  Proceed to LOOP C (Meta-Improvement).
- Otherwise, continue.

Count recurring error patterns in the Failures table:

```python
# Quick pattern count (run as inline python or mentally):
# Count occurrences of each unique "Top Issue" message.
# Any pattern appearing in 5+ rows triggers a PLAYBOOK UPDATE (Step 5).
```

Categorize the failures into buckets (examples from known patterns):
- **Missing variables** — variable name in IR but not in spec table
- **Missing CDO section** — `classDefaultValueDelta` non-empty, no CDO section
- **Missing CDO properties** — CDO section exists but individual properties absent
- **Missing functions** — user function in IR but not in spec
- **Missing Logic section** — spec has no `## Logic` but Blueprint has nodes
- **Missing modules** — module in IR dependency closure but not in spec Modules list
- **Missing Purpose section** — no `## Purpose` section
- **Missing IR Notes** — no `## IR Notes` section

---

### STEP 3: Sort by Impact

Order failing BPs by error count (highest first). The Failures table in
`_VALIDATION_FAILURES.md` is already sorted this way.

Work through the list top-to-bottom. Each BP you fix is a win.

---

### STEP 4: Regenerate Failing Specs

For each failing BP (in order of error count):

#### 4a. Read the source JSON IR

```bash
# Find the JSON export for this Blueprint:
ls Saved/BlueprintExports/BP_SLZR_All_20260220_200043/ | grep <BlueprintName>
# Then read it:
# Read Saved/BlueprintExports/BP_SLZR_All_20260220_200043/<BlueprintName>.json
```

#### 4b. Read the current (failing) spec

```bash
# Read docs_Lyra/BlueprintSpecs/<BlueprintName>.md
```

#### 4c. Identify the gap

Compare the spec against the IR using the error messages from `_VALIDATION_FAILURES.md`.
Examples:
- "Variable 'X' in IR but missing from spec" → add row to Variables table
- "CDO has N overrides but spec has no CDO section" → add CDO Overrides section from `classDefaultValueDelta`
- "Function 'Y' in IR but missing from spec" → add row to Functions table

#### 4d. Write the corrected spec

Follow ALL rules in `ANALYSIS_PLAYBOOK.md` — especially the CRITICAL RULES at the top.

For EXHAUSTIVE VARIABLE LISTING (the most common failure):
- Read ALL entries in `detailedVariables[]` from the JSON
- Every entry (except `UberGraphFrame`) gets a row in the Variables table
- Do not skip, summarize, or abbreviate

For CDO SECTIONS:
- Read `classDefaultValueDelta` (not `classDefaultValues`)
- Every key in the delta gets a row in CDO Overrides
- Interpret complex values (curve data, struct literals) into human-readable form

For FUNCTIONS:
- Read ALL entries in `detailedFunctions[]`
- Skip: `ExecuteUbergraph`, `ExecuteUbergraph_*`, and `UserConstructionScript` if trivial
- Include: every other function, especially BT event overrides, custom functions, RPCs

For LOGIC:
- Only required if `totalNodeCount > 0` and EventGraph has nodes
- Follow execution chains from K2Node_Event → execution edges → pseudo-code
- See "Reading a Blueprint Export → 7. Structured Graphs" in ANALYSIS_PLAYBOOK.md

#### 4e. Write the spec

```
Write docs_Lyra/BlueprintSpecs/<BlueprintName>.md
```

#### 4f. Continue to next failing BP

Process as many as feasible in one session. Aim for batches of 10-20 BPs
before committing. Commit every 20 specs or at natural stopping points.

---

### STEP 5: Update the Playbook (If Pattern Threshold Hit)

If any error pattern appears **5 or more times** in `_VALIDATION_FAILURES.md`
and is NOT already covered by a CRITICAL RULE in `ANALYSIS_PLAYBOOK.md`:

1. Read `ANALYSIS_PLAYBOOK.md`
2. Add a new CRITICAL RULE covering that pattern
3. Explain: what triggers the error, how to avoid it, what to write instead
4. Commit the playbook update with message: `Playbook: add rule for [pattern]`

This is the **self-improvement mechanism**. Each iteration makes the playbook
smarter so future AI sessions start with better instructions.

---

### STEP 6: Commit Progress

```bash
git add docs_Lyra/BlueprintSpecs/
git commit -m "Spec: iteration N — fix M specs (pass rate X% → Y%)"
```

Include the pass rate change in the commit message so git history tracks progress.

---

### STEP 7: Re-run Validator

Go back to STEP 1.

Check the new pass rate. Compare to QUALITY_GATES.json thresholds:
- L1 pass rate ≥ 95% ✓
- L2 pass rate ≥ 90% ✓
- L3 pass rate ≥ 85% ✓

If all thresholds met: **DONE with LOOP B**. Commit the final validation report.
Proceed to LOOP C. Otherwise: continue the loop.

---

### LOOP B — Stopping Criteria

The loop terminates when ALL of these are true:

1. `_VALIDATION_FAILURES.md` Summary shows:
   - **Pass rate ≥ 95%** (L1 threshold from `QUALITY_GATES.json`)
2. No L1 errors remain (only warnings are acceptable)
3. The validator exits with code 0

**100% L1 pass rate is the target.** Go for it.

---

## LOOP C — Meta-Improvement

**Trigger:** After every complete LOOP B cycle (one pass through STEP 1 → STEP 7).
**Purpose:** Analyze whether the improvements were good quality, then improve the improvement
process itself. This is the mechanism by which the system gets smarter about getting smarter.

LOOP C does not fix specs. It fixes the *process* that fixes specs.

### Why LOOP C Exists

LOOP B can converge to 95% with bad practices:
- Playbook rules that are too vague to follow reliably
- Playbook rules that address symptoms rather than root causes
- An improvement process that works for current failures but will miss future ones
- An AI that regenerates specs correctly but can't explain WHY the rule works

LOOP C catches these problems. It asks: **were the improvements actually good?**
Not just "did the pass rate go up" but "did the process that caused the pass rate
to go up produce rules that will prevent the same failures in the future?"

---

### LOOP C — Step by Step

#### C1. Gather the Iteration Evidence

Collect the evidence from the just-completed LOOP B cycle:

1. **Pass rate delta**: what was X% before, what is Y% now?
2. **Playbook rules added**: which new CRITICAL RULES were added in STEP 5?
3. **Specs regenerated**: which BPs were fixed, and what errors did they have?
4. **Git log**: read the commit messages from this cycle
5. **Remaining failures**: read current `_VALIDATION_FAILURES.md` — what still fails?

```bash
# See what changed in this iteration
git log --oneline -10

# Check current failure state
# Read docs_Lyra/BlueprintSpecs/_VALIDATION_FAILURES.md (Summary section)
```

---

#### C2. Analyze Rule Quality

For each CRITICAL RULE added during STEP 5 of LOOP B, ask these questions:

**Q1: Is the rule precise enough to be machine-verifiable?**
- A rule like "List all variables" is good — it has a clear pass/fail test.
- A rule like "Be thorough" is bad — no AI can reliably follow it.
- Fix: rewrite vague rules with explicit field names, counts, or format requirements.

**Q2: Does the rule address the root cause or just the symptom?**
- Symptom rule: "Don't forget CDO properties" — doesn't explain WHY they're forgotten.
- Root cause rule: "CDO Overrides must come from `classDefaultValueDelta`, not
  `classDefaultValues`. AIs frequently read the wrong field. Always check the key
  name in the JSON before writing CDO rows."
- Fix: rewrite rules to explain the mechanism behind the failure.

**Q3: Would a fresh AI reading only the playbook produce a passing spec?**
- Read the CRITICAL RULES section of `ANALYSIS_PLAYBOOK.md` as if you have never
  seen a spec before. Can you produce a correct spec from just the rules?
- If not — identify the gap and add it.

**Q4: Are rules ordered by importance?**
- CRITICAL RULES should be ordered so that the highest-impact failures are covered first.
- Missing variables (most common failure) should be RULE 2 or earlier.
- Fix: reorder rules so most-common failures appear first.

**Q5: Does the rule include a concrete example of CORRECT vs INCORRECT spec content?**
- Rules without examples are harder to follow reliably.
- Fix: add a "WRONG" and "RIGHT" example to any rule that lacks one.

---

#### C3. Analyze Improvement Process Quality

Beyond individual rules, analyze the LOOP B process itself:

**P1: Were specs fully regenerated or spot-patched?**
- Spot-patching (adding only the missing row) often misses other problems in the same spec.
- Full regeneration (rewrite from JSON IR) is always more reliable.
- Check: do any specs that were "fixed" this cycle still have errors? If yes, spot-patching
  is the likely cause. Update STEP 4 instructions to favor full regeneration.

**P2: Were BPs processed in the right order?**
- High-error BPs fixed first = maximum pass rate improvement per unit of work.
- If the order deviated, note why and update STEP 3.

**P3: Was the batch size (10-20 BPs per commit) appropriate?**
- If context was running out before 10 BPs, the batch size should be smaller.
- If BPs were simple data-only types, 20+ per batch may be fine.
- Update STEP 4f batch size guidance if evidence suggests a better number.

**P4: Did any new error categories appear after fixes?**
- If fixing variable tables revealed missing CDO sections (because the spec was rewritten),
  that's expected. If entirely new error types appeared from nowhere, this may indicate
  a validator bug or a JSON IR change. Document this.

**P5: How long did the cycle take?**
- If the cycle took more than one AI session, note which steps were slowest.
- Long steps indicate either too-large batches or insufficient guidance in the playbook.

---

#### C4. Update the Improvement Spec

Based on the analysis in C2 and C3:

1. **Update `ANALYSIS_PLAYBOOK.md`**: refine any CRITICAL RULES that were vague,
   add examples, fix rule ordering, add root-cause explanations.

2. **Update `ITERATION_LOOP.md` (this document)**: update the protocol steps if
   the process analysis (C3) found better approaches. Update the "Current State"
   section with the new pass rate and remaining priorities.

3. **Update `QUALITY_GATES.json`**: if `known_resolved_patterns` should be updated,
   or if new thresholds make sense given current state.

4. **Update `PRIME_DIRECTIVE.md`**: if the mandate itself needs clarification based
   on what was learned (rare, but possible for fundamental misunderstandings).

Commit each update separately with clear messages:
```bash
git commit -m "Playbook: refine RULE N — add example, clarify root cause"
git commit -m "IterationLoop: update LOOP C guidance for [pattern]"
git commit -m "QualityGates: add resolved pattern [name]"
```

---

#### C5. Score the Iteration

Record a brief self-assessment of the iteration quality:

```
Iteration N Meta-Assessment:
- Pass rate delta: X% → Y% (+Z%)
- BPs fixed: N
- New rules added: N
- Rules refined: N
- Root causes addressed: yes/partial/no
- Fresh-AI test: would a new AI produce passing specs from playbook alone? yes/partial/no
- Process changes made: [description]
- Confidence the next iteration will be faster: high/medium/low
- Reason: [one sentence]
```

This assessment should be written to a comment in `QUALITY_GATES.json`
under `current_status.last_meta_assessment`, or committed as a brief git message.

---

#### C6. Trigger Next LOOP B Cycle

After LOOP C completes, immediately run LOOP B again (starting at STEP 1).

The improvements from LOOP C take effect in the next B cycle:
- Better playbook rules → fewer errors in the next batch of spec regenerations
- Better process guidance → faster spec fixes
- Better understanding of root causes → fewer repeat failures

Keep cycling B → C → B → C until LOOP B's stopping criteria are met.

---

### LOOP C — Stopping Criteria

LOOP C does not have its own stopping criteria. It runs after every LOOP B cycle
until LOOP B reaches ≥ 95% L1 pass rate. At that point:

1. Run one final LOOP C to document what was learned
2. Ensure `ANALYSIS_PLAYBOOK.md` reflects all the lessons from the full run
3. The final playbook state should be good enough that a fresh AI generating
   specs from scratch produces ≥ 95% passing specs WITHOUT needing LOOP B at all

**That is the ultimate goal of LOOP C:** make LOOP B unnecessary for new projects.

---

## Context Compaction Protocol

This loop is multi-session by design. Every AI running it must manage context continuity.

### Proactive checkpoint trigger

When 3 or more of these are true, write a checkpoint before continuing:
- 15+ back-and-forth exchanges in the current session
- Multiple large JSON IR files and spec files have been read
- 3+ fix-validate-commit cycles have completed
- Context appears dense (slow responses, re-reading files already read)

See `cortext/playbooks/context_compaction_awareness.md` for the full protocol.

### What to write in the checkpoint

At `docs_Lyra/BlueprintSpecs/_HANDOFF_LATEST.md` (project-local handoff pointer):

```markdown
# LOOP B Handoff

## State
- Pass rate: X% (N/482)
- Branch: docs/restructure
- Last commit: <hash> — <message>

## Next action
Fix <N> remaining specs. Top priority: <list top 5 BP names from _VALIDATION_FAILURES.md>

## Validator command
python Plugins/BlueprintSerializer/Scripts/validate_specs.py \
  docs_Lyra/BlueprintSpecs/ \
  Saved/BlueprintExports/BP_SLZR_All_20260220_200043/ \
  --fix-list docs_Lyra/BlueprintSpecs/_VALIDATION_FAILURES.md

## Known blockers
None — all 482 specs pass 100%. Previous trailing-space blocker was fixed in
validate_specs.py (commit 1226e65) via _norm()=rstrip() normalization.
```

### On resume (after compaction or agent swap)

1. Read `docs_Lyra/BlueprintSpecs/_HANDOFF_LATEST.md`
2. Run the validator command listed there to confirm current state
3. Read `docs_Lyra/BlueprintSpecs/_VALIDATION_FAILURES.md`
4. Continue from STEP 3 (sort by impact) in LOOP B

Do not re-read all context from scratch. The validator output is ground truth.

---

## How to Handle Difficult Cases (LOOP B)

### Blueprint has 20+ variables and many are missing

Don't spot-fix. **Regenerate the entire spec from scratch** using the JSON IR.
It's faster and more reliable than hunting for which variables are missing.
The JSON is authoritative. The spec should reflect it completely.

### Blueprint is data-only (no logic)

Many BPs exist only to set CDO values. For these:
- The CDO Overrides section IS the spec. Make it thorough.
- Interpret curve data, struct literals, and asset references.
- Skip empty sections (Variables, Functions, Components if truly empty).
- Still need: `## Identity`, `## Purpose`, `## CDO Overrides`, `## Dependencies`, `## IR Notes`

### Blueprint has complex graph logic

For BPs with 50+ nodes, work graph-by-graph:
1. Start with Identity, Variables, Functions (table rows first)
2. EventGraph: find all K2Node_Event nodes → one section each
3. Follow execution chains from each event → write pseudo-code
4. Function graphs: one section each, describe what they compute
5. Don't try to document every node — focus on WHAT the logic does, not the mechanism

### IR has unsupported nodes

If `compilerIRFallback.hasUnsupportedNodes` is true:
- Note which node types are unsupported in `## IR Notes`
- Describe what the overall logic LIKELY does based on surrounding context
- Mark gaps explicitly: "IR gap: [NodeType] nodes not exported — logic here is inferred"

---

## Current State (as of last validator run)

**Pass rate: 100% (482/482) — PERFECT. ZERO ERRORS.**

All 482 matched specs pass. No remaining failures.

Root cause of the former "permanent" 3 failures was diagnosed and fixed:
UE DisplayName metadata trailing-space artifacts (e.g. "Follow Player ") were
preserved faithfully in the IR but the validator stripped spec names via .strip()
without stripping IR names — asymmetric comparison. Fixed by `_norm()=rstrip()`
in `validate_specs.py`. ANALYSIS_PLAYBOOK.md RULE 9 + SCHEMA_REFERENCE.md updated.
3 spec IR Notes updated to document the artifact for reconstruction completeness.
14/14 regression gates pass on fresh re-export.

Next: no spec quality work needed. Focus on next pipeline stage (LOOP C
meta-improvements or SYNTHESIZE phase).

*Update this section after every LOOP B cycle.*

---

## Validator Reference

```
validate_specs.py [spec_dir] [export_dir] [--fix-list path] [--report path]

Exit code 0: every spec has one exact serialized-name JSON pair and passes the
implemented static L1-L3 checks
Exit code 1: one or more specs fail static checks, have no exact pair, or have
ambiguous duplicate exact pairs

Output: _VALIDATION_FAILURES.md with summary table + per-BP top error
JSON report: optional, contains per-BP per-field diagnostic detail
```

The v2 result token is `STRUCTURAL_PASS` or `STRUCTURAL_FAIL`. It is never a
claim of Blueprint compilation, editor roundtrip, PIE/runtime behavior,
multiplayer correctness, persistence correctness, or C++ implementation. Those
surfaces remain explicitly `false` in the JSON report until separate tools run.

**Error severity:**
- `error` → causes FAIL, must fix for L1/L2/L3 compliance
- `warning` → causes "Pass with warnings", encouraged but not blocking

**Key L1 checks (errors):**
- Missing variable from detailedVariables[]
- Missing function from detailedFunctions[]
- CDO section absent when delta non-empty
- Missing Identity section (## Identity heading OR **Path:** + **Parent:** bold fields)
- IR has unsupported nodes but spec doesn't mention them
- Exact unsupported and partially supported node types missing from IR Notes
- Message-bearing node compiler diagnostic site missing by node GUID
- Missing or ambiguous exact `blueprintName` JSON pairing

**Key L1 checks (warnings):**
- Individual CDO property missing from CDO table
- Module missing from Dependencies
- No Purpose section
- No IR Notes section
- Component section absent

**L3 checks (errors):**
- Blueprint has EventGraph nodes but spec has no Logic section

---

## Quick Spec Template

For rapid spec production, start from this template and fill in each section:

```markdown
# <BlueprintName>

## Identity
**Path:** `/Game/...`
**Parent:** `<ParentClass>` (`/Script/<Module>.<ParentClass>`)
**Generated Class:** `<BlueprintName>_C`
**Type:** <type>

## Purpose
<1-3 sentences: what it does, why it exists, what system owns it>

## CDO Overrides (vs Parent)
| Property | Value | Notes |
|----------|-------|-------|
| ... | ... | ... |

(Or: "Inherits all defaults from parent.")

## Variables
| Name | Type | Default | Replicated | Specifiers |
|------|------|---------|------------|------------|
| ... | ... | ... | No | ... |

## Functions
| Name | Access | Pure | Net | Params | Notes |
|------|--------|------|-----|--------|-------|
| ... | Public | No | None | ... | ... |

## Components
<tree or "None">

## Logic
<only if totalNodeCount > 0>

### Event: <EventName>
```
<EventName>(<params>)
  → <what happens>
  → if <condition>:
      → <then branch>
    else:
      → <else branch>
```

## Dependencies
**Modules:** <all from dependencyClosure.moduleNames>
**C++ Types:** <from classPaths, structPaths, enumPaths>
**Assets:** <from assetPaths>

## IR Notes
Full coverage — no unsupported nodes.
```

---

*This document is part of the PRIME DIRECTIVE Context Engineering System.*
*Update it after each iteration to reflect the current state of the corpus.*
*Update LOOP C guidance after each meta-improvement cycle to make the next cycle smarter.*
