---
title: "MODT - Modeling Tool Manual"
subtitle: "Version 1.2.5"
author: "The MODT Team"
date: "March 10, 2026"
---
**Manual Version 1.2.5**

# Introduction

**MODT** (Modeling Tool) is a high-level modeling language and command-line utility designed to bridge the gap between abstract software design and concrete implementation. By using a simple, human-readable syntax, MODT allows you to define system structures, behaviors, and relationships.

## Key Features

- **PlantUML Diagrams**: Design, Domain, Activity, System Sequence, Sequence, and State.
- **Database Schemas**: Automated SQL DDL generation with intelligent mapping.
- **Documentation**: High-quality Markdown and PDF generation.
- **Modular**: Full support for cross-file processing and large-scale modeling.

---

# Getting Started

## Installation

MODT is distributed as pre-compiled binaries and packages for major platforms:

- **Linux**: Use `.rpm` or `.deb` packages from the releases page.
- **Windows**: Use the installer or standalone executable from the releases page.

## Your First Model

The fastest way to start is to let MODT scaffold a tiny starter project for you:

`modt hello-world`

This creates `hello.modt` in the current folder together with a small `README.md`. From there, just run `modt` in the same folder and MODT will use the current directory as input automatically.

If you prefer to create the file yourself, use this minimal example:

Create a file named `hello.modt`:

```modt
system
    name HelloMODT
    title My First Project

obj User
    name: string
    login()

obj Database
    rel "uses" -- User
```

Run MODT with any of these forms:

- `modt hello.modt`
- `modt --input hello.modt --interactive`
- `modt` from a folder that already contains `.modt` files

---

# Model Structure

## The System Block

The `system` block defines project-wide metadata.

```modt
system
    name ProjectID        # Internal identifier used for filenames
    title "System Title"  # Displayed in diagram titles and document headers
    description "..."     # Brief summary of the project
```

## The Artifacts Block

The `artifacts` block specifies what should be generated and where.

```modt
artifacts
    domain path/ [fmt]    # Domain Model (Analysis Class Diagram)
    design path/ [fmt]    # Design Class Diagram
    sql file.sql          # SQL DDL Script
    docs path/            # Markdown Documentation
    activity path/ [fmt]  # Activity Diagrams (per use case)
    ssd path/ [fmt]       # System Sequence Diagrams
    sequence path/ [fmt]  # Full Sequence Diagrams
    state path/ [fmt]     # State Machine Diagrams
```

* **Path**: Can be a directory (for multiple diagrams) or a specific file (for combined output like `sql` or `docs`).
* **Format `[fmt]`**: Optional. Supported: `svg` (default), `png`, `pdf`, `txt`.
* **Generated file names**:
    * `design` -> `<Project>.design.puml`
    * `domain` -> `<Project>.domain.puml`
    * `activity` -> `<Project>.activity.puml`
    * `ssd` -> `<Project>_<UseCase>.ssd.puml`
    * `sequence` -> `<Project>_<UseCase>.sequence.puml`
    * `state` -> `<Project>_<Class>.state.puml`

---

# Objects and Members

## Object Definition

Objects are defined using the `obj` keyword. They can include inheritance and stereotypes.

```modt
obj Admin -|> User [actor]
    # Members here...
```

* **Inheritance**: Use `-|>` followed by the parent class name.
* **Stereotypes**: Enclosed in square brackets. `[actor]` renders the class as an actor in UML.

## Attributes and Metadata

Attributes follow the format `name : type [metadata]`.

```modt
obj Product
    -sku : string [db(VARCHAR(50))] # Private attribute with SQL override
    +inStock : bool [state, initial(true)] # Public state variable
```

* **Visibility**: `+` (Public), `-` (Private), `#` (Protected), `~` (Package).
* **Types**: Common types like `string`, `int`, `bool`, `long`, `decimal`.
* **Metadata**:
    * `[db(Type)]`: Overrides the generated SQL data type.
    * `[state]`: Marks an attribute as a state for state machine generation.
    * `[initial(value)]`: Sets the initial state value. This can be boolean (`true` / `false`) or symbolic (`Draft`, `Requested`, `Packed`, etc.).

