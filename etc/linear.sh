#!/bin/bash
# linear.sh - Runs the linear graph example.
# Summary: Demonstrates one minimal two-node linear graph.
#
# Author:  KaisarCode
# Website: https://kaisarcode.com
# License: GNU GPL v3.0

set -e

# @brief Resolves the etc directory for the current graph examples.
# @return 0 on success.
resolve_example_dir() {
    EXAMPLE_DIR=$(CDPATH='' cd -- "$(dirname "$0")" && pwd)
}

# @brief Resolves the kc-flow binary for the current host architecture.
# @return 0 on success.
resolve_bin() {
    ARCH=$(uname -m)
    [ "$ARCH" = "x86_64" ] || [ "$ARCH" = "aarch64" ] || ARCH="arm64-v8a"
    export KC_FLOW_BIN="$EXAMPLE_DIR/../bin/$ARCH/kc-flow"
}

# @brief Runs the linear graph with the default message.
# @return 0 on success.
run_default() {
    "$KC_FLOW_BIN" --run "$EXAMPLE_DIR/linear.flow"
}

# @brief Runs the linear graph with one message override.
# @return 0 on success.
run_override() {
    "$KC_FLOW_BIN" --run "$EXAMPLE_DIR/linear.flow" --set param.message=kc
}

# @brief Entry point for the graph example script.
# @return 0 on success.
main() {
    resolve_example_dir
    resolve_bin
    printf "command:\n"
    printf '%s\n' "\"$KC_FLOW_BIN\" --run \"$EXAMPLE_DIR/linear.flow\""
    printf "default output:\n"
    run_default
    printf "\ncommand:\n"
    printf '%s\n' "\"$KC_FLOW_BIN\" --run \"$EXAMPLE_DIR/linear.flow\" --set param.message=kc"
    printf "override output:\n"
    run_override
    printf "\n"
}

main "$@"
