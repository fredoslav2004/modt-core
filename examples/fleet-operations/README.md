# Fleet Operations

A transportation and maintenance example that models:

- depots, drivers, vehicles, and electric-vehicle specialization
- trip dispatch and route execution workflows
- telemetry alerts and maintenance work orders
- many-to-many certifications between drivers and vehicles
- stateful assets that transition between dispatch and maintenance modes

Run from the repository root:

```bash
./modt --input examples/fleet-operations
```

Generated files land in `examples/fleet-operations/generated/` and are checked into the repository as an example snapshot.
