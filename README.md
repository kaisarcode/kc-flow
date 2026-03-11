# kc-flow

> **Note:** This application is in the development and testing phase, is not ready for production use, and may change without prior notice.

`kc-flow` executes machine-oriented flow graphs that compose commands and
nested flows through a headless runtime.

## Definitions

- **Flow**: executable unit that may run directly or be referenced as one node
    inside another flow.
- **Contract**: atomic node definition described with `key=value` records.

Flow structure:

- identity: `flow.id`, `flow.name`
- interface: `input.*`, `output.*`, `param.*`
- atomic runtime: `runtime.*`
- composed graph: `node.*`, `link.*`

## Runtime Surface

`kc-flow` provides the runtime surface for executing one root flow or one
atomic contract definition.

Invocation contexts:

- root flow: launched with `kc-flow --run <file>`
- nested flow: referenced by another flow as a node

## Architecture

`kc-flow` is a process-graph runtime, not a programming language runtime.

- Contracts and flows define structure with `key=value`.
- One node executes one command or script.
- One node receives raw input through one runtime descriptor.
- One node receives its declared parameters as environment for that execution.
- One node decides internally how to map that input and those parameters to
    the command it runs.
- Composed flows schedule ready nodes by resolved dependencies.
- Nested execution is the normal model (`flow -> node -> contract/flow`).
- Root execution uses `--fd-in` and `--fd-out` for the public flow interface.

### Contract/Flow Model

- Atomic contract uses `contract.id`, `contract.name`, `input.*`, `param.*`,
    `output.*`, and `runtime.script`.
- Composed flow uses `flow.id`, `flow.name`, `node.N.id`, `node.N.contract`,
    `link.N.from`, and `link.N.to`.

### Endpoint Semantics

- Source endpoints are `input.<id>` and `node.<node_id>.out.<id>`.
- Destination endpoints are `node.<node_id>.in.<id>` and `output.<id>`.

### Execution Semantics

- Ready nodes are dispatched by dependency order.
- Runtime concurrency is limited by `--workers`.
- Nested flows behave like any other node.
- Cycles are invalid topologies and fail fast.
- Loop semantics are not supported in this stage.
- Runtime transport is descriptor-based.
- Node parameters remain internal to the node and are exported to its process
    environment.
- The runtime does not parse one node output to rebuild the next node CLI.

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
kc-flow --run /path/to/file.flow --set param.width=1024
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

### Full Parameter Reference

| Flag | Description | Default |
| :--- | :--- | :--- |
| `--run` | Path to the flow file to execute | Required |
| `--set` | Runtime override (format: `key=value`) | `NULL` |
| `--workers` | Runtime worker process limit | CPU cores |
| `--fd-in` | Runtime input descriptor | `0` |
| `--fd-out` | Runtime output descriptor | `1` |
| `--help` | Shows help | `NULL` |

## Implementation Notes

Execution model:

1. Parse one `key=value` contract or flow.
2. Resolve indexed sections for params, inputs, outputs, nodes, and links.
3. Validate graph references before execution.
4. Execute ready nodes with raw input plus declared node parameters.
5. Connect node outputs to downstream inputs through descriptor transport.
6. Recurse into nested flows as closed execution units.

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

**Website:** [https://kaisarcode.com](https://kaisarcode.com)

**Email:** <kaisar@kaisarcode.com>

**License:** [GNU GPL v3.0](https://www.gnu.org/licenses/gpl-3.0.html)

© 2026 KaisarCode