## Methods

Methods are defined with parentheses and can include parameters, visibility, and state-effect metadata. Logic can be further detailed with indented `pre` and `post` blocks.

```modt
obj LightSwitch
    attr IsOn: bool [initial(false), state]

    method Toggle() [set(IsOn, !IsOn, Click)]
        pre !IsOn
        post IsOn

    method TurnOff() [set(IsOn, false, Click, true)]
        pre IsOn
        post !IsOn
```

* **Syntax**: `[+|-|#|~] [method] Name(param1: type, param2) [metadata]`
* **Parameters**: Comma-separated list. Types are optional.
* **Indented Conditions**:
  * `pre <condition>`: Defines what must be true before the method is called.
  * `post <condition>`: Defines what is true after the method completion.
* **State Effects**: Use `[set(variable, targetValue, trigger, fromValue)]` metadata.
    * `variable`: The state attribute name to change (must have `[state]` metadata).
    * `targetValue`: The new value. This may be boolean (`true` / `false`), symbolic (`Paid`, `Delivered`, `Captured`), or a toggle expression such as `!variable`.
    * `trigger`: Optional transition event label.
    * `fromValue`: Optional. Specifies the source state. If omitted, MODT deduces it from method `pre` conditions when possible.

---

# Behavioral Modeling: State Diagrams

MODT can automatically generate State Machine diagrams for objects.

- Mark at least one attribute with `[state]`.
- Use `[initial(value)]` to define the starting state.
- Methods with `[set(...)]` metadata or explicit `pre`/`post` conditions will generate transitions.
- Symbolic state values are preserved in the output. For example, `status [state, initial(Draft)]` with `set(status, Submitted, ...)` produces `Draft --> Submitted`, not placeholder nodes like `status` / `Not_status`.
- Use-case-driven transitions are scoped to explicitly qualified references such as `Order.status == Draft`, so states from one class do not leak into another class that happens to use the same attribute name.
- Transition discovery logic:
    1. Uses `fromValue` from `set()` metadata if present.
    2. Otherwise, matches method `pre` conditions to infer the source state.
    3. Uses method target values and use-case `post` conditions to infer destination states.
    4. For boolean state toggles, MODT can still produce `Not_<StateName>` style nodes when the model truly describes a boolean on/off state.

---

# Phases and Visibility

MODT supports two modeling phases: **Analysis** (`[a]`) and **Design** (`[d]`).

- By default, classes, attributes, methods, and enums are included in both phases.
- Use `[a]` or `[analysis]` to restrict an element to the Domain Model.
- Use `[d]` or `[design]` to restrict an element to the Design Class Diagram / SQL-oriented design output.
- Using `[a, d]` is equivalent to omitting the phase tag entirely; it is valid but redundant.

```modt
obj Account
    balance [a]         # Shown in Domain model
    -password [d]       # Shown in Class diagram
    +id [a, d]          # Shown in both
```

---

# Relationships

Connect objects using the `rel` keyword.

## Relationship Syntax

`rel From ["fLabel"] type ["tLabel"] To [: label]`

| Type  | UML Meaning |
| :---- | :---------- |
| `--`  | Association |
| `<--` | Directed Association |
| `-->` | Directed Association |
| `..>` | Dependency |
| `*--` | Composition |
| `o--` | Aggregation |

## Multiplicity and Junction Tables

MODT's SQL engine automatically handles multiplicity labels:

- **1 to N**: `rel Company "1" -- "*" Employee` adds a `Company_id` to the `Employee` table.
- **M to N**: `rel Student "*" -- "*" Course` generates a junction table `Student_Course`.

Example of indented relationship:

```modt
obj Order
    rel "*" -- "1" Customer : placed by
```

---

# Behavioral Modeling

### Use Cases

Use cases define the system's dynamic behavior and are used to generate Activity, Sequence, and State diagrams.

