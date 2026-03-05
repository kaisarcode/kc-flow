/**
 * build.h
 * Summary: CLI build/export interface for flow-to-terminal generation.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#ifndef KC_FLOW_BUILD_H
#define KC_FLOW_BUILD_H

#include <stddef.h>

int kc_flow_build_cli(
    const char *path,
    const char *shell,
    char *error,
    size_t error_size
);

#endif
