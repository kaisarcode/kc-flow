# kc-flow Flow

This document describes flow semantics for `kc-flow`.

## Purpose

A flow is the only executable unit type.

A flow can be atomic or composed.

A flow can be:

- opened directly
- executed directly
- imported into another flow as a tool

## Format

Flow files use flat `key=value` records.

GUI state is external metadata (`contract.gui.cfg`) and is not part of the engine contract.

## Definitions

- **Flow**: runtime execution unit (atomic or composed).
- **Contract**: text file that defines a flow (`key=value`).

## Required Identity

- `flow.id`
- `flow.name`

## Repeated Sections

The engine discovers indexed sections dynamically from keys present in the file:

- `param.N.*`
- `input.N.*`
- `output.N.*`
- `node.N.*`
- `node.param.N.*`
- `link.N.*`
- `expose.N.*`

## Node Rules

Each `node.N` requires:

- `node.N.id`
- `node.N.contract`

Optional:

- `node.N.title`

## Link Rules

Each `link.N` requires:

- `link.N.from`
- `link.N.to`

Links express explicit endpoint wiring between parent flow and node instances.

### Endpoint Paths

Allowed source endpoints (`link.N.from`):

- `input.<id>`
- `node.<node_id>.out.<id>`

Allowed destination endpoints (`link.N.to`):

- `node.<node_id>.in.<id>`
- `output.<id>`

No network meaning is implied by endpoint naming. These are logical data-flow endpoints only.

### Connection Semantics

- A link connects exactly one source endpoint to one destination endpoint.
- One source endpoint may connect to multiple destination endpoints (fan-out).
- A destination endpoint may aggregate multiple sources only if the target node/tool explicitly supports that input pattern; otherwise it is treated as invalid.
- Destination endpoints receive exactly one resolved value per execution step.
- Missing required destination values fail the flow.
- Unknown node ids or endpoint ids fail the flow.
- Cycles fail the flow.

### Parallelism

- Independent nodes (no dependency edge between them) are runnable in parallel.
- Fan-out naturally enables parallel branches from one source value.
- Fan-in is allowed when a node has multiple required `in.*` endpoints and all of them are resolved.
- A node starts only when its required inputs are fully resolved.

## Execution Rules

1. Build a dependency graph from `link.N.*`.
2. Resolve parent `input.*` values first.
3. Run a node when all required `node.<id>.in.*` values are resolved.
4. Resolve parent `output.*` values from linked sources.
5. Fail fast on missing required inputs, unknown endpoints, and cycles.

## Nested Flows

- A node contract may reference another flow unit.
- This allows flow-in-flow composition recursively.
- A parent flow can finish and be used as a reusable node inside a larger graph.

## Expose Rules

`expose.N` is optional compatibility metadata that maps internal node fields to public flow fields.

Preferred output wiring is explicit `link.N.to=output.<id>`.

Suggested keys:

- `expose.N.from`
- `expose.N.as`

This is what makes a flow reusable as a tool.

## Minimal Flow Example

```cfg
flow.id=kc.flow.sample
flow.name=Sample Flow

input.1.id=prompt
input.1.type=text

node.1.id=first
node.1.contract=kc.example.rewrite

node.2.id=second
node.2.contract=kc.example.generate

link.1.from=input.prompt
link.1.to=node.first.in.prompt

link.2.from=node.first.out.prompt
link.2.to=node.second.in.prompt

link.3.from=node.second.out.result
link.3.to=output.result
```

## Example Files

- `etc/example.flow`: minimal atomic flow
- `etc/example-input.flow`: atomic flow with required input override
- `etc/example-parallel.flow`: fan-out to parallel downstream nodes
- `etc/example-nested.flow`: flow composed from another flow unit
