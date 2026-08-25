# Architecture Decision Records

Architecture-level decisions get an ADR here, as required by
[CODING_STANDARDS.md](../CODING_STANDARDS.md). An ADR captures *why* a choice
was made and what it forecloses, so a future reader (or a future contest judge)
does not have to reverse-engineer intent from the code.

## When to write one

Write an ADR when a decision:

- constrains more than one layer (`apps` / `services` / `platform`), or
- picks one hardware direction over another, or
- would otherwise be re-litigated every few weeks because the reasoning lives
  only in someone's head.

Routine implementation choices do not need an ADR — a comment in the header is
enough.

## Process

1. Copy [template.md](template.md) to `NNNN-short-kebab-title.md`, next number
   in sequence.
2. Open it as `Proposed`; move to `Accepted` when the work starts.
3. Never edit an accepted ADR's decision in place. Write a new ADR that
   supersedes it and mark the old one `Superseded by ADR-NNNN`.
4. Link the ADR from any doc whose content depends on it.

## Index

| ADR | Title | Status |
|---|---|---|
| [0001](0001-dynamic-linked-prototype-display.md) | Dynamic display target via a linked external prototype display | Accepted (2026-08-24) |
