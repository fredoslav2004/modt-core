# MODT

![License: GPL v3](https://img.shields.io/badge/license-GPLv3-blue.svg)
![C++23](https://img.shields.io/badge/C%2B%2B-23-00599C.svg)
![CLI](https://img.shields.io/badge/interface-CLI-4c1.svg)
![Outputs](https://img.shields.io/badge/outputs-UML%20%7C%20SQL%20%7C%20Docs-6f42c1.svg)

**MODT** is a modeling language and CLI that turns concise, text-based system models into useful engineering artifacts: UML diagrams, SQL schema, and project documentation.

It is designed for teams that want modeling to stay versionable, reviewable, and close to the codebase instead of locked away in drag-and-drop tools.

## Why MODT

- **Text-first modeling** with a small, readable syntax that works well in Git
- **Multiple outputs from one source**: domain/design UML, activity, sequence, SSD, state, SQL, and docs
- **Scales from one file to many** with support for multi-file projects
- **Analysis and design phases** so conceptual and implementation views can coexist
- **Fast to adopt** with a `hello-world` scaffold and checked-in example projects

## What you get

From a single MODT project, you can generate:

- Domain and design class diagrams
- Activity, sequence, system-sequence, and state diagrams
- SQL DDL with metadata-driven type mapping
- Markdown documentation suitable for project handoff or review

## Installation

For most users, the best way to get started is to install a **prebuilt release** rather than building from source.

Release artifacts are already produced in [dist/releases](dist/releases) and can also be fetched through GitHub Releases.

### Recommended: install a release package

**Linux (.deb)**

```bash
sudo apt install ./modt_1.2.5_amd64.deb
```

**Linux (.rpm)**

```bash
sudo rpm -i ./modt_1.2.5_x86_64.rpm
```

**Linux (portable AppImage)**

```bash
chmod +x ./modt-1.2.5-x86_64.AppImage
./modt-1.2.5-x86_64.AppImage --help
```

**Windows**

- Use [dist/releases/modt-1.2.5-setup.exe](dist/releases/modt-1.2.5-setup.exe) for the installer experience
- Or use [dist/releases/modt-1.2.5-windows.zip](dist/releases/modt-1.2.5-windows.zip) for a portable bundle

### Build from source

If you want to hack on MODT itself or prefer a local build:

```bash
g++ -std=c++23 src/main.cpp src/Generator/DocGenerator.cpp src/Generator/PumlGenerator.cpp src/Generator/SqlGenerator.cpp src/Parser/Parser.cpp -o modt
```

## Quick start

### 1) Scaffold a starter project

```bash
./modt hello-world
```

### 2) Run MODT

```bash
./modt
./modt path/to/model.modt
./modt --input examples/commerce-platform
```

If you run `modt` inside a folder that already contains `.modt` files, MODT will automatically use the current directory as input.

## A tiny model

```modt
system
	name HelloMODT
	title My First Project

artifacts
	docs generated/docs/
	design generated/design/ [svg]
	sql generated/sql/hello.sql

obj User
	name: string
	login()

obj Database
	rel "uses" -- User
```

That single model can produce documentation, SQL, and diagrams.

## See the generated output

The repository includes checked-in example projects and their generated artifacts so you can evaluate MODT without running anything first.

### Commerce platform example

Source model:

- [examples/commerce-platform/00_system.modt](examples/commerce-platform/00_system.modt)

Generated outputs:

- [Documentation](examples/commerce-platform/generated/docs/commerce_platform.md)
- [SQL schema](examples/commerce-platform/generated/sql/commerce_platform.sql)
- [Domain diagram](examples/commerce-platform/generated/domain/CommercePlatform.domain.svg)
- [Design diagram](examples/commerce-platform/generated/design/CommercePlatform.design.svg)
- [Checkout sequence diagram](examples/commerce-platform/generated/sequence/CommercePlatform_Checkout.sequence.svg)
- [Order state diagram](examples/commerce-platform/generated/state/CommercePlatform_Order.state.svg)

![Commerce Platform Domain Model](examples/commerce-platform/generated/domain/CommercePlatform.domain.svg)

![Commerce Platform Checkout Sequence](examples/commerce-platform/generated/sequence/CommercePlatform_Checkout.sequence.svg)

### More example projects

| Project             | Focus                                                 | Source                                            | Generated docs                                                          | SQL                                                                    | Diagram preview                                                                    |
| ------------------- | ----------------------------------------------------- | ------------------------------------------------- | ----------------------------------------------------------------------- | ---------------------------------------------------------------------- | ---------------------------------------------------------------------------------- |
| Commerce Platform   | catalog, ordering, payments, fulfillment, returns     | [model](examples/commerce-platform/00_system.modt)   | [docs](examples/commerce-platform/generated/docs/commerce_platform.md)     | [sql](examples/commerce-platform/generated/sql/commerce_platform.sql)     | [domain](examples/commerce-platform/generated/domain/CommercePlatform.domain.svg)     |
| Fleet Operations    | dispatch, maintenance, work orders, trip states       | [model](examples/fleet-operations/00_system.modt)    | [docs](examples/fleet-operations/generated/docs/fleet_operations.md)       | [sql](examples/fleet-operations/generated/sql/fleet_operations.sql)       | [domain](examples/fleet-operations/generated/domain/FleetOperations.domain.svg)       |
| Hospital Operations | scheduling, care delivery, actors, stateful workflows | [model](examples/hospital-operations/00_system.modt) | [docs](examples/hospital-operations/generated/docs/hospital_operations.md) | [sql](examples/hospital-operations/generated/sql/hospital_operations.sql) | [domain](examples/hospital-operations/generated/domain/HospitalOperations.domain.svg) |

See [examples/README.md](examples/README.md) for a guided overview of the sample projects.

## Core concepts

- `system` defines project identity and description
- `artifacts` declares what to generate and where to write it
- `obj` defines model elements, members, inheritance, and stereotypes
- `rel` defines relationships and multiplicities
- `uc` defines behavior for activity, sequence, SSD, and state outputs

MODT supports both **analysis** and **design** phases, allowing one model to produce both conceptual and implementation-oriented views.

## Common workflows

### Generate everything declared by an example

```bash
./modt --input examples/commerce-platform
```

### Generate from the current directory

```bash
cd some-project-with-modt-files
../modt
```

### Explore the CLI

```bash
./modt --help
./modt --interactive
```

## Project layout

- [src](src) — parser, generators, and CLI entrypoint
- [examples](examples) — complete sample projects with checked-in outputs
- [Testing](Testing) — regression test runner and file-based test cases
- [Documentation.md](Documentation.md) — full language and CLI manual

## Development

Build from source:

```bash
g++ -std=c++23 src/main.cpp src/Generator/DocGenerator.cpp src/Generator/PumlGenerator.cpp src/Generator/SqlGenerator.cpp src/Parser/Parser.cpp -o modt
```

Run tests:

```bash
python3 Testing/test_runner.py
```

## Documentation

- [Documentation.md](Documentation.md) — full manual
- HTML and PDF version in every release

## License

MODT is licensed under the [GNU GPL v3](LICENSE).
