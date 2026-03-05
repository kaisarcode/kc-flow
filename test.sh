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
    if ! "$KC_BIN_EXEC" --help | grep -q "Options:"; then
        fail "General: Help flag failed."
    fi
    pass "General: Help flag verified."

    if "$KC_BIN_EXEC" --unknown >/dev/null 2>&1; then
        fail "General: Unknown flag should fail."
    fi
    pass "General: Unknown flag fail-fast verified."
}

test_functional() {
    EXAMPLE_FILE="$APP_ROOT/etc/example.flow"

    OUTPUT=$("$KC_BIN_EXEC" --run "$EXAMPLE_FILE")
    printf '%s' "$OUTPUT" | grep -q "run ok" || fail "Functional: run command failed."
    printf '%s' "$OUTPUT" | grep -q "kind=contract" || fail "Functional: run kind detection failed."
    printf '%s' "$OUTPUT" | grep -q "output.result=hello" || fail "Functional: run output binding failed."
    pass "Functional: Run command verified."

    INPUT_FILE="$(mktemp)"
    trap 'rm -f "$INPUT_FILE"' RETURN
    cat > "$INPUT_FILE" <<'EOF'
contract.id=kc.example.input_echo
contract.name=Input Echo Example
input.1.id=user_text
input.1.type=text
input.1.required=1
output.1.id=result
output.1.type=text
runtime.script=printf "%s\n" "<input.user_text>"
runtime.workdir=.
bind.output.1.id=result
bind.output.1.mode=stdout
EOF

    if "$KC_BIN_EXEC" --run "$INPUT_FILE" >/dev/null 2>&1; then
        fail "Functional: run without required input override should fail."
    fi
    pass "Functional: Missing input override fail-fast verified."

    OUTPUT=$("$KC_BIN_EXEC" --run "$INPUT_FILE" --set input.user_text=hello)
    printf '%s' "$OUTPUT" | grep -q "run ok" || fail "Functional: run with --set failed."
    printf '%s' "$OUTPUT" | grep -q "output.result=hello" || fail "Functional: --set input override output failed."
    pass "Functional: Run with --set input override verified."
    rm -f "$INPUT_FILE"
    trap - RETURN

    if "$KC_BIN_EXEC" --run "$EXAMPLE_FILE.missing" >/dev/null 2>&1; then
        fail "Functional: missing file should fail."
    fi
    pass "Functional: Missing file fail-fast verified."

    TMP_FILE=$(mktemp)
    trap 'rm -f "$TMP_FILE"' RETURN
    printf 'contract.id=broken\n' > "$TMP_FILE"

    if "$KC_BIN_EXEC" --run "$TMP_FILE" >/dev/null 2>&1; then
        fail "Functional: invalid contract should fail."
    fi
    pass "Functional: Invalid contract validation verified."

    rm -f "$TMP_FILE"
    trap - RETURN

    FLOW_TMP_DIR="$(mktemp -d)"
    trap 'rm -rf "$FLOW_TMP_DIR"' RETURN

    cat > "$FLOW_TMP_DIR/leaf.flow" <<'EOF'
contract.id=kc.example.leaf
contract.name=Leaf
input.1.id=user_text
input.1.type=text
input.1.required=1
output.1.id=result
output.1.type=text
runtime.script=printf "%s\n" "<input.user_text>"
runtime.workdir=.
bind.output.1.id=result
bind.output.1.mode=stdout
EOF

    cat > "$FLOW_TMP_DIR/child.flow" <<'EOF'
flow.id=kc.example.child
flow.name=Child
input.1.id=user_text
input.1.type=text
output.1.id=result
output.1.type=text
node.1.id=leaf
node.1.contract=leaf.flow
link.1.from=input.user_text
link.1.to=node.leaf.in.user_text
link.2.from=node.leaf.out.result
link.2.to=output.result
EOF

    cat > "$FLOW_TMP_DIR/parent.flow" <<'EOF'
flow.id=kc.example.parent
flow.name=Parent
input.1.id=user_text
input.1.type=text
output.1.id=result
output.1.type=text
node.1.id=child
node.1.contract=child.flow
link.1.from=input.user_text
link.1.to=node.child.in.user_text
link.2.from=node.child.out.result
link.2.to=output.result
EOF

    if "$KC_BIN_EXEC" --run "$FLOW_TMP_DIR/parent.flow" >/dev/null 2>&1; then
        fail "Functional: flow run without required input should fail."
    fi
    pass "Functional: Flow missing input fail-fast verified."

    OUTPUT=$("$KC_BIN_EXEC" --run "$FLOW_TMP_DIR/parent.flow" --set input.user_text=hello)
    printf '%s' "$OUTPUT" | grep -q "kind=flow" || fail "Functional: flow kind detection failed."
    printf '%s' "$OUTPUT" | grep -q "output.result=hello" || fail "Functional: nested flow output propagation failed."
    pass "Functional: Nested flow execution and chaining verified."

    rm -rf "$FLOW_TMP_DIR"
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
