# kc-flow - Branch Runtime Engine

> **Note:** This application is in the development and testing phase, is not
> ready for production use, and may change without prior notice.

`kc-flow` executes discrete execution branches described with flat
`key=value` flow files.

It is not a visual graph editor and it is not a contract/port runtime.
Its model is branch-oriented:

- one flow declares entry branches
- one node may execute one command
- one node may expand one child flow
- one node may open one or more next branches
- branches stay independent and do not merge

## Definitions

- **Flow**: one executable document that declares flow params, entry links,
    and local node definitions.
- **Node**: one branch step inside one flow.
- **Branch**: one execution path that moves through nodes and child flows.

## File Model

One flow file uses these keys:

- `flow.id=<id>`
- `flow.param.<key>=<value>`
- `flow.link=<node-ref>` repeated for each entry branch
- `node.<ref>.file=<path>`
- `node.<ref>.exec=<command>`
- `node.<ref>.param.<key>=<value>`
- `node.<ref>.link=<node-ref>` repeated for each outgoing branch

Node references are local to the current file.
They are flow-local names, not global reusable identities.

## Scope Model

Two placeholder scopes are supported:

- `<flow.param.X>`: effective parameter of the current flow
- `<node.param.X>`: effective parameter of the current node

Resolution rules:

- one node `exec` resolves in the scope of that node inside its parent flow
- one node `file` creates one child flow scope
- child flow defaults come from its own `flow.param.*`
- parent node params override those child flow defaults

## Execution Semantics

- runtime starts at every `flow.link`
- every `node.link` opens one independent branch
- sibling branches do not merge
- one node may define only `file`, only `exec`, or both
- when both are present, runtime expands `file` first and then applies
    `exec` to every active branch produced by that expansion
- one node without `exec` is valid; it may only expand a child flow or only
    forward branches
- one declared node that is not reachable from any `flow.link` remains part
    of the model but is not executed

Cycles are invalid and fail validation.

## Environment

Executed commands receive:

- `KC_FLOW_FILE`
- `KC_FLOW_DIR`
- `KC_FLOW_FD_IN`
- `KC_FLOW_FD_OUT`
- `KC_FLOW_FLOW_PARAM_*`
- `KC_FLOW_NODE_PARAM_*`
- `KC_FLOW_PARAM_*`

`KC_FLOW_PARAM_*` currently mirrors node params for shell convenience.

## Usage

### Help
```bash
kc-flow --help
```

### Run one flow
```bash
kc-flow --run /path/to/file.flow
```

### Override one flow param
```bash
kc-flow --run /path/to/file.flow --set flow.param.hello=Hello
```

`--set param.hello=Hello` is also accepted and maps to flow scope.

### Run with explicit descriptors
```bash
kc-flow --run /path/to/file.flow --fd-in 3 --fd-out 4
```

### Run with status output
```bash
kc-flow --run /path/to/file.flow --fd-status 5
```

## Current Example

Parent flow:
```flow
flow.id=parent
flow.param.hello=Hello
flow.link=node-1

node.node-1.file=child.flow
node.node-1.param.hello=<flow.param.hello>
node.node-1.link=node-2

node.node-2.param.hello=Hola
node.node-2.exec=printf "%s" "<node.param.hello> Mundo"

node.node-3.file=child.flow
node.node-3.param.hello=Bonjour
```

Child flow:
```flow
flow.id=child
flow.param.hello=Hello
flow.param.world=World
flow.link=node-1

node.node-1.exec=printf "%s" "<flow.param.hello> <flow.param.world>"
```

Runtime behavior:

- parent enters through `node-1`
- `node-1` expands `child.flow`
- child prints `Hello World`
- control returns to parent branch
- `node-2` prints `Hola Mundo`
- `node-3` stays declared but does not run because no branch reaches it

## Flags

| Flag | Description | Default |
| :--- | :--- | :--- |
| `--run` | Path to the flow file to execute | Required |
| `--set` | Flow param override in `key=value` form | None |
| `--workers` | Runtime worker count hint | CPU cores |
| `--fd-in` | Runtime input descriptor | Disabled |
| `--fd-out` | Runtime output descriptor | `1` |
| `--fd-status` | Runtime status descriptor | Disabled |
| `--help` | Show help | None |

## Testing

Run the automated test suite:
```bash
./test.sh
```

## Install

Install the current-architecture production binary on Linux:
```bash
wget -qO- https://raw.githubusercontent.com/kaisarcode/kc-flow/master/install.sh | bash
```

## Build

Build for specific architectures:
```bash
make x86_64
make win64
make aarch64
make arm64-v8a
make all
```

---

**Author:** KaisarCode

**Email:** <kaisar@kaisarcode.com>

**Website:** [https://kaisarcode.com](https://kaisarcode.com)

**License:** [GNU GPL v3.0](https://www.gnu.org/licenses/gpl-3.0.html)

© 2026 KaisarCode
