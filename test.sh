#!/bin/bash
# test.sh - Automated test suite for kc-flow
# Summary: Tiered testing for KCS, ecosystem compliance, and runtime logic.
#
# Author:  KaisarCode
# Website: https://kaisarcode.com
# License: GNU GPL v3.0
set -e

SCRIPT_DIR=$(CDPATH='' cd -- "$(dirname "$0")" && pwd)
APP_ROOT="$SCRIPT_DIR"

# @brief Prints one test failure and exits.
# @param $1 Failure message.
# @return Does not return.
fail() {
    printf "\033[31m[FAIL]\033[0m %s\n" "$1"
    exit 1
}

# @brief Prints one passing test message.
# @param $1 Success message.
# @return 0 on success.
pass() {
    printf "\033[32m[PASS]\033[0m %s\n" "$1"
}

# @brief Prepares the test runtime environment.
# @return 0 on success.
test_setup() {
    ARCH=$(uname -m)
    [ "$ARCH" = "x86_64" ] || [ "$ARCH" = "aarch64" ] || ARCH="arm64-v8a"
    export KC_BIN_EXEC="$APP_ROOT/bin/$ARCH/kc-flow"

    [ -f "$KC_BIN_EXEC" ] || fail "Binary not found at $KC_BIN_EXEC."
    pass "Environment verified: using $KC_BIN_EXEC"
}

# @brief Runs KCS validation across the repository.
# @return 0 on success.
test_kcs() {
    if command -v kcs >/dev/null 2>&1; then
        find "$APP_ROOT" -type f -not -path '*/.*' -not -path '*/bin/*' \
            -exec kcs {} + || fail "KCS validation failed."
        pass "General: KCS compliance verified."
    fi
}

# @brief Runs general CLI behavior checks.
# @return 0 on success.
test_general() {
    "$KC_BIN_EXEC" --help | grep -q "Options:" || fail "General: Help flag failed."
    pass "General: Help flag verified."

    if "$KC_BIN_EXEC" --unknown >/dev/null 2>&1; then
        fail "General: Unknown flag should fail."
    fi
    pass "General: Unknown flag fail-fast verified."

    if "$KC_BIN_EXEC" --run "$APP_ROOT/etc/example.flow" --workers 0 >/dev/null 2>&1; then
        fail "General: Invalid workers value should fail."
    fi
    pass "General: Invalid workers fail-fast verified."

    if "$KC_BIN_EXEC" --run "$APP_ROOT/etc/example.flow" --fd-in bad >/dev/null 2>&1; then
        fail "General: Invalid fd value should fail."
    fi
    pass "General: Invalid fd fail-fast verified."
}

# @brief Runs functional flow and contract behavior tests.
# @return 0 on success.
test_functional() {
    EXAMPLE_FILE="$APP_ROOT/etc/example.flow"
    OUTPUT=$("$KC_BIN_EXEC" --run "$EXAMPLE_FILE")
    [ "$OUTPUT" = "hello" ] || fail "Functional: example contract output mismatch."
    pass "Functional: direct contract output verified."

    INPUT_FILE=$(mktemp)
    trap 'rm -f "$INPUT_FILE"' RETURN
    cat > "$INPUT_FILE" <<'EOF'
contract.id=kc.example.decorate
contract.name=Decorate
input.1.id=raw
input.1.type=stream
param.1.id=prefix
param.1.type=text
param.1.default=hello:
output.1.id=raw
output.1.type=stream
runtime.script=printf "%s" "<param.prefix>"; cat
runtime.workdir=.
EOF

    OUTPUT=$(printf 'world' | "$KC_BIN_EXEC" --run "$INPUT_FILE")
    [ "$OUTPUT" = "hello:world" ] || fail "Functional: stdin contract transport failed."
    pass "Functional: stdin contract transport verified."

    OUTPUT=$(printf 'world' | "$KC_BIN_EXEC" --run "$INPUT_FILE" --set param.prefix=kc:)
    [ "$OUTPUT" = "kc:world" ] || fail "Functional: parameter override failed."
    pass "Functional: parameter override verified."

    FD_OUT_FILE=$(mktemp)
    trap 'rm -f "$INPUT_FILE" "$FD_OUT_FILE"' RETURN
    exec 3< <(printf 'socket')
    exec 4> "$FD_OUT_FILE"
    "$KC_BIN_EXEC" --run "$INPUT_FILE" --set param.prefix=fd: --fd-in 3 --fd-out 4
    exec 3<&-
    exec 4>&-
    OUTPUT=$(cat "$FD_OUT_FILE")
    [ "$OUTPUT" = "fd:socket" ] || fail "Functional: fd-in/fd-out runtime failed."
    pass "Functional: fd-in/fd-out runtime verified."

    rm -f "$INPUT_FILE" "$FD_OUT_FILE"
    trap - RETURN

    FLOW_TMP_DIR=$(mktemp -d)
    trap 'rm -rf "$FLOW_TMP_DIR"' RETURN

    cat > "$FLOW_TMP_DIR/leaf.flow" <<'EOF'
contract.id=kc.example.leaf
contract.name=Leaf
input.1.id=raw
input.1.type=stream
output.1.id=raw
output.1.type=stream
runtime.script=cat
runtime.workdir=.
EOF

    cat > "$FLOW_TMP_DIR/child.flow" <<'EOF'
flow.id=kc.example.child
flow.name=Child
input.1.id=raw
input.1.type=stream
output.1.id=raw
output.1.type=stream
node.1.id=leaf
node.1.contract=leaf.flow
link.1.from=input.raw
link.1.to=node.leaf.in.raw
link.2.from=node.leaf.out.raw
link.2.to=output.raw
EOF

    cat > "$FLOW_TMP_DIR/parent.flow" <<'EOF'
flow.id=kc.example.parent
flow.name=Parent
input.1.id=raw
input.1.type=stream
output.1.id=raw
output.1.type=stream
node.1.id=child
node.1.contract=child.flow
link.1.from=input.raw
link.1.to=node.child.in.raw
link.2.from=node.child.out.raw
link.2.to=output.raw
EOF

    OUTPUT=$(printf 'hello' | "$KC_BIN_EXEC" --run "$FLOW_TMP_DIR/parent.flow")
    [ "$OUTPUT" = "hello" ] || fail "Functional: nested flow transport failed."
    pass "Functional: nested flow transport verified."

    OUTPUT=$(printf 'hello' | "$KC_BIN_EXEC" --run "$FLOW_TMP_DIR/parent.flow" --workers 2)
    [ "$OUTPUT" = "hello" ] || fail "Functional: workers runtime failed."
    pass "Functional: workers runtime verified."

    cat > "$FLOW_TMP_DIR/cycle.flow" <<'EOF'
flow.id=kc.example.cycle
flow.name=Cycle
node.1.id=left
node.1.contract=leaf.flow
node.2.id=right
node.2.contract=leaf.flow
link.1.from=node.left.out.raw
link.1.to=node.right.in.raw
link.2.from=node.right.out.raw
link.2.to=node.left.in.raw
EOF

    if printf 'hello' | "$KC_BIN_EXEC" --run "$FLOW_TMP_DIR/cycle.flow" >/dev/null 2>&1; then
        fail "Functional: cycle validation should fail."
    fi
    pass "Functional: cycle validation verified."

    rm -rf "$FLOW_TMP_DIR"
    trap - RETURN
}

# @brief Runs the full test suite.
# @return 0 on success.
run_tests() {
    test_setup
    test_kcs
    test_general
    test_functional
    pass "All tests passed successfully."
}

run_tests
