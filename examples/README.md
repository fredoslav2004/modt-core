# MODT Example Projects

This folder contains larger, multi-file MODT projects intended to show how the language scales beyond the focused tests in `Testing/`.

## Included examples

- **commerce-platform** — marketplace domain with catalog, customer accounts, ordering, payments, fulfillment, returns, SQL metadata, and workflow modeling.
- **hospital-operations** — clinical scheduling and care-delivery model with inheritance, actors, stateful appointments, and cross-team relationships.
- **fleet-operations** — dispatch and maintenance model for a transportation fleet with trip state machines, maintenance workflows, and many-to-many certifications.

## How to run an example

From the repository root, build MODT and point it at an example directory:

```bash
./modt --input examples/commerce-platform
./modt --input examples/hospital-operations
./modt --input examples/fleet-operations
```

Each example includes an `artifacts` block, and the generated output is intentionally checked in under that example's `generated/` folder so users can inspect the resulting SQL, documentation, and PlantUML before adopting the tool.

## Design goals

These examples intentionally combine several features in one place:

- multi-file project layout
- domain and design modeling in the same project
- SQL-oriented metadata and relationship mapping
- use cases for activity and sequence generation
- stateful objects for state machine generation
- inheritance, stereotypes, and realistic entity boundaries
- checked-in generated outputs as reviewable snapshots
