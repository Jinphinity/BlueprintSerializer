# validate_specs v2 regression suite

This suite locks the static validator repairs released as
`validate_specs.py` `2.0.0`. Run it from any directory:

```powershell
python Plugins/BlueprintSerializer/Tests/ValidateSpecs/run_regressions.py
```

## Repaired defects

| Defect | Regression |
|---|---|
| `ir_notes_text` was never populated | Exact unsupported and partial types are read from `## IR Notes`. |
| Prefix glob could pair `BP_Inventory_Container` to `BP_Inventory_Container_Abstract` | Pairing uses exact serialized `blueprintName`; zero and duplicate exact matches fail. |
| Pipe-table parser split code-span or escaped pipes | Variable/function names containing `|` survive parsing. |
| Branch heuristic counted a parsed dictionary | Conditions are counted only in source Markdown. |
| Relative links did not handle encoded spaces/fragments/code | URL-decoded local paths resolve relative to the spec; fenced/inline code and external URLs are excluded. |
| Only a vague unsupported-node flag was checked | Every exact unsupported and partially supported node type must be classified in IR Notes. |
| Compiler messages were not an enforced evidence surface | Every nonempty error/upgrade-message site must be disclosed by node GUID. Empty nonzero `ErrorType` markers are counted separately. |
| PASS wording could be mistaken for runtime closure | Reports emit `STRUCTURAL_PASS`/`STRUCTURAL_FAIL` and keep compile, roundtrip, PIE/runtime, multiplayer, persistence, and C++ claims false. |
| Unmatched specs could still yield exit 0 | Missing/ambiguous/invalid pairings make the corpus result fail. |

The committed versioned receipts are
[`validate_specs_v2_regression_receipt.json`](receipts/validate_specs_v2_regression_receipt.json)
and
[`validate_specs_v2_regression_failures.md`](receipts/validate_specs_v2_regression_failures.md).

## Historical receipt preservation

The v2 release does not rewrite prior donor receipts. The following known
validator-defect receipts in the enclosing Lyra catalog retain their original
bytes:

| Catalog path | Bytes | SHA-256 |
|---|---:|---|
| `LYRA_PRIMARY_GOAL/BLUEPRINT_SPECS/hyper-slot-inventory-3/_VALIDATION_REPORT.json` | 2,088 | `5F4E1953A91797021F4BB6A79CC5A0A2A7603C7E01E7EC57F02A88599EC9C044` |
| `LYRA_PRIMARY_GOAL/BLUEPRINT_SPECS/hyper-slot-inventory-3/_VALIDATION_FAILURES.md` | 544 | `77D5D903C36A31762160ADC8CF6F60FEFB3896AFFFA303506F2DA655B2227C3F` |
| `LYRA_PRIMARY_GOAL/BLUEPRINT_SPECS/rpg-engine-7/_BP_GAME_INSTANCE_RPG_VALIDATION_REPORT.json` | 1,586 | `62F84AAA97D83B7162FBCC3117D3987C7579541EC77760EEDCE458C7B49BAE52` |
| `LYRA_PRIMARY_GOAL/BLUEPRINT_SPECS/rpg-engine-7/_BP_GAME_INSTANCE_RPG_VALIDATION_FAILURES.md` | 540 | `D73B64708954564724A137F9049750D9603105B906F0754070A4E5D5A691ABEB` |
| `LYRA_PRIMARY_GOAL/BLUEPRINT_SPECS/rpg-engine-7/RPG_QUEST_OWNER_VALIDATION_REPORT.json` | 1,519 | `E4842A83084005F3A5FC81A5B4A4428A18739C82087BEDB5152BE327CB147A80` |
| `LYRA_PRIMARY_GOAL/BLUEPRINT_SPECS/rpg-engine-7/RPG_QUEST_OWNER_VALIDATION_FAILURES.md` | 536 | `D4B42FB243DFB61985A15A59752796190812E20C09DB50FE457FDB70C965D943` |

New donor revalidations must use new versioned filenames beside these historical
receipts. A corrected v2 result does not retroactively turn an old tool run into
a pass.

No BlueprintSerializer C++ source is changed by this validator release, and
Unreal is not launched by the suite.
