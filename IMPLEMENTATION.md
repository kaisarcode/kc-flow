# kc-flow Implementation

## Core Principle

There is one unit type: `flow`.

A flow can be:

- atomic (single runtime-backed step)
- composed (graph of nested flows)

The only special case is invocation context:

- root flow: launched with `kc-flow --run <file>`
- nested flow: referenced by a parent flow node

## Implementation Layers

- file loading
- flow parsing
- graph planning
- headless execution
- CLI surface
- GUI metadata support

## Phase 1

Atomic flow execution.

Includes:

- load one flow file (`key=value`)
- collect indexed sections dynamically
- validate required keys (`flow.id`, `flow.name`, `runtime.script` for atomic)
- resolve placeholders and overrides
- run one atomic flow headlessly
- collect declared outputs

## Phase 2

Composed flow execution.

Includes:

- parse nodes, links, node param overrides
- build dependency graph from links
- run independent branches in parallel
- run node when required `in.*` endpoints are resolved
- resolve parent `output.*` endpoints
- fail fast on unknown endpoints, missing required values, and cycles

## Phase 3

Reusable nested flows.

Includes:

- allow `node.N.contract` to reference another flow unit
- execute nested flow with same runtime semantics
- expose nested outputs via parent links
- keep root and nested behavior identical except entrypoint

## Phase 4

Stable metadata and CLI completion.

Includes:

- load/save paired `contract.gui.cfg` metadata
- keep metadata separate from operational flow files
- extend CLI introspection while preserving minimal runtime surface

## GUI Scope Boundary

GUI implementation is outside `kc-flow` and belongs to `kc-studio`.

`kc-flow` only defines and executes flow contracts headlessly.

## Runtime Resolution

Runtime template supports:

- `<param.<id>>`
- `<input.<id>>`
- `<output.<id>>`

CLI overrides:

- `--set param.<id>=<value>`
- `--set input.<id>=<value>`

## Endpoint Grammar

Allowed source endpoints:

- `input.<id>`
- `node.<node_id>.out.<id>`

Allowed destination endpoints:

- `node.<node_id>.in.<id>`
- `output.<id>`

## Execution Guarantees

- deterministic fail-fast on invalid graph state
- explicit data-flow links only
- no payload schema enforcement in engine
- scripts/tools define data semantics
