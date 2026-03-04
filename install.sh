#!/bin/bash
# install.sh - Production installer for kc-stdio on Linux.
# Summary: Downloads and installs the kc-stdio binary for the host platform using wget.
#
# Author:  KaisarCode
# Website: https://kaisarcode.com
# License: GNU GPL v3.0

set -e

APP_ID="kc-stdio"
REPO_URL="https://raw.githubusercontent.com/kaisarcode/kc-stdio/master"
INSTALL_ROOT="/usr/local/bin"

fail() {
    printf "Error: %s\n" "$1" >&2
    exit 1
}

detect_arch() {
    case "$(uname -s)" in
        Linux) ;;
        *) fail "Unsupported OS: $(uname -s)" ;;
    esac

    case "$(uname -m)" in
        x86_64) TARGET_ARCH="x86_64" ;;
        aarch64|arm64) TARGET_ARCH="aarch64" ;;
        *) fail "Unsupported architecture: $(uname -m)" ;;
    esac
}

download_binary() {
    TMP_BIN=$(mktemp)
    URL="$REPO_URL/bin/$TARGET_ARCH/$APP_ID"
    wget -qO "$TMP_BIN" "$URL" || fail "Unable to download binary."
    chmod +x "$TMP_BIN"
}

install_binary() {
    sudo mkdir -p "$INSTALL_ROOT"
    sudo install -m 0755 "$TMP_BIN" "$INSTALL_ROOT/$APP_ID"
    rm -f "$TMP_BIN"
}

main() {
    command -v wget >/dev/null 2>&1 || fail "wget is required."
    command -v sudo >/dev/null 2>&1 || fail "sudo is required."
    detect_arch
    download_binary
    install_binary
    printf "%s installed.\n" "$APP_ID"
}

main "$@"
