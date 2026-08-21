# BP_TableLinks

## Identity

**Path:** `/Game/Test/BP_TableLinks.BP_TableLinks`

**Parent:** `Actor`

## Purpose

Exercises Markdown tables, links, support caveats, diagnostics, and source-prose branch coverage.

## Variables

| Name | Type | Notes |
|---|---|---|
| `Mode|State` | bool | Pipe inside a code span must stay in the name. |
| Pipe\|Name | bool | Escaped pipe must stay in the name. |

## Functions

| Name | Kind | Notes |
|---|---|---|
| `Resolve|State` | function | Code-span pipe regression. |

## Logic

### Event: BeginPlay

If the first condition is true, continue. If the second condition is false, use the fallback; else stop.

## IR Notes

- Unsupported node type: `K2Node_UnsupportedFixture`.
- Partially supported node type: `K2Node_PartialFixture`.
- Compiler diagnostic site: GUID `DIAGNOSTIC000000000000000000000001`; message `Fixture warning`.

## Evidence links

- [Angle-bracket path with spaces](<linked/evidence file.md#fixture-anchor>)
- [Percent-encoded path](linked/evidence%20file.md#fixture-anchor)
- [External URL](https://example.com/not-local)
- `[Inline-code link is not a link](missing-inline.md)`

```markdown
[Fenced-code link is not a link](missing-fenced.md)
```
