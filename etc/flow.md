# kc-flow flow

This document defines the first concrete flow file direction for `kc-flow`.

## Purpose

A flow file defines a composed executable unit.

A flow is also a contract-level unit.

It can be:

- opened directly
- executed directly
- imported into another flow as a tool

## File Direction

The initial flow format uses flat `key=value` records.

Suggested extension:

- `.cfg`

Associated visual metadata may be stored in a paired file:

- `.gui.cfg`

## Sections

A flow file contains these sections:

- identity
- public interface
- nodes
- links
- expose

The flow file stores the operational graph of the composed unit.

## Identity

Required keys:

- `flow.id`
- `flow.name`

Optional keys:

- `flow.version`
- `flow.description`

Example:

```cfg
flow.id=kc.flow.prompt_to_image
flow.name=Prompt To Image
flow.version=1
flow.description=Compose prompt rewriting and image generation
```

## Public Interface

A flow may expose:

- `params`
- `inputs`
- `outputs`

These use the same field structure as the contract format.

That includes value inputs, filesystem references, direct binary payloads,
and streams.

The connected contracts define the meaning and expected format of the data
they exchange.

Example:

```cfg
input.1.id=prompt
input.1.type=text
input.1.required=1
input.1.label=Prompt
input.1.description=Prompt text

output.1.id=image
output.1.type=file
output.1.label=Image
output.1.description=Generated image
```

## Nodes

Nodes are instances of other contracts.

Required node keys:

- `node.N.id`
- `node.N.contract`

Optional node keys:

- `node.N.title`

Example:

```cfg
node.1.id=llm
node.1.contract=kc.llm.rewrite
node.1.title=Prompt Rewrite

node.2.id=sdf
node.2.contract=kc.sdf.generate
node.2.title=Image Generate
```

## Node Param Overrides

A node may override its own params inside the flow.

Example:

```cfg
node.param.1.node=llm
node.param.1.id=predict
node.param.1.value=128

node.param.2.node=sdf
node.param.2.id=width
node.param.2.value=1024
```

Required override keys:

- `node.param.N.node`
- `node.param.N.id`
- `node.param.N.value`

## Links

Links connect one source field to one destination field.

The source and destination use dot paths.

Path forms:

- `input.<id>`
- `node.<node_id>.output.<id>`
- `node.<node_id>.input.<id>`
- `node.<node_id>.param.<id>`

Example:

```cfg
link.1.from=input.prompt
link.1.to=node.llm.input.prompt

link.2.from=node.llm.output.prompt
link.2.to=node.sdf.input.prompt
```

Required link keys:

- `link.N.from`
- `link.N.to`

## Expose

Expose maps internal node fields to the public interface of the flow.

Example:

```cfg
expose.1.from=node.sdf.output.image
expose.1.as=output.image
```

Required expose keys:

- `expose.N.from`
- `expose.N.as`

## Visual Metadata

Visual state may be stored in a paired `*.gui.cfg` file associated with a
flow and with its node instances.

That standard metadata may include:

- window position
- window size
- collapsed state
- selection state
- grouping
- zoom
- viewport

Any compatible `kc-flow` GUI relates that metadata to contracts and flow
nodes through their ids.

## Dynamic Discovery

The engine discovers repeated records dynamically by prefix and index.

Examples:

- `node.1.*`
- `node.2.*`
- `node.param.1.*`
- `link.1.*`
- `expose.1.*`

Repeated sections are discovered directly from the indexed keys present in
the file.

## Minimal Flow Example

```cfg
flow.id=kc.flow.prompt_to_image
flow.name=Prompt To Image
flow.version=1
flow.description=Compose prompt rewriting and image generation

input.1.id=prompt
input.1.type=text
input.1.required=1
input.1.label=Prompt
input.1.description=Prompt text

output.1.id=image
output.1.type=file
output.1.label=Image
output.1.description=Generated image

node.1.id=llm
node.1.contract=kc.llm.rewrite
node.1.title=Prompt Rewrite
node.2.id=sdf
node.2.contract=kc.sdf.generate
node.2.title=Image Generate

node.param.1.node=llm
node.param.1.id=predict
node.param.1.value=128
node.param.2.node=sdf
node.param.2.id=width
node.param.2.value=1024

link.1.from=input.prompt
link.1.to=node.llm.input.prompt
link.2.from=node.llm.output.prompt
link.2.to=node.sdf.input.prompt

expose.1.from=node.sdf.output.image
expose.1.as=output.image
```
