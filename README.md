# kc-stdio - Headless Contract Engine Scaffold

`kc-stdio` is the execution foundation for contract-defined KaisarCode
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
- platform-specific script runners described by the contract runtime

## Current Scope

This initial app is a scaffold that establishes the autonomous app layout and
the first headless CLI surface for `kc-stdio`.

The current binary provides the initial command surface for inspecting and
running contract and flow files while the deeper runtime model is being
defined.

## Usage

### Help
```bash
kc-stdio --help
```

### Version
```bash
kc-stdio --version
```

### Print schema direction
```bash
kc-stdio schema
```

### Inspect a contract or flow file
```bash
kc-stdio inspect path/to/file.kcs
```

### Run a contract or flow file
```bash
kc-stdio run path/to/file.kcs
```

## Command Surface

| Command | Description |
| :--- | :--- |
| `schema` | Prints the current conceptual direction for the contract model |
| `inspect <file>` | Verifies that a contract/flow file exists and reports its path |
| `run <file>` | Verifies that a contract/flow file exists and reports intended execution |
| `-h`, `--help` | Shows help |
| `-v`, `--version` | Shows version |

## Architecture Direction

The intended system is split into clear layers:

- `contract`: executable unit definition
- `flow`: composition of contracts and links
- `engine`: headless resolution and execution
- `cli`: non-GUI interface to engine behaviour
- `gui`: optional representation and editor over the same model

## Spec

The first written model draft lives in:

- `spec/v0.md`

This spec defines the initial direction for:

- contract identity and public interface
- runtime and binding rules
- composed flow structure
- headless engine responsibilities

## Testing

Run the automated test suite:
```bash
./test.sh
```

## Install

Install the current-architecture production binary on Linux:
```bash
wget -qO- https://raw.githubusercontent.com/kaisarcode/kc-stdio/master/install.sh | bash
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

`kc-stdio` follows the autonomous app layout:

- binaries are built into `bin/<arch>/`
- toolchains come from `/usr/local/share/kaisarcode/toolchains`

---

**Author:** KaisarCode

**Website:** [https://kaisarcode.com](https://kaisarcode.com)

**License:** [GNU GPL v3.0](https://www.gnu.org/licenses/gpl-3.0.html)

© 2026 KaisarCode
