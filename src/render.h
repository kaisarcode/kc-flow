/**
 * render.h
 * Summary: CLI renderer/export interface for flow-to-terminal generation.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#ifndef KC_FLOW_RENDER_H
#define KC_FLOW_RENDER_H

#include <stddef.h>

int kc_flow_render_cli(
    const char *path,
    char *error,
    size_t error_size
);

#endif
