# kc-flow Implementation

## Purpose

This document defines the implementation order for `kc-flow`.

`kc-flow` is built from the contract and flow model upward.

The engine is implemented first.

## Layers

The implementation is split into these layers:

- file loading
- contract parsing
- flow parsing
- execution planning
- headless execution
- CLI surface
- view metadata support

## Phase 1

Phase 1 implements one atomic contract executed headlessly.

The implementation includes:

- load one `.cfg` file
- parse flat `key=value` records
- detect whether the file is a contract or flow
- collect repeated sections dynamically by indexed prefix
- validate required contract fields
- resolve placeholders in execution strings
- execute one contract
- report outputs declared by the contract

## Phase 2

Phase 2 implements one composed flow.

The implementation includes:

- load one flow file
- parse nodes, links, expose records, and node param overrides
- resolve graph dependencies
- detect execution order
- resolve links from outputs to inputs and params
- execute node instances in dependency order
- expose flow outputs

## Phase 3

Phase 3 implements stable view metadata support.

The implementation includes:

- load one paired `.gui.cfg` file
- map visual metadata to contract ids and node ids
- preserve visual metadata on save
- keep view metadata separate from operational files

## File Loading

The loader reads line-oriented `key=value` records.

The loader handles:

- blank lines
- comments
- key parsing
- value parsing
- repeated indexed sections

The loader produces a flat record table for the parser.

## Contract Parser

The contract parser reads:

- identity
- params
- inputs
- outputs
- runtime
- output bindings

The contract parser validates:

- `contract.id`
- `contract.name`
- `runtime.script`

The parser also collects:

- `runtime.exec`
- `runtime.workdir`
- `runtime.stdin`
- `runtime.env.N.*`
- `bind.output.N.*`

## Flow Parser

The flow parser reads:

- identity
- public interface
- nodes
- node param overrides
- links
- expose

The flow parser validates:

- `flow.id`
- `flow.name`
- `node.N.id`
- `node.N.contract`
- `link.N.from`
- `link.N.to`

## Placeholder Resolution

Execution strings support contract placeholder replacement.

The initial placeholder syntax is angle-bracket substitution.

The initial placeholder surface is:

- `<input.<id>>`
- `<param.<id>>`
- `<output.<id>>`
- `<node.<node_id>.input.<id>>`
- `<node.<node_id>.output.<id>>`
- `<node.<node_id>.param.<id>>`

## Execution

Execution starts from the contract execution reference.

The initial execution path supports:

- direct execution of a script or command from `runtime.script`
- optional executable wrapper from `runtime.exec`
- optional working directory from `runtime.workdir`
- optional stdin source from `runtime.stdin`
- optional environment entries from `runtime.env.N.*`

The operating system resolves the concrete executable form of the referenced
script or command.

## Outputs

The engine collects outputs through declared bindings.

Initial output collection supports:

- `stdout`
- `stderr`
- `file`
- `exit_code`

## CLI

The CLI surface stays small.

Initial commands:

- `schema`
- `inspect <file>`
- `run <file>`

## Directory Direction

The implementation follows this layout:

- `src/`
- `etc/`
- `bin/<arch>/`

Operational definitions live in `etc/`.

## Immediate Next Steps

1. Finalize placeholder syntax for execution strings.
2. Implement the flat `key=value` loader.
3. Implement contract parsing and validation.
4. Make `inspect` print parsed contract structure.
5. Make `run` execute one atomic contract.
