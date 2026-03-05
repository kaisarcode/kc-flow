# kc-flow

`kc-flow` executes KaisarCode flow contracts with a deterministic headless runtime.

## Definitions

- **Flow**: runtime execution unit (atomic or composed).
- **Contract**: text file that defines a flow (`key=value`).

Contract structure:

- identity: `flow.id`, `flow.name`
- interface: `input.*`, `output.*`, `param.*`
- atomic runtime: `runtime.*`, `bind.output.*`
- composed graph: `node.*`, `link.*`, `expose.*`

## Runtime Surface

`kc-flow` provides a CLI runtime for executing flow contracts.

Invocation contexts:

- root flow: launched with `kc-flow --run <file>`
- nested flow: referenced by another flow as a node

## Architecture

`kc-flow` is a script orchestrator, not a programming language runtime.

- Contracts define structure (`key=value`).
- Scripts/binaries define execution logic.
- Parent scripts are responsible for chaining/handoff.
- Nested execution is the normal model (`flow -> node -> contract/flow`).

### Contract/Flow Model

- Atomic contract:
  - `contract.id`, `contract.name`
  - `input.*`, `param.*`, `output.*`
  - `runtime.script` (and optional runtime metadata)
- Composed flow:
  - `flow.id`, `flow.name`
  - `node.N.id`, `node.N.contract`
  - `link.N.from`, `link.N.to`

### Endpoint Semantics

- Source endpoints:
  - `input.<id>`
  - `node.<node_id>.out.<id>`
- Destination endpoints:
  - `node.<node_id>.in.<id>`
  - `output.<id>`

### Scheduling Semantics

- Linear chains are sequential by dependency.
- Fan-out branches are parallelizable.
- Fan-in waits for required upstream values.
- Cycles are invalid topologies and fail fast.

Current graph validation includes:

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

### Run with input/param overrides
```bash
kc-flow --run /path/to/file.flow --set input.user_text=hello --set param.width=1024
```

## Command Surface

| Command | Description |
| :--- | :--- |
| `--run <file> [--set key=value ...]` | Executes one flow file |
| `--cli <file>` | Renders execution chain as terminal CLI script |
| `--help` | Shows help |

## CLI Rendering

`--cli` is the export surface for reproducible chain execution.

- Intent: render runnable terminal chaining from contract topology.
- Baseline backend: `bash` (reference).
- Future parity target: `powershell`.
- Renderer output should preserve deterministic wiring semantics.

Status:

- `--cli` command path exists.
- Contract and flow CLI renderer backend is implemented (`bash`).
- Flow rendering is dependency-driven and deterministic.

## Implementation Notes

Execution model:

1. Parse `key=value` contract/flow.
2. Build normalized graph from `node.*` and `link.*`.
3. Validate endpoints/references and reject cycles.
4. Execute nested graph runtime with deterministic node chaining.

Node invocation contract:

- Parent passes resolved values via CLI args (`--set key=value`).
- Child returns data through stdout (`output.<id>=<value>`).
- Large payloads are handed off as paths.

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
# Linux x86_64
make ARCH=x86_64

# Windows x86_64
make ARCH=win64

# Linux ARM64
make ARCH=aarch64

# Android ARM64
make ARCH=arm64-v8a

# Build All
make all
```

## Structure

`kc-flow` follows the autonomous app layout:

- binaries are built into `bin/<arch>/`
- toolchains come from `/usr/local/share/kaisarcode/toolchains`

---

**Author:** KaisarCode

**Website:** [https://kaisarcode.com](https://kaisarcode.com)

**License:** [GNU GPL v3.0](https://www.gnu.org/licenses/gpl-3.0.html)

© 2026 KaisarCode
