#!/bin/bash
# test.sh - Automated test suite for kc-stdio
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
    export KC_BIN_EXEC="$APP_ROOT/bin/$ARCH/kc-stdio$EXT"

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

    if ! "$KC_BIN_EXEC" --version | grep -q "^kc-stdio "; then
        fail "General: Version flag failed."
    fi
    pass "General: Version flag verified."

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

    TMP_FILE=$(mktemp)
    trap 'rm -f "$TMP_FILE"' RETURN
    printf 'id=test\n' > "$TMP_FILE"

    OUTPUT=$("$KC_BIN_EXEC" inspect "$TMP_FILE")
    printf '%s' "$OUTPUT" | grep -q "inspect ok" || fail "Functional: inspect command failed."
    pass "Functional: Inspect command verified."

    OUTPUT=$("$KC_BIN_EXEC" run "$TMP_FILE")
    printf '%s' "$OUTPUT" | grep -q "run queued" || fail "Functional: run command failed."
    pass "Functional: Run command verified."

    if "$KC_BIN_EXEC" inspect "$TMP_FILE.missing" >/dev/null 2>&1; then
        fail "Functional: missing file should fail."
    fi
    pass "Functional: Missing file fail-fast verified."

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
