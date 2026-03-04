# kc-flow Renderer Spec

This document defines the minimal intermediate representation (IR) and
renderer contract to compile a flow into terminal-executable scripts.

## Objective

- Keep contracts as canonical source (`key=value`).
- Keep chaining semantics reproducible and readable.
- Keep execution target as terminal scripts, not general programming language runtimes.

## Scope

- Source: contract/flow definitions.
- IR: terminal-agnostic execution graph.
- Render target: command line scripts.

Current target order:

1. `bash` (reference backend)
2. `powershell`
3. optional `cmd` (limited)

## Core Principle

`kc-flow` is an orchestrator/compiler for scripts. It is not a programming
language. Business logic remains inside each node script/binary.

## IR Model (Terminal Agnostic)

The compiler must normalize a flow into:

1. Nodes
2. Directed links
3. Resolved inputs per node
4. Topological stages for parallel-safe execution

### Node Record

Required fields:

- `node_id`
- `contract_ref`
- `command_template`
- `required_inputs[]`
- `declared_outputs[]`

### Link Record

Required fields:

- `from_ref` (`input.*` or `node.<id>.out.*`)
- `to_ref` (`node.<id>.in.*` or `output.*`)

### Stage Record

Required fields:

- `stage_id`
- `nodes[]`

Rule:

- Nodes inside one stage have no dependency edge between them and are runnable
  in parallel.
- Linear chains are always sequential (`A -> B -> C`).
- Parallel execution appears only after fan-out from the same source branch.

## Runtime I/O Contract

Parent and child contracts/scripts interoperate through text conventions.

Input convention:

- Parent provides values through CLI assignments, for example
  `--set input.prompt=hello`.

Output convention:

- Child emits `output.<id>=<value>` lines to stdout.
- Large/binary payloads may be represented as paths:
  `output.image=/tmp/file.png`.

Parse contract:

- Parent captures stdout and extracts `output.*` lines only.
- Unknown lines are ignored unless strict mode is enabled.

## Renderer Contract

A renderer receives IR and emits an executable script that can run without
`kc-flow` as a runtime requirement (unless explicitly chosen).

Suggested export flag name:

- `--cli` (for command-line render/export)

Renderer responsibilities:

1. Materialize node invocations as terminal commands.
2. Store node outputs in script variables or temp files.
3. Apply links by wiring source output values to destination input values.
4. Execute each stage sequentially, with intra-stage parallelism.
5. Fail on missing required inputs at execution boundary.

## Bash Backend Rules (Reference)

- Script header: `#!/usr/bin/env bash` + `set -euo pipefail`.
- One function per node run is allowed but not required.
- Stage execution:
  - sequential across stages
  - parallel inside stage using `&` and `wait`
- Node stdout is captured, parsed for `output.*`.
- Handoff map can be implemented as variables and/or temp files.

## PowerShell Backend Rules

- Script header: `Set-StrictMode -Version Latest`.
- Stage execution:
  - sequential across stages
  - parallel inside stage via background jobs or thread jobs.
- Same `output.*` parsing contract as bash backend.

## Determinism Requirements

- Same source flow + same inputs + same backend => semantically equivalent
  execution graph and wiring.
- Node order inside the same stage must be stable (sorted by `node_id` or
  source index).
- Output key collisions are explicit errors unless merge behavior is declared.

## Parallelism Policy

- No `async` flag is required in the contract.
- No global concurrency cap is required in the contract.
- Readiness is dependency-driven only:
  - if a node depends on another node output, it is sequential.
  - if two nodes depend on the same resolved source and not on each other,
    they are parallel branches.
- Resource control is solved by topology design, not by runtime policy fields.

## Error Model

Compilation errors:

- invalid node reference
- invalid endpoint format
- cycle in graph
- unresolved required input

Runtime errors:

- node process failed
- missing expected output key
- invalid output line format (strict mode)

## Minimal End State

When this spec is implemented, a flow can be compiled into a concrete terminal
script that is:

- human-readable
- copy/paste runnable
- reproducible from source contracts
- independent from `kc-flow` for final execution
