# kc-flow

> **Note:** This application is in the development and testing phase, is not ready for production use, and may change without prior notice.

`kc-flow` composes arbitrary command executions in graph-based flows driven
by contract-defined nodes.

This repository provides the runtime layer for contract composition,
descriptor transport, and runtime status emission over those graphs.

> **Note:** `kc-flow` is the machine-readable runtime layer. Human-oriented
> authoring, management, and inspection of these DAGs live in separate GUI
> applications, such as `kc-studio`.

## Definitions

- **Flow**: executable DAG unit that may run directly.
- **Contract**: atomic node definition described with `key=value` records.

Flow structure:

- identity: `flow.id`, `flow.name`
- interface: `input.*`, `output.*`, `param.*`
- atomic runtime: `runtime.command`
- composed graph: `node.*`, `link.*`

## Runtime Surface

`kc-flow` provides the runtime surface for executing one root flow or one
atomic contract definition.

Invocation contexts:

- root flow: launched with `kc-flow --run <file>`
- sub-execution: started by one contract command that runs `kc-flow`

## Architecture

`kc-flow` is a process-graph runtime, not a programming language runtime.

- Contracts and flows define structure with `key=value`.
- One node executes one command string.
- One node receives one functional input FD.
- One node receives its declared parameters as environment for that execution.
- One node decides internally how to map those FDs and parameters to the
    command it runs.
- One flow keeps its linked nodes inside the same DAG execution.
- One contract may launch one sub-execution by invoking `kc-flow` in
    `runtime.command`.
- Composed flows schedule ready nodes by resolved dependencies.
- Root execution uses `--fd-in` and `--fd-out` for the public flow interface.
- Runtime status can be observed separately through `--fd-status`.

### Contract/Flow Model

- Atomic contract uses `contract.id`, `contract.name`, `input.*`, `param.*`,
    `output.*`, and `runtime.command`.
- Composed flow uses `flow.id`, `flow.name`, `node.N.id`, `node.N.contract`,
    `link.N.from`, and `link.N.to`.

### Endpoint Semantics

- Source endpoints are `input.<id>` and `node.<node_id>.out.<id>`.
- Destination endpoints are `node.<node_id>.in.<id>` and `output.<id>`.

### Execution Semantics

- Ready nodes are dispatched by dependency order.
- Runtime concurrency is limited by `--workers`.
- Links define one horizontal DAG inside the same runtime execution.
- One child flow becomes one sub-execution only when one contract command
    starts `kc-flow` explicitly in `runtime.command`.
- Cycles are invalid topologies and fail fast.
- Loop semantics are not supported in this stage.
- Runtime transport is descriptor-based.
- Node parameters remain internal to the node and are exported to its process
    environment.
- Atomic commands receive `KC_FLOW_FILE`, `KC_FLOW_DIR`,
    `KC_FLOW_FD_IN`, `KC_FLOW_FD_OUT`, and `KC_FLOW_PARAM_*` in their
    environment.
- The runtime does not force one child command to use `stdin/stdout`.

Graph validation includes:

- endpoint format validation
- node reference validation
- cycle detection

## Usage

### Help
```bash
kc-flow --help
```

### Run a flow file
```bash
kc-flow --run /path/to/file.flow
```

### Run with overrides
```bash
kc-flow --run /path/to/file.flow --set param.message=hello
```

### Run with a custom worker limit
```bash
kc-flow --run /path/to/file.flow --workers 2
```

### Run with explicit descriptors
```bash
kc-flow --run /path/to/file.flow --fd-in 3 --fd-out 4
```

Use explicit descriptors when the flow is embedded inside another runtime or
parent process.

### Run with a status descriptor
```bash
kc-flow --run /path/to/file.flow --fd-status 5
```

`--fd-status` emits one line per runtime event without changing the
functional data path of the flow.

### Minimal graph examples

Linear graph:
```bash
./etc/linear.sh
kc-flow --run ./etc/linear.flow
kc-flow --run ./etc/linear.flow --set param.message=kc
```

Nesting graph:
```bash
./etc/nest.sh
kc-flow --run ./etc/nest.flow
kc-flow --run ./etc/nest.flow --set param.message=kc
```

The `linear` example stays inside one DAG:
```text
source -> render
```

The `nest` example starts one child sub-execution from one contract:
```text
prepare -> child
```

Inside `child`, one separate `kc-flow` run executes:
```text
render
```

### Full Parameter Reference

| Flag | Description | Default |
| :--- | :--- | :--- |
| `--run` | Path to the flow file to execute | Required |
| `--set` | Runtime override (format: `key=value`) | `NULL` |
| `--workers` | Runtime worker process limit | CPU cores |
| `--fd-in` | Runtime input descriptor | `0` |
| `--fd-out` | Runtime output descriptor | `1` |
| `--fd-status` | Runtime status descriptor | Disabled |
| `--help` | Shows help | `NULL` |

Status event examples:

```text
event=run.started pid=1001 kind=flow id=kc.nest path=/repo/etc/nest.flow
event=node.started pid=1001 kind=node node=prepare target_kind=contract target_path=/repo/etc/nest-prepare.flow
event=node.finished pid=1001 kind=node node=prepare target_kind=contract target_path=/repo/etc/nest-prepare.flow status=ok
event=node.started pid=1001 kind=node node=child target_kind=contract target_path=/repo/etc/nest-child.flow
event=node.finished pid=1001 kind=node node=child target_kind=contract target_path=/repo/etc/nest-child.flow status=ok
event=run.finished pid=1001 kind=flow id=kc.nest path=/repo/etc/nest.flow status=ok
```

## Implementation Notes

Execution model:

1. Parse one `key=value` contract or flow.
2. Resolve indexed sections for params, inputs, outputs, nodes, and links.
3. Validate graph references before execution.
4. Execute ready nodes with functional FDs plus declared node parameters.
5. Connect node outputs to downstream inputs through descriptor transport.
6. Start one sub-execution only when one contract command runs `kc-flow`
    explicitly.

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

## Compilation
Build for specific architectures:
```bash
Linux x86_64:
make ARCH=x86_64

Windows x86_64:
make ARCH=win64

Linux ARM64:
make ARCH=aarch64

Android ARM64:
make ARCH=arm64-v8a

Build all:
make all
```

---

**Author:** KaisarCode

**Email:** <kaisar@kaisarcode.com>

**Website:** [https://kaisarcode.com](https://kaisarcode.com)

**License:** [GNU GPL v3.0](https://www.gnu.org/licenses/gpl-3.0.html)

© 2026 KaisarCode
