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
| `--help` | Shows help |

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
