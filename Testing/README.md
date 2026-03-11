# Testing

MODT tests are now file-based instead of embedding escaped `.modt` source inside JSON.

## Case layout

Each test case lives in its own folder under `Testing/temp/<case-id>/`:

- `case.toml` — readable metadata and expectations
- `test.modt` or an input directory — input model source(s)
- generated outputs are written to `Testing/.runs/<case-id>/` during test execution

The checked-in test case stays small and readable: input plus focused assertions.

## `case.toml` example

```toml
version = 1
name = "DocGenerator: Basic Object"
description = "Generates basic Markdown documentation for a single object"
args = ["-genDocs"]
input = "test.modt"
tags = ["doc", "regression"]

[metadata]
id = "doc_basic_class"
owner = "core"
priority = "smoke"

[[outputs]]
file = "test.md"
contains = ["### Class: User"]
matches = ['\\| void login\\(string username\\) \\| unspecified \\|']
```

## Supported output checks

For each `[[outputs]]` entry:

- `file` — generated file name relative to the run output directory
- `contains` / `not_contains` — literal text checks
- `matches` / `not_matches` — regex checks

Snapshot comparisons are still supported by the runner when needed, but the default style is targeted assertions so tests only specify the parts of the output that matter.

## Legacy format

The runner still understands the old JSON format if needed, but discovery now prefers `case.toml` cases.

## Multi-file cases

- Set `input = "some-folder"` to pass a directory of `.modt` files to MODT.
- Add multiple `[[outputs]]` blocks to verify runs that generate several files, such as SSD or state diagram exports.
