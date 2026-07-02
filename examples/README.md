# MODT Example Projects

This folder contains larger, multi-file MODT projects intended to show how the language scales beyond the focused tests in `Testing/`.

## Included examples

- **commerce-platform** — marketplace domain with catalog, customer accounts, ordering, payments, fulfillment, returns, SQL metadata, and workflow modeling. See [examples/commerce-platform/README.md](examples/commerce-platform/README.md) for a visual walkthrough of the generated outputs.
- **hospital-operations** — clinical scheduling and care-delivery model with inheritance, actors, stateful appointments, and cross-team relationships. See [examples/hospital-operations/README.md](examples/hospital-operations/README.md).
- **fleet-operations** — dispatch and maintenance model for a transportation fleet with trip state machines, maintenance workflows, and many-to-many certifications. See [examples/fleet-operations/README.md](examples/fleet-operations/README.md).

## How to run an example

From the repository root, build MODT and point it at an example directory:

```bash
./modt --input examples/commerce-platform
./modt --input examples/hospital-operations
./modt --input examples/fleet-operations
```

Each example includes an `artifacts` block, and the generated output is intentionally checked in under that example's `generated/` folder so users can inspect the resulting SQL, documentation, and PlantUML before adopting the tool.

## What to inspect first

- Start with [examples/commerce-platform/README.md](examples/commerce-platform/README.md) if you want the fullest visual overview in one place.
- Use each example's `generated/docs/*.md` file when you want the corresponding generated documentation snapshot.
- Use the `generated/activity`, `generated/ssd`, `generated/sequence`, and `generated/state` folders to compare how one use case is projected into different views.
- Behavior models use readable text literals, for example `step [Submit order]`, so generated diagrams and docs show natural language instead of compressed one-word labels.

## Design goals

These examples intentionally combine several features in one place:

- multi-file project layout
- domain and design modeling in the same project
- SQL-oriented metadata and relationship mapping
- use cases for activity and sequence generation
- stateful objects for state machine generation
- inheritance, stereotypes, and realistic entity boundaries
- checked-in generated outputs as reviewable snapshots
