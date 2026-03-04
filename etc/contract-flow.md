# kc-flow Flow/Contract Model

Everything is a flow unit.

A unit can be:

- atomic (runtime-backed)
- composed (node/link-backed)

The file format is flat `key=value` records.

Invocation context:

- root flow: launched with `--run`
- nested flow: referenced by a parent flow node

## Definitions

### Flow

A Flow is the execution unit in `kc-flow`.

It defines:

- inputs
- processing
- outputs

A Flow can be:

- atomic (one runtime step/script)
- composed (graph of nested flows linked by endpoints)

### Contract

A Contract is the text file that defines a Flow.

It uses flat `key=value` records and stores:

- identity (`flow.id`, `flow.name`)
- interface (`input.*`, `output.*`, `param.*`)
- runtime fields (for atomic flows)
- graph fields (`node.*`, `link.*`, `expose.*`) for composed flows

## Identity

Required:

- `flow.id`
- `flow.name`

## Atomic Unit (runtime-backed)

Required for atomic execution:

- `runtime.script`

Optional:

- `runtime.exec`
- `runtime.workdir`
- `runtime.stdin`

Optional repeated sections:

- `param.N.*`
- `input.N.*`
- `output.N.*`
- `runtime.env.N.*`
- `bind.output.N.*`

## Composed Unit (graph-backed)

Optional repeated sections:

- `param.N.*`
- `input.N.*`
- `output.N.*`
- `node.N.*`
- `node.param.N.*`
- `link.N.*`
- `expose.N.*`

Node requirements per index:

- `node.N.id`
- `node.N.contract`

`node.N.contract` points to another flow unit reference (id/path).

Note: the key name is kept for compatibility with the current runner.

Link requirements per index:

- `link.N.from`
- `link.N.to`

## Dynamic Discovery

Indexed sections are discovered dynamically from keys present in the file.

## Runtime Environment

Supported runtime env keys:

- `runtime.env.N.key`
- `runtime.env.N.value`

## Output Binding

Supported binding keys:

- `bind.output.N.id`
- `bind.output.N.mode`
- `bind.output.N.path`

Supported binding modes:

- `stdout`
- `stderr`
- `exit_code`
- `file`

## Placeholder Resolution

Runtime templates can resolve:

- `<param.<id>>`
- `<input.<id>>`
- `<output.<id>>`

CLI overrides:

- `--set param.<id>=<value>`
- `--set input.<id>=<value>`

## Endpoint Paths

Allowed source endpoints:

- `input.<id>`
- `node.<node_id>.out.<id>`

Allowed destination endpoints:

- `node.<node_id>.in.<id>`
- `output.<id>`

## Data Channels

Recommended channel labels:

- `text`
- `stream`
- `binary`
- `void`

These labels are orchestration metadata only. Script/tool logic defines payload semantics.

## GUI Metadata

GUI metadata is separate from the contract file (`contract.gui.cfg`).

## Minimal Atomic Example

```cfg
flow.id=kc.example.echo
flow.name=Echo
runtime.script=./etc/echo.sh "<input.user_text>"
runtime.workdir=.

input.1.id=user_text
input.1.type=text
input.1.required=1
input.1.default=hello

output.1.id=result
output.1.type=text

bind.output.1.id=result
bind.output.1.mode=stdout
```

## Minimal Composed Example

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
