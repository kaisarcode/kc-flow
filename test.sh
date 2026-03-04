#!/bin/bash
# test.sh - Automated test suite for kc-flow
# Summary: Tiered testing for KCS, Ecosystem compliance, and Functional logic.
#
# Author:  KaisarCode
# Website: https://kaisarcode.com
# License: GNU GPL v3.0
set -e

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
APP_ROOT="$SCRIPT_DIR"

fail() {
    printf "\033[31m[FAIL]\033[0m %s\n" "$1"
    exit 1
}

pass() {
    printf "\033[32m[PASS]\033[0m %s\n" "$1"
}

test_setup() {
    ARCH=$(uname -m)
    [ "$ARCH" = "x86_64" ] || [ "$ARCH" = "aarch64" ] || ARCH="arm64-v8a"
    case "$ARCH" in
        x86_64) EXT="" ;;
        aarch64) EXT="" ;;
        arm64-v8a) EXT="" ;;
        *) EXT="" ;;
    esac
    export KC_BIN_EXEC="$APP_ROOT/bin/$ARCH/kc-flow$EXT"

    if [ ! -f "$KC_BIN_EXEC" ]; then
        fail "Binary not found at $KC_BIN_EXEC."
    fi
    pass "Environment verified: using $KC_BIN_EXEC"
}

test_kcs() {
    if command -v kcs >/dev/null 2>&1; then
        find "$APP_ROOT" -type f -not -path '*/.*' -not -path '*/bin/*' \
            -exec kcs {} + || fail "KCS validation failed."
        pass "General: KCS compliance verified."
    fi
}

test_general() {
    if ! "$KC_BIN_EXEC" --help | grep -q "Commands:"; then
        fail "General: Help flag failed."
    fi
    pass "General: Help flag verified."

    if "$KC_BIN_EXEC" --unknown >/dev/null 2>&1; then
        fail "General: Unknown flag should fail."
    fi
    pass "General: Unknown flag fail-fast verified."
}

test_functional() {
    OUTPUT=$("$KC_BIN_EXEC" schema)
    printf '%s' "$OUTPUT" | grep -q "contract" || fail "Functional: schema output missing contract."
    printf '%s' "$OUTPUT" | grep -q "flow" || fail "Functional: schema output missing flow."
    pass "Functional: Schema direction verified."

    EXAMPLE_FILE="$APP_ROOT/etc/example.flow"

    OUTPUT=$("$KC_BIN_EXEC" inspect "$EXAMPLE_FILE")
    printf '%s' "$OUTPUT" | grep -q "inspect ok" || fail "Functional: inspect command failed."
    printf '%s' "$OUTPUT" | grep -q "kind=contract" || fail "Functional: inspect kind detection failed."
    printf '%s' "$OUTPUT" | grep -q "id=kc.example.echo" || fail "Functional: inspect id output failed."
    printf '%s' "$OUTPUT" | grep -q "runtime.script=" || fail "Functional: inspect runtime output failed."
    pass "Functional: Inspect command verified."

    OUTPUT=$("$KC_BIN_EXEC" --run "$EXAMPLE_FILE")
    printf '%s' "$OUTPUT" | grep -q "run ok" || fail "Functional: run command failed."
    printf '%s' "$OUTPUT" | grep -q "kind=contract" || fail "Functional: run kind detection failed."
    printf '%s' "$OUTPUT" | grep -q "output.result=hello" || fail "Functional: run output binding failed."
    pass "Functional: Run command verified."

    INPUT_FILE="$APP_ROOT/etc/example-input.flow"

    if "$KC_BIN_EXEC" --run "$INPUT_FILE" >/dev/null 2>&1; then
        fail "Functional: run without required input override should fail."
    fi
    pass "Functional: Missing input override fail-fast verified."

    OUTPUT=$("$KC_BIN_EXEC" --run "$INPUT_FILE" --set input.user_text=hola)
    printf '%s' "$OUTPUT" | grep -q "run ok" || fail "Functional: run with --set failed."
    printf '%s' "$OUTPUT" | grep -q "output.result=hola" || fail "Functional: --set input override output failed."
    pass "Functional: Run with --set input override verified."

    if "$KC_BIN_EXEC" inspect "$EXAMPLE_FILE.missing" >/dev/null 2>&1; then
        fail "Functional: missing file should fail."
    fi
    pass "Functional: Missing file fail-fast verified."

    TMP_FILE=$(mktemp)
    trap 'rm -f "$TMP_FILE"' RETURN
    printf 'contract.id=broken\n' > "$TMP_FILE"

    if "$KC_BIN_EXEC" inspect "$TMP_FILE" >/dev/null 2>&1; then
        fail "Functional: invalid contract should fail."
    fi
    pass "Functional: Invalid contract validation verified."

    rm -f "$TMP_FILE"
    trap - RETURN
}

run_tests() {
    test_setup
    test_kcs
    test_general
    test_functional
    pass "All tests passed successfully."
}

run_tests
