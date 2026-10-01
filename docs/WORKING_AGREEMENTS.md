# AI and Engineering Working Agreements

This document is the shared operating contract for human contributors, Claude,
Codex, and other engineering agents working in this repository.

## Source-of-truth order

When sources disagree, use this order:

1. verified code, tests, and captured hardware evidence;
2. accepted architecture decision records and locked requirements;
3. current status, roadmap, and hardware documents;
4. open issues and pull requests;
5. conversation history.

Conversation history is useful context but is not a durable project record.
If `STATUS.md` conflicts with verified code or evidence, update or flag the
status document rather than silently relying on it.

## Claim states

Label consequential claims using one of these states:

- **Observed:** directly measured or reproduced; link the artifact or procedure.
- **Inferred:** reasoned from observations; state the reasoning and uncertainty.
- **Proposed:** a candidate awaiting review or a decision gate.
- **Accepted:** approved for implementation or baselining.
- **Rejected/Superseded:** retained for traceability but no longer active.

Vendor specifications are sourced facts, not project verification. Record the
source URL and access date, then bench-verify claims that affect the design.

## Cross-repository ownership

The repository running an experiment owns its procedure, raw outputs, failures,
and conclusions. PlatypusOne links that evidence and makes its own product
decision through an ADR, requirements update, and BOM review.

Do not copy a prototype conclusion into PlatypusOne as an accepted requirement.
Do not change the BOM merely because a related experiment worked.

For the current depth work:

- Project Platypus owns Tab5/M024/VL53L8CX experiments and evidence.
- PlatypusOne owns its camera/depth architecture, requirements, and BOM.
- VL53L8CX and VL53L9CX remain candidates until the depth-sensing ADR gate
  passes.

## Branch and work ownership

- Inspect open pull requests before starting work.
- Use one purpose per branch and avoid editing another active agent's branch
  unless the pull request explicitly owns the follow-up.
- State stacked branch dependencies in the pull request body.
- Prefer small slices that can be reviewed and merged independently.
- Never resolve an unknown by silently choosing a value. Record it as a gate,
  assumption, or blocker.

## Hardware and evidence rules

A hardware conclusion must identify:

- exact hardware and revision;
- wiring, supplies, firmware, and configuration;
- date and environmental conditions;
- procedure and raw artifact locations;
- expected and actual results;
- failures, invalid data, and limitations;
- whether the result is sufficient to promote a product decision.

Keep raw evidence immutable. Derived summaries must link back to it. Hardware
that has not been exercised is **not verified**, even if its driver builds or a
vendor evaluation board works.

## Decision promotion

Use this sequence for consequential hardware choices:

1. record the candidate and the question;
2. define the acceptance test;
3. capture raw evidence in the experiment-owning repository;
4. summarize what was and was not proven;
5. review through a PlatypusOne ADR;
6. only then update locked requirements, the BOM, CAD envelopes, or production
   defaults.

## Required pull-request handoff

Every agent-authored pull request ends with:

```markdown
## Handoff

- Changed:
- Verified:
- Not verified:
- Decisions made:
- Assumptions:
- Blockers:
- Cross-repository effects:
- Next safe action:
```

“Verified” must name the command, test, inspection, or hardware procedure.
Research-only work says what was source-verified and what still requires a
bench test.