```modt
uc ProcessOrder
    actor Clerk
    description "Processes a customer order"
    pre Order.Status == Pending
  
    step checkInventory
    alt [Out of stock] goto replenishment
  
    step createInvoice :> @sys
        - orderId
        - amount
  
    post Order.Status == Invoiced
```

### Steps and Control Flow

- **`step`**: A simple action.
- **`:> Target`**: Specifies the target of an action.
    * In **System Sequence Diagrams**, only actor <-> system boundary messages are shown.
    * In **Sequence Diagrams**, named collaborator targets are shown explicitly.
    * `@sys` represents the system.
    * `@user`, `@actor`, and `@me` indicate an explicit response back to the actor.
    * Any other target name (for example `PaymentGateway`, `SchedulingService`, `WarehouseService`) becomes a collaborator in full Sequence Diagrams.
- **`alt [Condition]`**: An alternative branch.
- **`goto @Label`**: Jump to a step marked with `@Label`.
- **`@Label`**: Define a target for jumps within a step name (e.g., `step @start enterData`).
- Combine `alt`, `goto`, and labels when you need richer flow control such as retries, optional blocks, and converging paths.
- A backward jump like `alt [Invalid input] goto @start` models retry loops well in activity diagrams.
- Forward-skip patterns like `alt [Condition] goto @laterStep` are useful for activity flow, but in interaction diagrams they render best when the skipped block is the optional work. In other words, keep SSD/SD-friendly use cases mostly linear and use labeled jumps to skip optional blocks, not to hide the main happy path.
- **Parameters**: Indented lines starting with `-` denote data passed during a step.

Example of a more complex, activity-oriented use case:

```modt
uc Authenticate
    actor User

    @start step enterCredentials
    step validate :> @sys

    alt [Invalid] goto @start
    alt [Locked] goto @help

    step createSession :> SessionService
    step showDashboard :> @user

    @help step showSupportContact :> @user
```

- In `activity`, this becomes a proper branched control flow with retry and alternate exit paths.
- In `ssd`, only actor/system boundary messages remain visible.
- In `sequence`, named collaborators are shown, but system-only control flow is intentionally projected into a cleaner interaction view rather than reproducing the full activity graph verbatim.

### System Sequence Diagrams vs Sequence Diagrams

MODT now distinguishes between two different outputs for behavioral interactions:

- **System Sequence Diagrams (`ssd`)**
    * Title format: `System Sequence Diagram: <UseCase>`
    * Show only messages that cross the actor/system boundary.
    * Useful during analysis and requirements discussion.
    * Internal orchestration and collaborator calls are intentionally hidden.

- **Sequence Diagrams (`sequence`)**
    * Title format: `Sequence Diagram: <UseCase>`
    * Show the fuller collaboration flow, including named services and downstream participants.
    * Do not render a synthetic `System` participant; steps without a named collaborator are annotated on the current lifeline instead.
    * Forward-skip alternatives such as `alt [Condition] goto @Label` are rendered as optional blocks around the skipped interaction region.
    * Useful during design, integration planning, and service interaction modeling.

Example:

```modt
uc Checkout
    actor Customer
    step submitOrder :> @sys
        - cartToken
    step authorizePayment :> PaymentGateway
        - amount
    step sendConfirmation :> @user
        - orderId
```

- In `ssd`, this becomes an actor-to-system request followed by a system-to-actor confirmation.
- In `sequence`, this also includes `Actor -> PaymentGateway : authorizePayment(amount)`.

### State Transitions

If a class has a `[state]` attribute, MODT identifies transitions by matching `pre` and `post` conditions in use cases to `ClassName.AttributeName`.

---

# Internal Generation Logic

### SQL Engine

- **Auto-ID**: Adds `id INT PRIMARY KEY AUTO_INCREMENT` if no "id" is defined.
- **Inheritance**: Implements Table-per-Class using a `parent` FK.
- **M:N Detection**: Detects `*` on both sides of a relationship and creates a junction table.

### PlantUML Engine

