#!/bin/bash
# test.sh - Automated test suite for kc-flow
# Summary: Focused validation for the branch-oriented flow runtime.
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
    export KC_FLOW_BIN="$KC_BIN_EXEC"

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

    if "$KC_BIN_EXEC" --run "$APP_ROOT/etc/parent.flow" --workers 0 >/dev/null 2>&1; then
        fail "General: Invalid workers value should fail."
    fi
    pass "General: Invalid workers fail-fast verified."

    if "$KC_BIN_EXEC" --run "$APP_ROOT/etc/parent.flow" --fd-in bad >/dev/null 2>&1; then
        fail "General: Invalid fd value should fail."
    fi
    pass "General: Invalid fd fail-fast verified."
}

# @brief Runs functional flow behavior tests.
# @return 0 on success.
test_functional() {
    OUTPUT=$("$KC_BIN_EXEC" --run "$APP_ROOT/etc/parent.flow")
    [ "$OUTPUT" = "Hello WorldHola Mundo" ] || fail "Functional: default parent flow output mismatch."
    pass "Functional: default parent flow output verified."

    OUTPUT=$("$KC_BIN_EXEC" --run "$APP_ROOT/etc/parent.flow" --set flow.param.hello=Salut)
    [ "$OUTPUT" = "Salut WorldHola Mundo" ] || fail "Functional: flow override should feed child branch."
    pass "Functional: flow override propagation verified."

    CHILD_OUTPUT=$("$KC_BIN_EXEC" --run "$APP_ROOT/etc/child.flow" --set flow.param.world=Planet)
    [ "$CHILD_OUTPUT" = "Hello Planet" ] || fail "Functional: direct child flow output mismatch."
    pass "Functional: direct child flow output verified."

    TMP_DIR=$(mktemp -d)
    trap 'rm -rf "$TMP_DIR"' RETURN

    cat > "$TMP_DIR/fanout.flow" <<'EOF'
flow.id=fanout
flow.param.greeting=Hi
flow.link=root

node.root.param.greeting=<flow.param.greeting>
node.root.link=left
node.root.link=right

node.left.exec=printf "%s" "<flow.param.greeting> Left"
node.right.exec=printf "%s" "<flow.param.greeting> Right"
EOF

    OUTPUT=$("$KC_BIN_EXEC" --run "$TMP_DIR/fanout.flow")
    [ "$OUTPUT" = "Hi LeftHi Right" ] || fail "Functional: branch fan-out output mismatch."
    pass "Functional: branch fan-out verified."

    cat > "$TMP_DIR/cycle.flow" <<'EOF'
flow.id=cycle
flow.link=left

node.left.link=right
node.right.link=left
EOF

    if "$KC_BIN_EXEC" --run "$TMP_DIR/cycle.flow" >/dev/null 2>&1; then
        fail "Functional: cycle validation should fail."
    fi
    pass "Functional: cycle validation verified."

    rm -rf "$TMP_DIR"
    trap - RETURN
}

test_setup
test_kcs
test_general
test_functional
pass "All tests passed successfully."
