---
title: "MODT - Modeling Tool Manual"
subtitle: "Version 1.2.3"
author: "The MODT Team"
date: "March 10, 2026"
---
**Manual Version 1.2.3**

Introduction

**MODT** (Modeling Tool) is a high-level modeling language and command-line utility designed to bridge the gap between abstract software design and concrete implementation. By using a simple, human-readable syntax, MODT allows you to define system structures, behaviors, and relationships.

## Key Features

- **PlantUML Diagrams**: Class, Domain, Activity, Sequence, State.
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

Run MODT: `modt --input hello.modt --interactive` and choose your outputs.

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
    state path/ [fmt]     # State Machine Diagrams
```

* **Path**: Can be a directory (for multiple diagrams) or a specific file (for combined output like `sql` or `docs`).
* **Format `[fmt]`**: Optional. Supported: `svg` (default), `png`, `pdf`, `txt`.

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
  * `[initial(true/false)]`: Sets the initial value for a state variable.

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
  * `variable`: The boolean attribute name to change (must have `[state]` metadata).
  * `targetValue`: The new value. Use `true`/`false` for booleans, or `!variable` to toggle.
  * `trigger`: Optional transition event label.
  * `fromValue`: Optional. Specifies the source state. If omitted, MODT deduces it from method `pre` conditions or uses `[*]`.

---

# Behavioral Modeling: State Diagrams

MODT can automatically generate State Machine diagrams for objects.

- Mark at least one attribute with `[state]`.
- Use `[initial(value)]` to define the starting state.
- Methods with `[set(...)]` metadata or explicit `pre`/`post` conditions will generate transitions.
- Transition discovery logic:
  1. Uses `fromValue` from `set()` metadata if present.
  2. Otherwise, matches `pre` conditions against the variable name.
  3. Otherwise, defaults to an "Initial" or "Any" state transition.

---

# Phases and Visibility

MODT supports two modeling phases: **Analysis** (`[a]`) and **Design** (`[d]`).

- By default, all members are included in both.
- Use `[a]` or `[analysis]` to restrict to Analysis (Domain Model).
- Use `[d]` or `[design]` to restrict to Design (Class Diagram).

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

| Type    | UML Meaning          |
| :------ | :------------------- |
| `--`  | Association          |
| `<--` | Directed Association |
| `-      | >`                   |
| `..>` | Dependency           |
| `*--` | Composition          |
| `o--` | Aggregation          |

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
- **`:> Target`**: Specifies the target of an action (used in SSDs as `System -> Actor` or `Actor -> System`). `@sys` represents the system.
- **`alt [Condition]`**: An alternative branch.
- **`goto @Label`**: Jump to a step marked with `@Label`.
- **`@Label`**: Define a target for jumps within a step name (e.g., `step @start enterData`).
- **Parameters**: Indented lines starting with `-` denote data passed during a step.

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
- **SSDs**: Infers direction based on step target.

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

# CLI Reference

The MODT command-line interface provides several flags to control output generation.

| Command                             | Description                                 |
| :---------------------------------- | :------------------------------------------ |
| `--input <path>`                  | Input .modt file or directory               |
| `--out-path <dir>`                | Base output directory                       |
| `-genDesign`, `-genPUML`        | Generate Design Class Diagrams              |
| `-genDomain`, `-genDomainModel` | Generate Domain Model Diagrams              |
| `-genActivity`                    | Generate Activity Diagrams from use cases   |
| `-genSSD`                         | Generate System Sequence Diagrams           |
| `-genState`                       | Generate State Machine Diagrams             |
| `-genSQL`                         | Generate SQL/DDL Schema                     |
| `-genDocs`                        | Generate Markdown documentation             |
| `-i`, `--interactive`           | Start in interactive mode to pick artifacts |