- **Domain vs Design**: Filters members based on `[a]` / `[d]` tags.
- **Activity Diagrams**: Uses `partition` for each use case and `if/elseif` for alternatives.
- **Behavior Abstraction**: Treat activity diagrams as the richest control-flow projection; SSDs and full sequence diagrams are intentionally simpler communication views derived from the same use case model.
- **SSDs**: Show only actor/system boundary interactions.
- **Sequence Diagrams**: Show the fuller interaction flow, including named collaborators and collaborator-to-collaborator handoffs, while annotating system-only steps without adding a synthetic `System` participant.
- **State Diagrams**: Preserve symbolic lifecycle values when the model expresses them explicitly, and scope use-case state transitions to `ClassName.AttributeName` references.

---

# Advanced Features

## PlantUML Customization

Inject raw PlantUML code using `@puml-head`.

```modt
@puml-head !theme materia
@puml-head skinparam classBackgroundColor #EEE
```

## Cross-File Support

Point MODT to a directory instead of a file: `modt --input ./models`. MODT merges all `.modt` files in the directory.

If you are already inside that directory, you can usually just run `modt` and MODT will infer the current folder as the input when it finds `.modt` files.

- Objects defined in one file can be referenced or inherited in another.
- Artifacts are generated from the combined model.

---

# Complete Export Example

Below is a complete example showing a `.modt` file and its various outputs.

### Input: `example.modt`

```modt
system
    name Shop
    title Online Store

artifacts
    sql db.sql
    docs manual.md

obj Product
    name: string
    price: decimal

obj Order
    status: string [state, initial(Pending)]
    rel "*" -- "1" Product
    cancel() [set(status, Cancelled)]

uc Checkout
    actor Customer
    pre Order.status == Pending
    step pay :> @sys
    post Order.status == Paid
```

### Export: `db.sql`

```sql
-- SQL Schema generated by MODT

CREATE TABLE Product (
    id INT PRIMARY KEY AUTO_INCREMENT,
    name VARCHAR(255),
    price decimal
);

CREATE TABLE Order (
    id INT PRIMARY KEY AUTO_INCREMENT,
    status VARCHAR(255),
    Product_id INT,
    FOREIGN KEY (Product_id) REFERENCES Product(id)
);
```

### Export: `manual.md` (Excerpt)

```markdown
# Project Documentation: Online Store

## Use Cases

### Checkout
**Actor:** Customer

**Preconditions:**
- `Order.status == Pending`

**Flow of Events:**
1. pay (Target: @sys)

**Postconditions:**
- `Order.status == Paid`
```

---

# Examples

The repository includes larger worked examples under the `examples/` folder.

- Each example keeps its generated outputs checked in so you can inspect the result before adopting the tool.
- The examples generate both:
    * **System Sequence Diagrams** in `generated/ssd/`
    * **Sequence Diagrams** in `generated/sequence/`

This makes it easier to compare analysis-oriented actor/system interactions with richer design-oriented interaction flows.

---

# CLI Reference

The MODT command-line interface provides several flags to control output generation.

| Command | Description |
| :------ | :---------- |
| `hello-world` | Scaffold a starter MODT project here |
| `<path>` | Single positional input file or directory |
| `--input <path>` | Input .modt file or directory |
| `--out-path <dir>` | Base output directory |
| `-genDesign`, `-genPUML` | Generate Design Class Diagrams |
| `-genDomain`, `-genDomainModel` | Generate Domain Model Diagrams |
| `-genActivity` | Generate Activity Diagrams from use cases |
| `-genSSD` | Generate System Sequence Diagrams |
| `-genSequence` | Generate full Sequence Diagrams |
| `-genState` | Generate State Machine Diagrams |
| `-genSQL` | Generate SQL/DDL Schema |
| `-genDocs` | Generate Markdown documentation |
| `-i`, `--interactive` | Start in interactive mode to pick artifacts |
| `-h`, `--help` | Show CLI help |

When no explicit input path is provided, MODT checks the current working directory. If it finds `.modt` files there, it uses that directory as the input automatically.
