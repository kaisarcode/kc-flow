# kc-flow etc

This directory contains the minimal graph composition examples for `kc-flow`.

## Files

- `linear.flow`: one linear DAG with two nodes in the same execution
- `linear-source.flow`: source contract for the linear graph
- `linear-render.flow`: render contract for the linear graph
- `linear.sh`: helper script that runs the linear graph
- `nest.flow`: one parent graph that starts one child sub-execution
- `nest-child.flow`: wrapper contract that runs one child `kc-flow`
- `nest-child-inner.flow`: child flow executed by the wrapper contract
- `nest-child-render.flow`: leaf contract inside the child flow
- `nest.sh`: helper script that runs the nesting graph

## Semantics

`linear.flow` stays inside one DAG:

```text
source -> render
```

`nest.flow` starts one child sub-execution:

```text
child
```

Inside `child`, one separate `kc-flow` run executes:

```text
render
```

## Usage

Run the linear graph:

```bash
./etc/linear.sh
kc-flow --run ./etc/linear.flow
kc-flow --run ./etc/linear.flow --set param.message=kc
```

Run the nesting graph:

```bash
./etc/nest.sh
kc-flow --run ./etc/nest.flow
kc-flow --run ./etc/nest.flow --set param.message=kc
```

---

**Author:** KaisarCode

**Email:** <kaisar@kaisarcode.com>

**Website:** [https://kaisarcode.com](https://kaisarcode.com)

**License:** [GNU GPL v3.0](https://www.gnu.org/licenses/gpl-3.0.html)

© 2026 KaisarCode
