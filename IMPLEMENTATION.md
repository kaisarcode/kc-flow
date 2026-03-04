# kc-flow Implementation

This document defines the concrete implementation path for `kc-flow` as a
script orchestrator and CLI renderer.

## 1. Runtime Role

`kc-flow` is not a programming language runtime.

`kc-flow` is responsible for:

- parsing contracts/flows (`key=value`)
- validating graph wiring
- building an execution graph
- executing/compiling CLI chaining

Node business logic belongs to node scripts/binaries.

## 2. Source Model

### 2.1 Atomic Contract

Atomic node contract keys:

- identity: `contract.id`, `contract.name`
- interface: `input.*`, `param.*`, `output.*`
- runtime: `runtime.script`, optional `runtime.exec`, `runtime.env.*`
- output bind: `bind.output.*`

### 2.2 Composed Flow

Composed flow keys:

- identity: `flow.id`, `flow.name`
- nodes: `node.N.id`, `node.N.contract`
- links: `link.N.from`, `link.N.to`
- optional expose metadata: `expose.*`

## 3. IR (Execution Graph)

Compiler/runtime normalized IR:

- `nodes[]`: resolved node descriptors
- `edges[]`: directional data links
- `inputs[]`: root inputs
- `outputs[]`: root outputs
- `stages[]`: topological layers

Node descriptor:

- `node_id`
- `contract_ref`
- `contract_kind` (`atomic|flow`)
- `required_inputs[]`
- `declared_outputs[]`

Edge descriptor:

- `from_ref`: `input.*` or `node.<id>.out.*`
- `to_ref`: `node.<id>.in.*` or `output.*`

## 4. Scheduling Semantics

Execution is dependency-driven:

- linear chain is sequential
- parallel happens only on fan-out branches without dependency between them

Algorithm:

1. Build DAG from links.
2. Validate references and endpoint formats.
3. Reject cycles.
4. Build topological stages (Kahn layering).
5. Execute stage-by-stage.
6. Execute nodes within stage concurrently.

No contract-level async/sync flags are required.

## 5. Node Invocation Contract

Parent-to-node:

- node invocation receives resolved values via CLI assignments
- baseline form:
  - `kc-flow --run <contract> --set input.x=... --set param.y=...`
  - or direct node runner command (renderer mode)

Node-to-parent:

- stdout lines: `output.<id>=<value>`
- parent parser extracts only `output.*` lines
- large payloads are returned as paths (still text handoff)

## 6. Execution Modes

### 6.1 Runtime Mode

`kc-flow --run <file>` executes directly.

- if atomic contract: execute runtime script
- if composed flow: execute graph recursively

### 6.2 CLI Render Mode

`kc-flow --cli <file>` emits runnable terminal script.

Target backend selection (initial):

- default: `bash`
- later: `powershell`

Renderer output must be runnable without `kc-flow` if direct node commands are
rendered.

## 7. Bash Renderer Design

Generated script properties:

- shebang: `#!/usr/bin/env bash`
- strict mode: `set -euo pipefail`
- per-node runner blocks
- stage execution loop
- parallel within stage using `&` + `wait`

Data handoff:

- script map emulation via variables + temp files
- parse function for `output.*`
- source output to destination input binding from edges

## 8. Validation Rules

Compilation-time:

- missing required keys
- unknown node ids in links
- invalid endpoint namespace
- cycle detected
- unresolved required input wiring

Runtime-time:

- node process failure (non-zero exit)
- missing expected output key
- malformed output line (strict mode)

## 9. Recursive Flow Execution

`node.N.contract` can reference:

- atomic contract
- nested composed flow

Execution contract:

- recursion depth must be bounded by practical stack/guard
- parent consumes child outputs through same `output.*` parser

## 10. Implementation Phases

### Phase 1: Graph Runtime

- add composed flow execution path in `src/main.c`
- implement DAG validation and cycle detection
- implement stage scheduler

### Phase 2: Nested Dispatch

- implement node dispatch by contract kind (`atomic|flow`)
- unify output parser and value map

### Phase 3: CLI Renderer (`--cli`)

- IR to `bash` script generation
- deterministic stable output order
- fixture tests for linear + fan-out + fan-in

### Phase 4: PowerShell Renderer

- backend parity with bash semantics

## 11. Testing Strategy

Unit-level:

- endpoint parser
- cycle detection
- stage builder
- output line parser

Functional-level:

- linear chain
- fan-out parallel branches
- fan-in join
- nested flow-in-flow
- invalid graphs fail fast

Golden tests:

- `--cli` output compared against expected scripts for fixed fixtures.
