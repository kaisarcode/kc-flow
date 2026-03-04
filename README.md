# kc-flow

`kc-flow` is the execution foundation for contract-defined KaisarCode
tools and composed flows.

It is intentionally centered on headless execution first.

The core model is the contract plus the composed flow interpreted by a
headless engine.

The GUI layer is a visual editor and representation over that same model.

## Core Direction

- contract-defined executable tools
- flow composition through explicit input/output links
- headless inspection and execution
- reusable workflows that can be imported as tools
- scripts and commands described by the contract execution reference

## Runtime Surface

`kc-flow` currently provides a headless CLI surface for inspecting and
running contract and flow files.

## Usage

### Help
```bash
kc-flow --help
```

### Print schema direction
```bash
kc-flow schema
```

### Inspect a contract or flow file
```bash
kc-flow inspect /path/to/file.flow
```

### Run a contract or flow file
```bash
kc-flow --run /path/to/file.flow
```

### Run with input/param overrides
```bash
kc-flow --run /path/to/file.flow --set input.user_text=hola --set param.width=1024
```

## Command Surface

| Command | Description |
| :--- | :--- |
| `schema` | Prints the current conceptual direction for the contract model |
| `inspect <file>` | Verifies that a contract/flow file exists and reports its path |
| `--run <file> [--set key=value ...]` | Resolves one contract/flow file path and executes contract files headlessly |
| `--help` | Shows help |

## Architecture Direction

The intended system is split into clear layers:

- `contract`: executable unit definition
- `flow`: composition of contracts and links
- `engine`: headless resolution and execution
- `cli`: non-GUI interface to engine behaviour
- `gui`: optional representation and editor over the same model

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
